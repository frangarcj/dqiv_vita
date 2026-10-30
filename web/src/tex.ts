// Texture2D -> DQTX blob (port of dqivconv/tex.py, default options: RGB24->RGB565, RGBA32->RGBA8888).
// Pixel rows stay in Unity/GL order (row 0 = bottom row).
import * as B from "./blob";

const TF_A8 = 1, TF_RGB565 = 2, TF_RGBA4444 = 3, TF_RGBA8888 = 4;
const TFL_NPOT = 1, TFL_HAS_ALPHA = 2, TFL_HAD_MIPS = 4, TFL_NOT_RESIZED = 8;
// Unity TextureFormat ids
const UF = { Alpha8: 1, ARGB4444: 2, RGB24: 3, RGBA32: 4, ARGB32: 5, RGB565: 7, DXT1: 10, DXT5: 12, RGBA4444: 13, BGRA32: 14, ETC_RGB4: 34 };

function rgb565(rgb: Uint8Array, stride: number, n: number): Uint8Array {
  const out = new Uint8Array(n * 2);
  for (let i = 0, p = 0; i < n; i++, p += stride) {
    const v = (((rgb[p] * 31 + 127) / 255 | 0) << 11) | (((rgb[p + 1] * 63 + 127) / 255 | 0) << 5) | ((rgb[p + 2] * 31 + 127) / 255 | 0);
    out[2 * i] = v & 255; out[2 * i + 1] = v >> 8;
  }
  return out;
}

// ---- ETC1 (port of texture2ddecoder's decode_etc1; output is RGB, 3 bytes per pixel)
const ETC1_MOD = [[2, 8], [5, 17], [9, 29], [13, 42], [18, 60], [24, 80], [33, 106], [47, 183]];
const ETC1_SUB = [[0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1], [0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1]];   // by flip bit; pixels are column-major
const WRITE_ORDER = [0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15];
const clamp = (n: number) => (n < 0 ? 0 : n > 255 ? 255 : n);

export function decodeEtc1(d: Uint8Array, w: number, h: number): Uint8Array {
  const out = new Uint8Array(w * h * 3);
  const bw = (w + 3) >> 2, bh = (h + 3) >> 2;
  const c = [[0, 0, 0], [0, 0, 0]];
  const buf = new Uint8Array(48);
  let p = 0;
  for (let by = 0; by < bh; by++) {
    for (let bx = 0; bx < bw; bx++, p += 8) {
      const code = [d[p + 3] >> 5, (d[p + 3] >> 2) & 7];
      const table = ETC1_SUB[d[p + 3] & 1];
      if (d[p + 3] & 2) {          // differential
        for (let i = 0; i < 3; i++) {
          let a = d[p + i] & 0xf8;
          let b = (a + ((d[p + i] << 3) & 0x18) - ((d[p + i] << 3) & 0x20)) & 0xff;
          a |= a >> 5; b |= b >> 5;
          c[0][i] = a; c[1][i] = b;
        }
      } else {
        for (let i = 0; i < 3; i++) { c[0][i] = (d[p + i] & 0xf0) | (d[p + i] >> 4); c[1][i] = (d[p + i] & 0x0f) | ((d[p + i] << 4) & 0xff); }
      }
      let j = (d[p + 6] << 8) | d[p + 7], k = (d[p + 4] << 8) | d[p + 5];
      for (let i = 0; i < 16; i++, j >>= 1, k >>= 1) {
        const t = table[i];
        const m = ETC1_MOD[code[t]][j & 1] * (k & 1 ? -1 : 1);
        const o = WRITE_ORDER[i] * 3;
        buf[o] = clamp(c[t][0] + m); buf[o + 1] = clamp(c[t][1] + m); buf[o + 2] = clamp(c[t][2] + m);
      }
      const cw = Math.min(4, w - bx * 4), ch = Math.min(4, h - by * 4);
      for (let y = 0; y < ch; y++) out.set(buf.subarray(y * 12, y * 12 + cw * 3), ((by * 4 + y) * w + bx * 4) * 3);
    }
  }
  return out;
}

export function convertTexture(m: any): { blob: Uint8Array; aux: number; info: { fmt: number; out: number; w: number; h: number; alpha: boolean } } {
  const fmt: number = m.m_TextureFormat, w: number = m.m_Width, h: number = m.m_Height;
  const data: Uint8Array = m["image data"];
  const n = w * h;
  let hasAlpha = false, outFmt: number, px: Uint8Array;
  const need = (bytes: number) => { if (data.length < bytes) throw new Error(`texture data too short (${data.length} < ${bytes})`); };
  if (fmt === UF.Alpha8) { need(n); outFmt = TF_A8; px = data.subarray(0, n); hasAlpha = true; }
  else if (fmt === UF.RGB24) { need(n * 3); outFmt = TF_RGB565; px = rgb565(data, 3, n); }
  else if (fmt === UF.RGBA32 || fmt === UF.ARGB32 || fmt === UF.BGRA32) {
    need(n * 4);
    outFmt = TF_RGBA8888;
    if (fmt === UF.RGBA32) px = data.subarray(0, n * 4);
    else {
      px = new Uint8Array(n * 4);
      const s = fmt === UF.ARGB32 ? [1, 2, 3, 0] : [2, 1, 0, 3];
      for (let i = 0; i < n * 4; i += 4) { px[i] = data[i + s[0]]; px[i + 1] = data[i + s[1]]; px[i + 2] = data[i + s[2]]; px[i + 3] = data[i + s[3]]; }
    }
    for (let i = 3; i < n * 4; i += 4) if (px[i] !== 255) { hasAlpha = true; break; }
  }
  else if (fmt === UF.RGB565) { need(n * 2); outFmt = TF_RGB565; px = data.subarray(0, n * 2); }
  else if (fmt === UF.RGBA4444) {
    need(n * 2); outFmt = TF_RGBA4444; px = data.subarray(0, n * 2);
    for (let i = 0; i < n * 2; i += 2) if ((px[i] & 0xf) !== 0xf) { hasAlpha = true; break; }
  }
  else if (fmt === UF.ARGB4444) {
    need(n * 2); outFmt = TF_RGBA4444; px = new Uint8Array(n * 2);
    for (let i = 0; i < n * 2; i += 2) {
      const v = data[i] | data[i + 1] << 8, r = (((v & 0x0fff) << 4) | (v >> 12)) & 0xffff;
      px[i] = r & 255; px[i + 1] = r >> 8;
      if ((r & 0xf) !== 0xf) hasAlpha = true;
    }
  }
  else if (fmt === UF.ETC_RGB4) { outFmt = TF_RGB565; px = rgb565(decodeEtc1(data, w, h), 3, n); }
  else throw new Error(`unsupported texture format ${fmt}`);

  const ts = m.m_TextureSettings ?? {};
  let flags = 0;
  if ((w & (w - 1)) || (h & (h - 1))) flags |= TFL_NPOT;
  if (hasAlpha) flags |= TFL_HAS_ALPHA;
  if (m.m_MipMap) flags |= TFL_HAD_MIPS;
  flags |= TFL_NOT_RESIZED;
  const wr = new B.W(48 + px.length + 64);
  wr.u32(B.MAGIC[B.K_TEX]); wr.u32(0);
  wr.u32(outFmt);
  wr.u16(w); wr.u16(h); wr.u16(w); wr.u16(h);
  wr.u32(flags);
  wr.u8(ts.m_WrapMode ?? 0); wr.u8(ts.m_FilterMode ?? 1); wr.u8(Math.max(1, ts.m_Aniso ?? 1)); wr.u8(fmt);
  wr.u32(48); wr.u32(px.length); wr.u32(1); wr.f32(ts.m_MipBias ?? 0); wr.u32(0);
  if (wr.length !== 48) throw new Error("texture header size");
  wr.raw(px);
  return { blob: wr.finish(), aux: ((w << 16) | h) >>> 0, info: { fmt, out: outFmt, w, h, alpha: hasAlpha } };
}

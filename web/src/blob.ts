// Little-endian blob writer shared by all converters (port of dqivconv/blob.py).
import { encodeUtf8Replace } from "./util";

export const K_TEX = 1, K_MESH = 2, K_MAT = 3, K_SHADER = 4, K_PREFAB = 5, K_ANIM = 6, K_TEXT = 7, K_FONT = 8;
export const KIND_NAMES: Record<number, string> = { 1: "Texture", 2: "Mesh", 3: "Material", 4: "Shader", 5: "Prefab", 6: "AnimClip", 7: "TextAsset", 8: "Font" };
export const DF_META = 0, DF_MESH = 1, DF_TEX = 2, DF_BLOB = 3;
export const DF_NAMES = ["meta.pak", "mesh.pak", "tex.pak", "blob.pak"];
export const KIND_DF: Record<number, number> = { 1: DF_TEX, 2: DF_MESH, 3: DF_META, 4: DF_META, 5: DF_META, 6: DF_META, 7: DF_BLOB, 8: DF_BLOB };

const fourcc = (s: string) => s.charCodeAt(0) | s.charCodeAt(1) << 8 | s.charCodeAt(2) << 16 | s.charCodeAt(3) << 24;
export const MAGIC: Record<number, number> = {
  1: fourcc("DQTX"), 2: fourcc("DQMS"), 3: fourcc("DQMT"), 4: fourcc("DQSH"), 5: fourcc("DQPF"), 6: fourcc("DQAN"), 7: fourcc("DQTA"), 8: fourcc("DQFN"),
};
export const BUILTIN_BIT = 0x80000000;   // ref to a Unity built-in resource: BUILTIN_BIT | pathID (not in the pack)

/** Append-only little-endian writer with a deferred, de-duplicated string table. */
export class W {
  b: Uint8Array;
  dv: DataView;
  n = 0;
  private patches: [number, string][] = [];
  /** `capacity`: expected final size (avoids the doubling copies for the multi-MB texture and mesh blobs). */
  constructor(capacity = 256) { this.b = new Uint8Array(Math.max(256, capacity)); this.dv = new DataView(this.b.buffer); }

  get length() { return this.n; }
  private grow(extra: number) {
    if (this.n + extra <= this.b.length) return;
    let c = this.b.length * 2;
    while (c < this.n + extra) c *= 2;
    const nb = new Uint8Array(c); nb.set(this.b.subarray(0, this.n)); this.b = nb; this.dv = new DataView(nb.buffer);
  }
  u8(v: number) { this.grow(1); this.b[this.n++] = v & 0xff; }
  u16(v: number) { this.grow(2); this.dv.setUint16(this.n, v & 0xffff, true); this.n += 2; }
  u32(v: number) { this.grow(4); this.dv.setUint32(this.n, v >>> 0, true); this.n += 4; }
  i32(v: number) { this.grow(4); this.dv.setInt32(this.n, v, true); this.n += 4; }
  i16(v: number) { this.grow(2); this.dv.setInt16(this.n, v, true); this.n += 2; }
  f32(...v: number[]) { this.grow(4 * v.length); for (const x of v) { this.dv.setFloat32(this.n, x, true); this.n += 4; } }
  f32a(v: ArrayLike<number>) { this.grow(4 * v.length); for (let i = 0; i < v.length; i++) { this.dv.setFloat32(this.n, v[i], true); this.n += 4; } }
  raw(d: Uint8Array) { this.grow(d.length); this.b.set(d, this.n); this.n += d.length; }
  zeros(n: number) { this.grow(n); this.b.fill(0, this.n, this.n + n); this.n += n; }
  align(a: number) { const r = this.n % a; if (r) this.zeros(a - r); }
  /** u32 placeholder -> offset of the NUL-terminated utf-8 string in the blob's string table (0 = empty). */
  str(s: string | null | undefined) {
    if (s == null || s === "") { this.u32(0); return; }
    this.patches.push([this.n, s]);
    this.u32(0);
  }
  setU32(pos: number, v: number) { this.dv.setUint32(pos, v >>> 0, true); }

  finish(): Uint8Array {
    this.align(4);
    const table = new Map<string, number>();
    for (const [pos, s] of this.patches) {
      let off = table.get(s);
      if (off === undefined) {
        off = this.n;
        const e = encodeUtf8Replace(s);
        this.grow(e.length + 1); this.b.set(e, this.n); this.b[this.n + e.length] = 0; this.n += e.length + 1;
        table.set(s, off);
      }
      this.dv.setUint32(pos, off, true);
    }
    this.align(16);
    this.dv.setUint32(4, this.n, true);
    // a nearly full buffer is returned as a view (no copy); otherwise trim it
    return this.b.length - this.n > (this.n >> 3) + 4096 ? this.b.slice(0, this.n) : this.b.subarray(0, this.n);
  }
}

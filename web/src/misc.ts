// TextAsset -> DQTA, Font -> DQFN, AnimationClip -> DQAN (port of dqivconv/misc.py).
import * as B from "./blob";
import type { Resolver } from "./registry";

const lossy = new TextDecoder("utf-8");

/** Raw TextAsset object bytes -> (name, payload). Layout: str m_Name, bytes m_Script, str m_PathName. */
export function convertText(raw: Uint8Array) {
  const dv = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
  const n = dv.getInt32(0, true);
  const name = lossy.decode(raw.subarray(4, 4 + n));
  const p = (4 + n + 3) & ~3;
  const ln = dv.getInt32(p, true);
  const data = raw.subarray(p + 4, p + 4 + ln);
  const w = new B.W();
  w.u32(B.MAGIC[B.K_TEXT]); w.u32(0); w.u32(data.length); w.u32(0);
  w.raw(data);
  return { blob: w.finish(), aux: data.length, name };
}

export function convertFont(m: any, res: Resolver) {
  const rects: any[] = m.m_CharacterRects || [];
  const kern: any[] = m.m_KerningValues || [];
  const ttf: Uint8Array = m.m_FontData || new Uint8Array(0);
  const names: string[] = m.m_FontNames || [];
  const w = new B.W();
  w.u32(B.MAGIC[B.K_FONT]); w.u32(0);
  w.f32(m.m_FontSize, m.m_LineSpacing, m.m_Ascent, m.m_Kerning, m.m_PixelScale);
  w.i32(m.m_CharacterSpacing); w.i32(m.m_CharacterPadding); w.i32(m.m_ConvertCase);
  w.i32(m.m_AsciiStartOffset); w.i32(m.m_DefaultStyle); w.i32(m.m_FontRenderingMode);
  w.u32(res.ref(m.m_Texture, B.K_TEX)); w.u32(res.ref(m.m_DefaultMaterial, B.K_MAT));
  w.u32(rects.length); w.u32(0);
  w.u32(kern.length); w.u32(0);
  w.u32(ttf.length); w.u32(0);
  w.str(names.length ? names[0] : "");
  w.u32((m.m_FallbackFonts || []).length);
  w.u32(0);
  if (w.length !== 96) throw new Error("font header size " + w.length);
  w.align(4); w.setU32(64, w.length);
  for (const r of rects) {
    const uv = r.uv, vt = r.vert;
    w.u32(r.index); w.f32(uv.x, uv.y, uv.width, uv.height, vt.x, vt.y, vt.width, vt.height, r.width);
    w.u32(r.flipped ? 1 : 0);
  }
  w.align(4); w.setU32(72, w.length);
  for (const k of kern) { const [a, b] = k[0]; w.u16(a); w.u16(b); w.f32(k[1]); }
  w.align(16); w.setU32(80, w.length);
  w.raw(ttf);
  return { blob: w.finish(), aux: ttf.length, info: { rects: rects.length, ttf: ttf.length } };
}

// ------------------------------------------------------------------ animation
const CT_POS = 0, CT_ROT = 1, CT_SCALE = 2, CT_FLOAT = 3;
const NKEY: Record<number, number> = { 0: 10, 1: 13, 2: 10, 3: 4 };
const xyz = (v: any) => [v.x, v.y, v.z];
const xyzw = (v: any) => [v.x, v.y, v.z, v.w];

interface Curve { path: string; attr: string; type: number; cid: number; keys: number[]; n: number; pre: number; post: number; }

export function convertAnim(m: any) {
  const curves: Curve[] = [];
  const add = (lst: any[] | undefined, type: number, getv: (v: any) => number[]) => {
    for (const c of lst || []) {
      const cv = c.curve, keys: number[] = [];
      for (const k of cv.m_Curve) keys.push(k.time, ...getv(k.value), ...getv(k.inSlope), ...getv(k.outSlope));
      curves.push({ path: c.path, attr: "", type, cid: 0, keys, n: cv.m_Curve.length, pre: cv.m_PreInfinity ?? 2, post: cv.m_PostInfinity ?? 2 });
    }
  };
  add(m.m_PositionCurves, CT_POS, xyz);
  add(m.m_RotationCurves, CT_ROT, xyzw);
  add(m.m_ScaleCurves, CT_SCALE, xyz);
  for (const c of m.m_FloatCurves || []) {
    const cv = c.curve, keys: number[] = [];
    for (const k of cv.m_Curve) keys.push(k.time, k.value, k.inSlope, k.outSlope);
    curves.push({ path: c.path, attr: c.attribute ?? "", type: CT_FLOAT, cid: c.classID ?? 0, keys, n: cv.m_Curve.length, pre: cv.m_PreInfinity ?? 2, post: cv.m_PostInfinity ?? 2 });
  }
  if ((m.m_CompressedRotationCurves || []).length) throw new Error("compressed rotation curves not supported");
  const events: any[] = m.m_Events || [];
  let duration = 0;
  for (const c of curves) if (c.n) duration = Math.max(duration, c.keys[(c.n - 1) * NKEY[c.type]]);
  const w = new B.W();
  w.u32(B.MAGIC[B.K_ANIM]); w.u32(0);
  w.f32(m.m_SampleRate ?? 60, duration);
  w.i32(m.m_WrapMode ?? 0); w.u32(curves.length);
  w.u32(0); w.u32(events.length); w.u32(0);
  const bd = m.m_Bounds || { m_Center: { x: 0, y: 0, z: 0 }, m_Extent: { x: 0, y: 0, z: 0 } };
  w.f32(...xyz(bd.m_Center), ...xyz(bd.m_Extent));
  w.u32(0);
  if (w.length !== 64) throw new Error("anim header size " + w.length);
  w.align(4); w.setU32(24, w.length);
  const keyoff: number[] = [];
  for (const c of curves) {
    w.str(c.path); w.str(c.attr); w.u32(c.type); w.u32(c.cid); w.u32(c.n);
    keyoff.push(w.length); w.u32(0);
    w.i32(c.pre); w.i32(c.post);
  }
  w.align(4); w.setU32(32, w.length);
  for (const e of events) {
    w.f32(e.time); w.str(e.functionName ?? ""); w.str(e.data ?? "");
    w.f32(e.floatParameter ?? 0); w.i32(e.intParameter ?? 0); w.u32(0);
  }
  curves.forEach((c, i) => { w.align(4); w.setU32(keyoff[i], w.length); w.f32a(c.keys); });
  return { blob: w.finish(), aux: curves.length, info: { curves: curves.length } };
}

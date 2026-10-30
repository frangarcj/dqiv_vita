// Material -> DQMT blob, Shader -> DQSH blob (port of dqivconv/material.py).
import * as B from "./blob";
import type { Resolver, Registry } from "./registry";


const rec = (o: Record<string, number>) => Object.assign(Object.create(null) as Record<string, number>, o);   // no inherited keys
const QUEUES = rec({ background: 1000, geometry: 2000, alphatest: 2450, transparent: 3000, overlay: 4000 });
const CMP = rec({ never: 0, less: 1, equal: 2, lequal: 3, greater: 4, notequal: 5, gequal: 6, always: 7 });
const BLEND = rec({
  zero: 0, one: 1, srccolor: 2, srcalpha: 3, dstcolor: 4, dstalpha: 5, oneminussrccolor: 6, oneminussrcalpha: 7, oneminusdstcolor: 8, oneminusdstalpha: 9, srcalphasaturate: 10,
});
const CULL = rec({ off: 0, front: 1, back: 2 });
const CMASK = rec({ R: 1, G: 2, B: 4, A: 8 });

export function convertMaterial(m: any, res: Resolver, reg: Registry) {
  const sp = m.m_SavedProperties;
  const shader = res.ref(m.m_Shader, B.K_SHADER);
  const builtin = (shader & B.BUILTIN_BIT) !== 0;
  let sname = reg.shaderNames.get(shader) ?? "";
  let bname = "";
  if (builtin) {
    const [fn, pid] = res.target(m.m_Shader);
    bname = `builtin:${fn}:${pid}`;
    sname = bname;
  }
  const texs: [string, any][] = sp.m_TexEnvs.map(([n, v]: [any, any]) => [n.name, v]);
  const floats: [string, number][] = sp.m_Floats.map(([n, v]: [any, number]) => [typeof n === "object" ? n.name : n, v]);
  const cols: [string, any][] = sp.m_Colors.map(([n, v]: [any, any]) => [n.name, v]);
  let kws: any = m.m_ShaderKeywords || [];
  if (typeof kws === "string") kws = kws.split(/\s+/).filter(Boolean);
  let mainTex = 0;
  const w = new B.W();
  w.u32(B.MAGIC[B.K_MAT]); w.u32(0);
  w.u32(builtin ? 0 : shader);
  w.i32(m.m_CustomRenderQueue ?? -1);
  w.str(sname);
  const base = w.length;
  for (let i = 0; i < 8; i++) w.u32(0);
  w.u32(0);
  w.u32(0); w.u32(0);
  if (w.length !== 64) throw new Error("material header size " + w.length);
  w.align(4); w.setU32(base + 0, texs.length); w.setU32(base + 4, w.length);
  for (const [name, v] of texs) {
    const tid = res.ref(v.m_Texture, B.K_TEX);
    if (name === "_MainTex") mainTex = tid;
    w.str(name); w.u32(tid); w.f32(v.m_Scale.x, v.m_Scale.y, v.m_Offset.x, v.m_Offset.y);
  }
  w.setU32(base + 32, mainTex);
  w.align(4); w.setU32(base + 8, floats.length); w.setU32(base + 12, w.length);
  for (const [name, v] of floats) { w.str(name); w.f32(v); }
  w.align(4); w.setU32(base + 16, cols.length); w.setU32(base + 20, w.length);
  for (const [name, v] of cols) { w.str(name); w.f32(v.r, v.g, v.b, v.a); }
  w.align(4); w.setU32(base + 24, kws.length); w.setU32(base + 28, w.length);
  for (const k of kws) w.str(k);
  return { blob: w.finish(), aux: 0, info: { shader: sname || bname || "?" } };
}

// ---------------------------------------------------------------- shaders
/** s[i] == '{': index after the matching '}' (quote aware). */
function matchBlock(s: string, i: number): number {
  let depth = 0, inq = false;
  for (; i < s.length; i++) {
    const c = s[i];
    if (c === '"') inq = !inq;
    else if (!inq) {
      if (c === "{") depth++;
      else if (c === "}" && --depth === 0) return i + 1;
    }
  }
  return s.length;
}

export function shaderDeclaredName(text: string, fallback: string): string {
  const mt = /Shader\s+"([^"]+)"/.exec(text);
  return mt ? mt[1] : fallback;
}

interface PassState { zwrite?: number; ztest?: number; cull?: number; blend?: [number, number]; atest?: [number, number]; cmask?: number; offset?: [number, number]; }

// Python's float(): decimal literal only
const num = (s: string) => {
  if (!/^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$/.test(s)) throw new RangeError("bad float");
  return Number(s);
};
const need = <T>(v: T | undefined): T => { if (v === undefined) throw new RangeError("lookup"); return v; };
const pyWs = (s: string) => s.trim().split(/\s+/).filter(x => x !== "");

function parseState(txt: string, st: PassState) {
  const t = txt.replace(/\/\/[^\n]*/g, "");
  for (const mt of t.matchAll(/(?<![^\n])\s*(ZWrite|ZTest|Cull|Blend|AlphaTest|ColorMask|Offset)\b([^\n]*)/g)) {
    const k = mt[1], a = pyWs(mt[2].trim().replace(/"/g, ""));
    try {
      if (k === "ZWrite") st.zwrite = need(a[0]).toLowerCase() === "on" ? 1 : 0;
      else if (k === "ZTest") st.ztest = CMP[need(a[0]).toLowerCase()] ?? -1;
      else if (k === "Cull") st.cull = CULL[need(a[0]).toLowerCase()] ?? -1;
      else if (k === "Blend") {
        if (need(a[0]).toLowerCase() === "off") st.blend = [-1, -1];
        else st.blend = [need(BLEND[a[0].toLowerCase()]), need(BLEND[need(a[1]).toLowerCase().replace(/,+$/, "")])];
      } else if (k === "AlphaTest") {
        if (need(a[0]).toLowerCase() === "off") st.atest = [-1, 0];
        else st.atest = [need(CMP[a[0].toLowerCase()]), a.length > 1 && !a[1].startsWith("[") ? num(a[1]) : 0];
      } else if (k === "ColorMask") {
        const mk = need(a[0]);
        st.cmask = mk === "0" ? 0 : [...mk].reduce((s, c) => s + (CMASK[c] ?? 0), 0);
      } else if (k === "Offset") st.offset = [num(need(a[0]).replace(/,+$/, "")), num(need(a[1]))];
    } catch (e) {
      if (!(e instanceof RangeError)) throw e;
    }
  }
}

interface ShaderInfo { name: string; queue: number; fallback: string; props: [string, string, string][]; passes: PassState[]; gles: number; }

export function parseShader(text: string): ShaderInfo {
  const info: ShaderInfo = { name: shaderDeclaredName(text, ""), queue: -1, fallback: "", props: [], passes: [], gles: 0 };
  const pm = /Properties\s*\{/.exec(text);
  if (pm) {
    const end = pm.index + pm[0].length;
    const e = matchBlock(text, end - 1);
    const re = /(?<![^\n])\s*(?:\[[^\]]*\]\s*)*(\w+)\s*\(\s*"([^"]*)"\s*,\s*([^)]*?)\)\s*=\s*([^\n]*)(?![^\n])/g;
    for (const mt of text.slice(end, e).matchAll(re)) info.props.push([mt[1], mt[3].trim(), mt[4].trim()]);
  }
  const fm = /Fallback\s+"([^"]*)"/.exec(text);
  if (fm) info.fallback = fm[1];
  const sm = /SubShader\s*\{/.exec(text);
  if (sm) {
    const send = sm.index + sm[0].length;
    const e = matchBlock(text, send - 1);
    const sub = text.slice(send, e);
    const pi = /\bPass\s*\{/.exec(sub);
    const head = pi ? sub.slice(0, pi.index) : sub;
    const tq = /"QUEUE"\s*=\s*"(\w+)\s*([+-]\s*\d+)?"/i.exec(head);
    if (tq) info.queue = (QUEUES[tq[1].toLowerCase()] ?? 2000) + (tq[2] ? parseInt(tq[2].replace(/ /g, ""), 10) : 0);
    const base: PassState = {};
    parseState(head, base);
    let pos = 0;
    for (const pm2 of sub.matchAll(/\bPass\s*\{/g)) {
      if (pm2.index! < pos) continue;
      const pe = matchBlock(sub, pm2.index! + pm2[0].length - 1);
      pos = pe;
      const body = sub.slice(pm2.index! + pm2[0].length, pe);
      const progs = /Program\s+"/.exec(body);
      const st = { ...base };
      parseState(progs ? body.slice(0, progs.index) : body, st);
      info.passes.push(st);
    }
  }
  for (const mt of text.matchAll(/SubProgram\s+"gles\s*"\s*\{\s*(?:Keywords\s*\{([^}]*)\}\s*)?"!!GLES(.*?)"\s*\}/gs)) {
    // variants without GLSL (the "fp" stub that repeats the "vp" source) are not counted
    if (mt[2].includes("#ifdef VERTEX") || mt[2].includes("#ifdef FRAGMENT")) info.gles++;
  }
  return info;
}

export const safeName = (n: string) => n.replace(/[^A-Za-z0-9_.-]+/g, "_");

export function convertShader(m: any) {
  let text: any = m.m_Script || "";
  if (text instanceof Uint8Array) text = new TextDecoder().decode(text);
  const info = parseShader(text);
  const name = info.name || m.m_Name;
  const dump = safeName(name) + ".txt";
  const w = new B.W();
  w.u32(B.MAGIC[B.K_SHADER]); w.u32(0);
  w.str(name); w.i32(info.queue); w.str(dump); w.str(info.fallback);
  w.u32(info.passes.length); w.u32(info.props.length); w.u32(info.gles);
  const po = w.length; w.u32(0); w.u32(0);
  w.u32(0);
  if (w.length !== 48) throw new Error("shader header size " + w.length);
  w.align(4); w.setU32(po, w.length);
  for (const st of info.passes) {
    const [bs, bd] = st.blend ?? [-1, -1], [at, ar] = st.atest ?? [-1, 0], of = st.offset ?? [0, 0];
    w.i32(st.zwrite ?? -1); w.i32(st.ztest ?? -1); w.i32(st.cull ?? -1);
    w.i32(bs); w.i32(bd); w.i32(at); w.f32(ar); w.i32(st.cmask ?? 15); w.f32(of[0], of[1]);
  }
  w.align(4); w.setU32(po + 4, w.length);
  for (const [pn, pt, pd] of info.props) { w.str(pn); w.str(pt); w.str(pd); }
  return { blob: w.finish(), aux: 0, info: { name, dump, textLen: text.length } };
}

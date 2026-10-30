// GameObject hierarchies (prefabs and scenes) -> DQPF blob (port of dqivconv/prefab.py).
import * as B from "./blob";
import { PyFloat, SerializedFile, UObject } from "./unity";
import type { Resolver } from "./registry";

const C_TRANSFORM = 4, C_CAMERA = 20, C_MESHRENDERER = 23, C_MESHFILTER = 33, C_ANIMATION = 111, C_MONOBEHAVIOUR = 114, C_SKINNED = 137;
const P_INT = 0, P_FLOAT = 1, P_BOOL = 2, P_STR = 3, P_VEC2 = 4, P_VEC3 = 5, P_VEC4 = 6, P_ASSET = 7, P_LOCAL = 8;
const TYPED = new Set([C_CAMERA, C_MESHRENDERER, C_MESHFILTER, C_ANIMATION, C_SKINNED]);
const SKIP_COMPS = new Set([95]);      // Animator (FBX importer default; the game uses the legacy Animation component)
const HDR = 64, NODE = 80, COMP = 16;

type PVal = number | boolean | string;
const f = (x: any): number => (x instanceof PyFloat ? x.v : Number(x));
const sameKeys = (v: object, ks: string[]) => { const k = Object.keys(v); return k.length === ks.length && ks.every(x => k.includes(x)); };

/** Flattened (name, type, values) list for generic components. */
class Props {
  items: [string, number, PVal[]][] = [];
  constructor(private bld: Builder) {}
  add(name: string, t: number, vals: PVal[]) { this.items.push([name, t, vals]); }

  pptr(name: string, pp: any) {
    const node = this.bld.localRef(pp);
    if (node) this.add(name, P_LOCAL, [node[0], node[1], 0, 0]);
    else this.add(name, P_ASSET, [this.bld.res.ref(pp), 0, 0, 0]);
  }

  flatten(prefix: string, v: any, depth = 0) {
    if (depth > 6) return;
    if (typeof v === "boolean") this.add(prefix, P_BOOL, [v ? 1 : 0]);
    else if (typeof v === "number") this.add(prefix, P_INT, [v]);
    else if (v instanceof PyFloat) this.add(prefix, P_FLOAT, [v.v]);
    else if (typeof v === "string") this.add(prefix, P_STR, [v]);
    else if (v instanceof Uint8Array) this.add(prefix + ".bytes", P_INT, [v.length]);
    else if (Array.isArray(v)) {
      this.add(prefix + ".size", P_INT, [v.length]);
      v.slice(0, 64).forEach((e, i) => this.flatten(`${prefix}[${i}]`, e, depth + 1));
    } else if (v && typeof v === "object") {
      if (sameKeys(v, ["m_FileID", "m_PathID"])) this.pptr(prefix, v);
      else if (sameKeys(v, ["x", "y", "z", "w"])) this.add(prefix, P_VEC4, [f(v.x), f(v.y), f(v.z), f(v.w)]);
      else if (sameKeys(v, ["x", "y", "z"])) this.add(prefix, P_VEC3, [f(v.x), f(v.y), f(v.z)]);
      else if (sameKeys(v, ["x", "y"])) this.add(prefix, P_VEC2, [f(v.x), f(v.y)]);
      else if (sameKeys(v, ["r", "g", "b", "a"])) this.add(prefix, P_VEC4, [f(v.r), f(v.g), f(v.b), f(v.a)]);
      else if (sameKeys(v, ["x", "y", "width", "height"])) this.add(prefix, P_VEC4, [f(v.x), f(v.y), f(v.width), f(v.height)]);
      else for (const kk of Object.keys(v)) {
        if (kk === "m_GameObject") continue;
        this.flatten((prefix ? prefix + "." : "") + kk, v[kk], depth + 1);
      }
    }
  }
}

function writeProps(w: B.W, className: string, props: Props) {
  w.str(className);
  w.u32(props.items.length);
  for (const [name, t, vals] of props.items) {
    w.str(name); w.u32(t);
    if (t === P_STR) { w.str(vals[0] as string); w.zeros(12); }
    else if (t === P_FLOAT || t === P_VEC2 || t === P_VEC3 || t === P_VEC4) {
      const vv = vals.map(x => Number(x)); while (vv.length < 4) vv.push(0);
      w.f32(...vv);
    } else {
      const vv = vals.map(x => Number(x)); while (vv.length < 4) vv.push(0);
      for (const x of vv) w.u32(x);
    }
  }
}

// ------------------------------------------------------------------ known MonoBehaviour layouts (no typetree in the files)
const MB_SCHEMAS: Record<string, [string, "pptr" | "pptr[]"][]> = {
  MainLoop: [],
  TouchPad: ["m_siColDebug", "m_siColMove", "m_siMovePad", "m_siMoveCurNormal", "m_siMoveCurBS", "m_siMoveCurLight", "m_siMoveDirs", "m_siMoveDirLight", "m_siMoveDirNormal"].map(n => [n, "pptr"]),
  Boot: [["m_logoTexture", "pptr[]"], ["m_logoShader", "pptr"], ["m_obbMenuParts", "pptr"], ["m_obbFont", "pptr[]"], ["m_obbMat", "pptr[]"], ["m_obbmsg", "pptr"], ["m_quitmsg", "pptr"]],
};

function parseMbFields(raw: Uint8Array, schema: [string, string][]): [[string, any][], number] {
  const dv = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
  let p = 8 + 4 + 8;                                   // m_GameObject, m_Enabled(+pad), m_Script
  const n = dv.getInt32(p, true);
  p = p + 4 + ((n + 3) & ~3);                          // m_Name
  const pptr = () => { const v = { m_FileID: dv.getInt32(p, true), m_PathID: dv.getInt32(p + 4, true) }; p += 8; return v; };
  const out: [string, any][] = [];
  for (const [name, kind] of schema) {
    if (kind === "pptr") out.push([name, pptr()]);
    else {
      const cnt = dv.getInt32(p, true); p += 4;
      const arr = [];
      for (let i = 0; i < cnt; i++) arr.push(pptr());
      out.push([name, arr]);
    }
  }
  return [out, raw.length - p];
}

export interface BuildCtx { warn(m: string): void; getObj(fname: string, pid: number): UObject | undefined; }
interface BNode { goPid: number; name: string; parent: number; layer: number; tag: number; active: number; tr: any; comps: [number, number][]; firstChild: number; next: number; }

const toHex = (b: Uint8Array) => Array.from(b, x => x.toString(16).padStart(2, "0")).join("");

export class Builder {
  nodes: BNode[] = [];
  tmap = new Map<number, number>();                 // transform pid -> node idx
  local = new Map<number, [number, number]>();      // component pid -> (node idx, class id)
  private cache = new Map<number, any>();
  constructor(private ctx: BuildCtx, private sf: SerializedFile, public res: Resolver) {}

  tt(pid: number): any {
    let t = this.cache.get(pid);
    if (t === undefined) {
      const o = this.sf.objects.get(pid);
      if (!o) throw new Error(`object ${pid} missing`);
      this.cache.set(pid, t = o.read());
    }
    return t;
  }

  localRef(pp: any): [number, number] | undefined {
    return pp.m_FileID === 0 ? this.local.get(pp.m_PathID) : undefined;
  }

  // ---- pass 1: hierarchy
  visit(goPid: number, parent: number): number {
    const go = this.tt(goPid);
    const comps: [number, number][] = go.m_Component.map(([cid, pp]: [number, any]) => [cid, pp.m_PathID]);
    const tc = comps.find(c => c[0] === C_TRANSFORM);
    if (!tc) throw new Error(`GameObject ${goPid} has no Transform`);
    const tpid = tc[1];
    const tr = this.tt(tpid);
    const idx = this.nodes.length;
    const n: BNode = {
      goPid, name: go.m_Name, parent, layer: go.m_Layer, tag: go.m_Tag, active: go.m_IsActive ? 1 : 0, tr,
      comps: comps.filter(([c]) => c !== C_TRANSFORM && !SKIP_COMPS.has(c)), firstChild: -1, next: -1,
    };
    this.nodes.push(n);
    this.tmap.set(tpid, idx);
    this.local.set(tpid, [idx, C_TRANSFORM]);
    for (const [c, p] of n.comps) this.local.set(p, [idx, c]);
    let prev = -1;
    for (const ch of tr.m_Children) {
      if (ch.m_FileID !== 0 || ch.m_PathID === 0) continue;
      const ctr = this.tt(ch.m_PathID);
      const ci = this.visit(ctr.m_GameObject.m_PathID, idx);
      if (prev < 0) n.firstChild = ci; else this.nodes[prev].next = ci;
      prev = ci;
    }
    return idx;
  }

  // ---- component payloads
  compPayload(w: B.W, cid: number, pid: number): number {
    const o = this.sf.objects.get(pid)!;
    const res = this.res;
    if (cid === C_MONOBEHAVIOUR) return this.mono(w, o);
    let t: any;
    try { t = this.cache.get(pid) ?? o.read(!TYPED.has(cid)); }
    catch (e: any) {
      this.ctx.warn(`component class ${cid} pid ${pid} unreadable: ${String(e.message).slice(0, 80)}`);
      const props = new Props(this);
      props.add("raw_hex", P_STR, [toHex(o.raw())]);
      writeProps(w, o.className, props);
      return 1;
    }
    const en = (t.m_Enabled ?? 1) ? 1 : 0;
    if (cid === C_MESHFILTER) w.u32(res.ref(t.m_Mesh, B.K_MESH));
    else if (cid === C_MESHRENDERER) {
      w.u32((t.m_CastShadows ? 1 : 0) | (t.m_ReceiveShadows ? 2 : 0));
      w.i32(t.m_LightmapIndex ?? 255); w.i32(t.m_SortingOrder ?? 0);
      w.u32(t.m_Materials.length);
      for (const m of t.m_Materials) w.u32(res.ref(m, B.K_MAT));
    } else if (cid === C_SKINNED) {
      w.u32((t.m_CastShadows ? 1 : 0) | (t.m_ReceiveShadows ? 2 : 0) | (t.m_UpdateWhenOffscreen ? 4 : 0));
      w.u32(res.ref(t.m_Mesh, B.K_MESH));
      const rb = t.m_RootBone;
      w.i32(rb.m_FileID === 0 ? this.tmap.get(rb.m_PathID) ?? -1 : -1);
      w.u32(t.m_Materials.length); w.u32(t.m_Bones.length);
      const ab = t.m_AABB;
      w.f32(ab.m_Center.x, ab.m_Center.y, ab.m_Center.z, ab.m_Extent.x, ab.m_Extent.y, ab.m_Extent.z);
      w.i32(t.m_Quality ?? 0);
      for (const m of t.m_Materials) w.u32(res.ref(m, B.K_MAT));
      for (const b of t.m_Bones) w.i32(b.m_FileID === 0 ? this.tmap.get(b.m_PathID) ?? -1 : -1);
    } else if (cid === C_ANIMATION) {
      const clips: any[] = t.m_Animations || [];
      w.i32(t.m_WrapMode ?? 0); w.u32(t.m_PlayAutomatically ? 1 : 0);
      w.i32(t.m_CullingType ?? 0); w.u32(res.ref(t.m_Animation, B.K_ANIM)); w.u32(clips.length);
      for (const c of clips) w.u32(res.ref(c, B.K_ANIM));
    } else if (cid === C_CAMERA) {
      const bg = t.m_BackGroundColor, vp = t.m_NormalizedViewPortRect;
      w.u32(t.m_ClearFlags); w.f32(bg.r, bg.g, bg.b, bg.a);
      w.f32(vp.x, vp.y, vp.width, vp.height);
      w.f32(t["near clip plane"], t["far clip plane"], t["field of view"], t["orthographic size"]);
      w.u32(t.orthographic ? 1 : 0); w.f32(t.m_Depth);
      w.u32(t.m_CullingMask.m_Bits); w.i32(t.m_RenderingPath ?? -1);
      w.u32(res.ref(t.m_TargetTexture, B.K_TEX)); w.u32(t.m_HDR ? 1 : 0);
      w.u32(t.m_OcclusionCulling ? 1 : 0);
    } else {
      const props = new Props(this);
      for (const k of Object.keys(t)) {
        if (k === "m_GameObject" || k === "m_Enabled") continue;
        props.flatten(k, t[k]);
      }
      writeProps(w, o.className, props);
    }
    return en;
  }

  mono(w: B.W, o: UObject): number {
    const raw = o.raw();
    const dv = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
    // typetree of custom scripts is not stored in the files: read the fixed MonoBehaviour prefix by hand
    const script = { m_FileID: dv.getInt32(12, true), m_PathID: dv.getInt32(16, true) };
    let cname = "?";
    const [fn, spid] = this.res.target(script);
    if (fn !== null) {
      const so = this.ctx.getObj(fn, spid);
      if (so && so.className === "MonoScript") cname = so.read().m_ClassName ?? "?";
    }
    const props = new Props(this);
    const schema = MB_SCHEMAS[cname];
    try {
      if (schema === undefined) throw new Error(`no schema for MonoBehaviour class ${cname}`);
      const [fields, left] = parseMbFields(raw, schema);
      if (left) {
        this.ctx.warn(`MonoBehaviour ${cname}: ${left} leftover bytes`);
        props.add("raw_hex", P_STR, [toHex(raw)]);
      }
      for (const [name, v] of fields) {
        if (Array.isArray(v)) {
          props.add(name + ".size", P_INT, [v.length]);
          v.forEach((e, i) => props.pptr(`${name}[${i}]`, e));
        } else props.pptr(name, v);
      }
    } catch (e: any) {
      this.ctx.warn(`MonoBehaviour ${cname} fields unreadable (${String(e.message).slice(0, 60)}); raw hex kept`);
      props.add("raw_hex", P_STR, [toHex(raw)]);
    }
    writeProps(w, cname, props);
    return raw[8] ? 1 : 0;
  }

  build(rootGoPids: number[], isScene: boolean) {
    for (const g of rootGoPids) this.visit(g, -1);
    const roots = this.nodes.map((n, i) => (n.parent < 0 ? i : -1)).filter(i => i >= 0);
    for (let i = 0; i + 1 < roots.length; i++) this.nodes[roots[i]].next = roots[i + 1];
    const compsTotal = this.nodes.reduce((s, n) => s + n.comps.length, 0);
    const w = new B.W();
    w.u32(B.MAGIC[B.K_PREFAB]); w.u32(0);
    w.u32(this.nodes.length); w.u32(compsTotal);
    w.u32(HDR); w.u32(HDR + NODE * this.nodes.length);
    w.u32(roots.length); w.u32(isScene ? 1 : 0);
    w.zeros(HDR - w.length);
    let cf = 0;
    for (const n of this.nodes) {
      const tr = n.tr, lp = tr.m_LocalPosition, lr = tr.m_LocalRotation, ls = tr.m_LocalScale;
      w.str(n.name); w.i32(n.parent); w.i32(n.firstChild); w.i32(n.next);
      w.f32(lp.x, lp.y, lp.z, lr.x, lr.y, lr.z, lr.w, ls.x, ls.y, ls.z);
      w.u8(n.layer); w.u8(n.active); w.u16(n.tag);
      w.u32(cf); w.u32(n.comps.length); w.u32(n.goPid); w.u32(0); w.u32(0);
      cf += n.comps.length;
    }
    const compsOff = w.length;
    w.zeros(COMP * compsTotal);
    let ci = 0;
    for (const n of this.nodes) {
      for (const [cid, pid] of n.comps) {
        w.align(4);
        const off = w.length;
        const en = this.compPayload(w, cid, pid);
        w.align(4);
        const rec = compsOff + ci * COMP;
        w.setU32(rec, cid); w.setU32(rec + 4, en); w.setU32(rec + 8, off); w.setU32(rec + 12, w.length - off);
        ci++;
      }
    }
    return { blob: w.finish(), aux: this.nodes.length, info: { nodes: this.nodes.length, comps: compsTotal } };
  }
}

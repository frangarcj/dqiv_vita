// Per-file conversion (worker side): enumerate a serialized file, or convert all of its exportable objects.
import * as B from "./blob";
import { Registry, Resolver, EXPORT_KIND } from "./registry";
import { CLASS_NAMES } from "./classids";
import { SerializedFile, UObject } from "./unity";
import { UnitySource } from "./unitysrc";
import { convertTexture } from "./tex";
import { convertMesh } from "./mesh";
import { convertMaterial, convertShader, shaderDeclaredName } from "./material";
import { convertText, convertFont, convertAnim } from "./misc";
import { Builder } from "./prefab";
import { hex } from "./util";

export interface EnumResult { name: string; pids: Int32Array; classes: Uint16Array; shaders: [number, string][]; }

export async function enumFile(name: string, src: UnitySource): Promise<EnumResult> {
  const sf = await src.load(name, true);
  const pids = [...sf.objects.keys()].sort((a, b) => a - b);
  const classes = new Uint16Array(pids.length);
  const shaders: [number, string][] = [];
  pids.forEach((pid, i) => {
    const o = sf.objects.get(pid)!;
    classes[i] = o.classId;
    if (CLASS_NAMES[o.classId] === "Shader") {
      let n = "";
      try { const t = o.read(); n = shaderDeclaredName(typeof t.m_Script === "string" ? t.m_Script : new TextDecoder().decode(t.m_Script), t.m_Name); } catch { /* unnamed */ }
      shaders.push([pid, n]);
    }
  });
  return { name, pids: Int32Array.from(pids), classes, shaders };
}

export interface ConvOut { gid: number; kind: number; name: string; aux: number; size: number; blob: Uint8Array | null; sha1: string; shader?: { dump: string; textLen: number }; }
export interface ConvResult { out: ConvOut[]; warns: string[]; fails: [string, number, string][]; }

async function sha1hex(b: Uint8Array): Promise<string> { return hex(new Uint8Array(await crypto.subtle.digest("SHA-1", b as BufferSource))); }

/** Make the files that MonoBehaviour scripts live in available synchronously (Builder.getObj is sync, like the Python ctx). */
async function preloadScriptFiles(sf: SerializedFile, src: UnitySource) {
  for (const o of sf.objects.values()) {
    if (o.classId !== 114 || o.size < 20) continue;
    const dv = new DataView(o.sf.data.buffer, o.sf.data.byteOffset + o.start, o.size);
    const fid = dv.getInt32(12, true);
    if (fid > 0 && fid - 1 < sf.externals.length) {
      const fn = sf.externals[fid - 1];
      if (!src.cached(fn)) { try { await src.load(fn); } catch { /* unresolvable script file: class stays "?" */ } }
    }
  }
}

export interface ConvertOpts {
  only?: Set<number>;                          // convert just these gids (the texture pass)
  keep: (kind: number) => boolean;             // return the blob (true) or only its size and hash (false)
}

export async function convertFile(fname: string, reg: Registry, src: UnitySource, opts: ConvertOpts): Promise<ConvResult> {
  const out: ConvOut[] = [], warns: string[] = [], fails: [string, number, string][] = [];
  let sf: SerializedFile;
  try { sf = await src.load(fname, true); }
  catch (e: any) { return { out, warns, fails: [["*", 0, "load: " + e.message]] }; }
  await preloadScriptFiles(sf, src);
  const res = new Resolver(reg, fname, sf.externals);
  const ctx = {
    warn: (m: string) => warns.push(`${fname}\t${m}`),
    getObj: (fn: string, pid: number): UObject | undefined => (fn === fname ? sf : src.cached(fn))?.objects.get(pid),
  };
  for (const [pid, gid, kind] of reg.byFile.get(fname) ?? []) {
    if (opts.only && !opts.only.has(gid)) continue;
    const o = pid ? sf.objects.get(pid) : undefined;
    res.unresolved = [];
    try {
      let r: { blob: Uint8Array; aux: number }, name = "", shader: ConvOut["shader"];
      if (kind === B.K_TEXT) { const t = convertText(o!.raw()); r = t; name = t.name; }
      else if (kind === B.K_PREFAB) {
        if (pid) {
          const bl = new Builder(ctx, sf, res);
          r = bl.build([pid], false);
          name = bl.nodes.length ? bl.nodes[0].name : "";
        } else {          // scene: every root GameObject
          const roots: number[] = [];
          for (const p2 of [...sf.objects.keys()].sort((a, b) => a - b)) {
            const o2 = sf.objects.get(p2)!;
            if (o2.className !== "GameObject") continue;
            const go = o2.read();
            const tp = go.m_Component.find(([c]: [number]) => c === 4)[1].m_PathID;
            if (sf.objects.get(tp)!.read().m_Father.m_PathID === 0) roots.push(p2);
          }
          const bl = new Builder(ctx, sf, res);
          r = bl.build(roots, true);
          name = fname;
        }
      } else {
        const m = o!.read();
        name = m.m_Name ?? "";
        if (kind === B.K_TEX) r = convertTexture(m);
        else if (kind === B.K_MESH) r = convertMesh(m);
        else if (kind === B.K_MAT) r = convertMaterial(m, res, reg);
        else if (kind === B.K_SHADER) { const s = convertShader(m); r = s; shader = s.info; }
        else if (kind === B.K_FONT) r = convertFont(m, res);
        else if (kind === B.K_ANIM) r = convertAnim(m);
        else throw new Error("kind");
      }
      for (const u of res.unresolved) warns.push(`${fname}\tunresolved ref: ${u}`);
      out.push({ gid, kind, name, aux: r.aux, size: r.blob.length, blob: opts.keep(kind) ? r.blob : null, sha1: opts.only ? "" : await sha1hex(r.blob), shader });
    } catch (e: any) {
      fails.push([fname, pid, `${e.constructor?.name ?? "Error"}: ${String(e.message).slice(0, 200)}`]);
      if ((globalThis as any).DQIV_VERBOSE) console.error(e);
    }
  }
  return { out, warns, fails };
}

export { EXPORT_KIND };

// Global object registry (gid assignment) and PPtr resolution (port of dqivconv/unityenv.py).
import * as B from "./blob";

export const SCENE_FILES = ["mainData", "level0"];            // scenes exported as prefabs scene/<name>
export const SHARED_FILES = ["sharedassets0.assets", "sharedassets1.assets"];
export const TOPLEVEL = new Set([...SCENE_FILES, ...SHARED_FILES]);

/** Unity classes turned into standalone pack objects. */
export const EXPORT_KIND: Record<string, number> = {
  Texture2D: B.K_TEX, Mesh: B.K_MESH, Material: B.K_MAT, Shader: B.K_SHADER, TextAsset: B.K_TEXT, Font: B.K_FONT, AnimationClip: B.K_ANIM,
};
/** Classes silently dropped (documented in the converter report). */
export const SKIP_CLASSES = new Set(["Avatar", "Animator", "MonoScript", "PreloadData", "OcclusionCullingSettings", "HaloManager", "LightmapSettings", "NavMeshSettings"]);

/** Plain-data form of the registry, structured-cloneable to workers. */
export interface RegData {
  files: string[];            // every serialized file name (the converter's `reg.files`)
  fileIdx: number[];          // per gid (index 0 = null): index into `files`
  pid: number[];              // per gid: object path id (0 for scenes)
  kind: number[];             // per gid: object kind
  shaderNames: [number, string][];
  sceneGid: [string, number][];
}

export class Registry {
  ids = new Map<string, Map<number, number>>();   // fname -> pid -> gid
  kind: number[] = [0];
  where: [string, number][] = [["", 0]];
  byFile = new Map<string, [number, number, number][]>();   // fname -> [(pid, gid, kind)] in registration order
  shaderNames = new Map<number, string>();
  files = new Set<string>();
  fileList: string[] = [];
  sceneGid = new Map<string, number>();

  add(fname: string, pid: number, kind: number): number {
    let m = this.ids.get(fname);
    if (!m) this.ids.set(fname, m = new Map());
    const have = m.get(pid);
    if (have !== undefined) return have;
    const g = this.kind.length;
    m.set(pid, g);
    this.kind.push(kind);
    this.where.push([fname, pid]);
    let l = this.byFile.get(fname);
    if (!l) this.byFile.set(fname, l = []);
    l.push([pid, g, kind]);
    return g;
  }
  gid(fname: string, pid: number): number | undefined { return this.ids.get(fname)?.get(pid); }

  toData(): RegData {
    const idx = new Map(this.fileList.map((f, i) => [f, i]));
    return {
      files: this.fileList, fileIdx: this.where.map(w => idx.get(w[0]) ?? -1), pid: this.where.map(w => w[1]), kind: this.kind,
      shaderNames: [...this.shaderNames], sceneGid: [...this.sceneGid],
    };
  }
  static fromData(d: RegData): Registry {
    const r = new Registry();
    r.fileList = d.files; r.files = new Set(d.files);
    for (let g = 1; g < d.kind.length; g++) r.add(d.files[d.fileIdx[g]], d.pid[g], d.kind[g]);
    r.shaderNames = new Map(d.shaderNames); r.sceneGid = new Map(d.sceneGid);
    return r;
  }
}

/** PPtr -> gid for the objects of one serialized file. */
export class Resolver {
  unresolved: string[] = [];
  constructor(public reg: Registry, public fname: string, public ext: string[]) {}

  target(pptr: any): [string | null, number] {
    const fid = pptr.m_FileID, pid = pptr.m_PathID;
    if (pid === 0) return [null, 0];
    if (fid === 0) return [this.fname, pid];
    if (fid - 1 >= this.ext.length) return ["?bad", pid];
    return [this.ext[fid - 1], pid];
  }

  /** gid, or BUILTIN_BIT|pid for Unity built-ins, or 0 for null/missing. */
  ref(pptr: any, wantKind = 0): number {
    const [fn, pid] = this.target(pptr);
    if (fn === null) return 0;
    const g = this.reg.gid(fn, pid);
    if (g !== undefined) {
      if (wantKind && this.reg.kind[g] !== wantKind) {
        this.unresolved.push(`${fn}:${pid} wrong kind ${this.reg.kind[g]} (want ${wantKind})`);
        return 0;
      }
      return g;
    }
    if (this.reg.files.has(fn)) {
      this.unresolved.push(`${fn}:${pid} not exported (missing/unsupported class)`);
      return 0;
    }
    return (B.BUILTIN_BIT | (pid & 0x7fffffff)) >>> 0;
  }
}

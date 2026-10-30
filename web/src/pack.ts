// Pack layout (port of write_pack in dqiv_convert.py): blobs sorted by Resources path, deduped by SHA-1, 16-byte aligned.
// Only sizes/hashes are needed to compute every offset and index.bin, so the .pak files themselves can then be streamed blob by blob.
import * as B from "./blob";
import type { Registry } from "./registry";
import { cmpBytes, concat, encodeUtf8Replace } from "./util";

/** What a conversion result contributes to the layout. `blob` is only kept for the small paks; textures are re-converted when written. */
export interface PackObj { kind: number; name: string; aux: number; size: number; sha1: string; blob: Uint8Array | null; }
export interface Placement { gid: number; off: number; size: number; }
export interface PackLayout {
  index: Uint8Array;               // index.bin
  sizes: number[];                 // size of meta.pak, mesh.pak, tex.pak, blob.pak
  placements: Placement[][];       // per data file, in file order (blobs that are duplicates of an earlier one are not listed)
}

class StrPool {
  parts: Uint8Array[] = [new Uint8Array(1)];
  size = 1;
  d = new Map<string, number>([["", 0]]);
  add(s: string): number {
    let o = this.d.get(s);
    if (o === undefined) {
      const e = encodeUtf8Replace(s);
      o = this.size;
      this.d.set(s, o);
      this.parts.push(e, new Uint8Array(1)); this.size += e.length + 1;
    }
    return o;
  }
  bytes() { return concat(this.parts); }
}

export function layoutPack(reg: Registry, results: Map<number, PackObj>, container: [string, string, number][]): PackLayout {
  const n = reg.kind.length - 1;
  const resEntries: [Uint8Array, string, number][] = [];       // (path bytes, path, gid)
  const addRes = (key: string, g: number) => resEntries.push([encodeUtf8Replace(key), key, g]);
  for (const [key, fname, pid] of container) { const g = reg.gid(fname, pid); if (g !== undefined) addRes(key, g); }
  for (const [sname, g] of reg.sceneGid) addRes("scene/" + sname.toLowerCase(), g);
  resEntries.sort((a, b) => cmpBytes(a[0], b[0]) || reg.kind[a[2]] - reg.kind[b[2]] || a[2] - b[2]);
  const firstPos = new Map<number, number>();
  resEntries.forEach((e, i) => { if (!firstPos.has(e[2])) firstPos.set(e[2], i); });
  const order = [...results.keys()].sort((a, b) => ((firstPos.get(a) ?? 1 << 30) - (firstPos.get(b) ?? 1 << 30)) || a - b);

  const sizes = [0, 0, 0, 0];
  const seen = new Map<string, number>();
  const ent = new Map<number, [number, number, number, number, string, number]>();   // gid -> kind, df, off, size, name, aux
  const placements: Placement[][] = [[], [], [], []];
  for (const g of order) {
    const r = results.get(g)!;
    const df = B.KIND_DF[r.kind];
    const key = df + ":" + r.sha1;
    let off = seen.get(key);
    if (off === undefined) {
      sizes[df] += (16 - sizes[df] % 16) % 16;
      off = sizes[df];
      placements[df].push({ gid: g, off, size: r.size });
      sizes[df] += r.size;
      seen.set(key, off);
    } else r.blob = null;                                        // duplicate: never written
    ent.set(g, [r.kind, df, off, r.size, r.name, r.aux]);
  }

  const sp = new StrPool();
  const objs = new Uint8Array(24 * (n + 1)), odv = new DataView(objs.buffer);
  for (let g = 1; g <= n; g++) {
    const e = ent.get(g), o = 24 * g;
    if (!e) { odv.setUint32(o, reg.kind[g], true); odv.setUint32(o + 16, sp.add(""), true); }       // failed / not converted: keep the slot, size 0
    else { odv.setUint32(o, e[0], true); odv.setUint32(o + 4, e[1], true); odv.setUint32(o + 8, e[2], true); odv.setUint32(o + 12, e[3], true); odv.setUint32(o + 16, sp.add(e[4]), true); odv.setUint32(o + 20, e[5], true); }
  }
  const resb = new Uint8Array(16 * resEntries.length), rdv = new DataView(resb.buffer);
  resEntries.forEach(([, key, g], i) => { rdv.setUint32(16 * i, sp.add(key), true); rdv.setUint32(16 * i + 4, g, true); rdv.setUint32(16 * i + 8, reg.kind[g], true); });
  const dfn = B.DF_NAMES.map(nm => sp.add(nm));
  const strs = sp.bytes();
  const HDR = 128, objsOff = HDR, resOff = objsOff + objs.length, strOff = resOff + resb.length;
  const hdr = new Uint8Array(HDR), hv = new DataView(hdr.buffer);
  hdr.set([0x44, 0x51, 0x49, 0x56]);                                                       // "DQIV" (what dqiv_convert.py writes and dqiv_pack.c checks)
  [1, n + 1, resEntries.length, objsOff, resOff, strOff, strs.length, B.DF_NAMES.length, 0].forEach((v, i) => hv.setUint32(4 + 4 * i, v, true));
  for (let i = 0; i < 4; i++) { hv.setUint32(64 + 8 * i, dfn[i], true); hv.setUint32(68 + 8 * i, sizes[i], true); }
  return { index: concat([hdr, objs, resb, strs]), sizes, placements };
}

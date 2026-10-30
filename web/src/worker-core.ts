// Worker-side request handler, shared by the browser Worker and the Node worker_threads entry points.
import { openHandle, type SourceHandle } from "./source";
import { ZipReader } from "./zip";
import { UnitySource } from "./unitysrc";
import { Registry, type RegData } from "./registry";
import { enumFile, convertFile } from "./convert";
import { Md5 } from "./md5";
import { K_TEX } from "./blob";

let src: UnitySource | null = null;
let reg: Registry | null = null;

export type Request =
  | { t: "md5"; file: SourceHandle }
  | { t: "init"; apk: SourceHandle; obb: SourceHandle }
  | { t: "reg"; data: RegData }
  | { t: "enum"; names: string[] }
  | { t: "convert"; names: string[] }                       // pass 1: sizes + hashes of everything, blobs of the non-texture kinds
  | { t: "objs"; items: [string, number][] };               // pass 2: the blobs of these (file, gid) pairs, in this order

export interface Reply { result: any; transfer?: ArrayBuffer[]; }

export async function handle(m: Request, progress: (p: any) => void): Promise<Reply> {
  switch (m.t) {
    case "md5": {
      const s = await openHandle(m.file);
      const h = new Md5();
      const CH = 4 << 20;
      for (let o = 0, last = 0; o < s.size; o += CH) {
        h.update(await s.read(o, Math.min(CH, s.size - o)));
        if (o - last > (32 << 20)) { progress(o / s.size); last = o; }
      }
      return { result: h.hex() };
    }
    case "init":
      src = new UnitySource(await ZipReader.open(await openHandle(m.apk)), await ZipReader.open(await openHandle(m.obb)));
      return { result: true };
    case "reg":
      reg = Registry.fromData(m.data);
      return { result: true };
    case "enum": {
      const out = [];
      for (const n of m.names) out.push(await enumFile(n, src!));
      const transfer: ArrayBuffer[] = [];
      for (const r of out) transfer.push(r.pids.buffer as ArrayBuffer, r.classes.buffer as ArrayBuffer);
      return { result: out, transfer };
    }
    case "convert": {
      const res = { out: [] as any[], warns: [] as string[], fails: [] as any[] };
      const transfer: ArrayBuffer[] = [];
      for (const n of m.names) {
        const r = await convertFile(n, reg!, src!, { keep: k => k !== K_TEX });
        res.out.push(...r.out); res.warns.push(...r.warns); res.fails.push(...r.fails);
      }
      for (const o of res.out) if (o.blob) transfer.push(o.blob.buffer);
      return { result: res, transfer };
    }
    case "objs": {
      const byFile = new Map<string, Set<number>>();
      for (const [f, g] of m.items) { if (!byFile.has(f)) byFile.set(f, new Set()); byFile.get(f)!.add(g); }
      const got = new Map<number, Uint8Array>();
      for (const [f, gids] of byFile) {
        const r = await convertFile(f, reg!, src!, { only: gids, keep: () => true });
        if (r.fails.length) throw new Error("re-conversion failed: " + JSON.stringify(r.fails[0]));
        for (const o of r.out) got.set(o.gid, o.blob!);
      }
      const blobs = m.items.map(([, g]) => got.get(g)!);
      return { result: blobs, transfer: blobs.map(b => b.buffer as ArrayBuffer) };
    }
  }
}

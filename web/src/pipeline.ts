// The whole conversion: check inputs, extract APK files, convert the Unity assets in a worker pool, write everything to a sink.
// Equivalent of tools/make_data.py + tools/convert/dqiv_convert.py; runs unchanged in the browser and in Node.
import * as B from "./blob";
import { openHandle, type SourceHandle } from "./source";
import { ZipReader } from "./zip";
import { UnitySource, HEX32 } from "./unitysrc";
import { Registry, EXPORT_KIND, SCENE_FILES } from "./registry";
import { CLASS_NAMES } from "./classids";
import { Pool } from "./pool";
import type { Sink, SinkWriter } from "./sinks";
import type { EnumResult, ConvOut } from "./convert";
import { layoutPack, type PackObj, type Placement } from "./pack";
import { Sha256 } from "./sha256";
import { hex } from "./util";
import { CG_FILES } from "./cgdata";

export const KNOWN_MD5 = {
  apk: "2f0bcb22598a16ce50a038488696bc98",     // Dragon Quest IV v1.0.5 APK (com.square_enix.android_googleplay.dq4)
  obb: "3d27a44d87f30f119fa4a7d73805c4b7",     // main.106.com.square_enix.android_googleplay.dq4.obb
};
/** sha256 of the pack produced from the supported APK/OBB by tools/make_data.py (the conversion is deterministic). */
export const PACK_SHA256: Record<string, string> = {
  "index.bin": "e09f13f2c90ebbe010d6b96a531a481ea3cf27159aeca1a736b342699c6d2ab6",
  "meta.pak": "9e21a1a6770ada363aa11a389377e918c4426bbbdfd8bcb44fbd183e11fcd176",
  "mesh.pak": "0f27a607a7e6c76d487d82410dd7afff7f9a038481ad8fa4ddde936906cce29a",
  "tex.pak": "a593dfd60e1548ac9d9d3d305fb2ccc102c0041e385de5d28842a7412f9a02cd",
  "blob.pak": "2301d6a60b0c59b175756e66b02ef572fd87645381f870518769212acafa0a48",
};
export const LANGS = ["en", "ko", "zh-tw", "zh-cn"];       // the OBB carries no Japanese game text
/** <32 hex> Unity serialized files in the supported OBB (a different count only warns when the md5 is skipped). */
export const OBB_GUID_FILES = 17769;
export const LAYOUTS = ["landscape", "split", "portrait"];
const LIBS = ["libdq4core.so", "libsdlib_android.so"];

export type InputErrorCode = "bad_zip" | "md5" | "wrong_apk" | "wrong_obb";
/** A problem with the files the user selected (as opposed to an internal failure). */
export class InputError extends Error {
  constructor(public code: InputErrorCode, public which: "apk" | "obb", message: string, public got?: string) { super(message); }
}

export interface Options { lang: string; layout: string; skipMd5?: boolean; }
export interface Progress { phase: Phase; frac: number; detail?: string; }
export type Phase = "check" | "extract" | "enumerate" | "convert" | "pack" | "write" | "obb";
export interface Report {
  seconds: Record<string, number>;
  sha256: Record<string, string>;
  identical: boolean;               // pack files == the reference conversion
  objects: number; failures: string[]; warnings: number;
  sizes: Record<string, number>;
}

/** Coalesces blobs into large chunks for the sink and hashes everything that goes through (the paks are never held whole). */
class PakWriter {
  readonly hash = new Sha256();
  written = 0;
  private buf = new Uint8Array(4 << 20);
  private n = 0;
  constructor(private w: SinkWriter, private onBytes: (n: number) => void) {}

  private async flush() { if (this.n) { await this.w.write(this.buf.subarray(0, this.n)); this.n = 0; } }
  async push(d: Uint8Array) {
    this.hash.update(d); this.written += d.length; this.onBytes(d.length);
    if (d.length >= this.buf.length >> 2) { await this.flush(); await this.w.write(d); return; }
    if (this.n + d.length > this.buf.length) await this.flush();
    this.buf.set(d, this.n); this.n += d.length;
  }
  /** Zero-pad up to `off` (blobs are 16-byte aligned) and append the blob. */
  async blobAt(off: number, d: Uint8Array) {
    if (off > this.written) await this.push(new Uint8Array(off - this.written));
    await this.push(d);
  }
  async close(): Promise<string> { await this.flush(); await this.w.close(); return this.hash.hex(); }
}

export function configText(lang: string, layout: string): string {
  let t = "# Dragon Quest IV (native port) -- ux0:data/dqiv/config.txt, one key=value per line\n";
  t += `# lang: en (default) | ko | zh-tw | zh-cn   (the OBB has no Japanese game text)\nlang=${lang}\n`;
  t += "# confirm=circle   swap Cross/Circle (Japanese layout)\n# layout=landscape|split|portrait   screen layout (default landscape)\n";
  if (layout !== "landscape") t += `layout=${layout}\n`;
  return t;
}

export async function convertAll(inputs: { apk: SourceHandle; obb: SourceHandle }, opts: Options, sink: Sink, pool: Pool,
                                 progress: (p: Progress) => void): Promise<Report> {
  const seconds: Record<string, number> = {};
  let t = performance.now();
  const lap = (k: string) => { const n = performance.now(); seconds[k] = (n - t) / 1000; t = n; };

  // ---- 1. check the inputs: zip structure, then md5 (like make_data.check_inputs)
  progress({ phase: "check", frac: 0 });
  const apkSrc = await openHandle(inputs.apk), obbSrc = await openHandle(inputs.obb);
  let apkZip: ZipReader, obbZip: ZipReader;
  try { apkZip = await ZipReader.open(apkSrc); } catch (e: any) { throw new InputError("bad_zip", "apk", "APK is not a valid zip file: " + e.message); }
  try { obbZip = await ZipReader.open(obbSrc); } catch (e: any) { throw new InputError("bad_zip", "obb", "OBB is not a valid zip file: " + e.message); }
  if (!LIBS.every(l => apkZip.has("lib/armeabi-v7a/" + l)) || !apkZip.has("assets/bin/Data/mainData")) throw new InputError("wrong_apk", "apk", "this APK does not look like Dragon Quest IV (libdq4core.so / libsdlib_android.so / mainData missing)");
  if (obbZip.entries.filter(e => HEX32.test(e.name)).length < 17000 || !obbZip.has("assets/bin/Data/level0") || !obbZip.has("assets/bin/Data/sharedassets1.assets")) throw new InputError("wrong_obb", "obb", "this OBB does not hold the Dragon Quest IV assets");
  const frac = [0, 0];
  const sums = await Promise.all((["apk", "obb"] as const).map((k, i) =>
    pool.run<string>({ t: "md5", file: inputs[k] }, { onProgress: p => { frac[i] = p; progress({ phase: "check", frac: (frac[0] * apkSrc.size + frac[1] * obbSrc.size) / (apkSrc.size + obbSrc.size) }); } })));
  (["apk", "obb"] as const).forEach((k, i) => {
    if (sums[i] !== KNOWN_MD5[k] && !opts.skipMd5)
      throw new InputError("md5", k, `${k.toUpperCase()} md5 mismatch: expected ${KNOWN_MD5[k]}, got ${sums[i]}`, sums[i]);
  });
  progress({ phase: "check", frac: 1 });
  lap("check");

  // ---- 2. files that are copied/extracted as they are
  progress({ phase: "extract", frac: 0 });
  for (const lib of LIBS) await sink.put("lib/" + lib, await apkZip.read("lib/armeabi-v7a/" + lib));
  // DQ4 has no assets/filelist.bin (libdq4core parses the OBB zip itself); only copy what the APK really holds
  for (const a of ["ct.inf"]) if (apkZip.has("assets/" + a)) await sink.put("assets/" + a, await apkZip.read("assets/" + a));
  for (const [name, data] of Object.entries(CG_FILES)) await sink.put("cg/" + name, data);
  await sink.put("config.txt", new TextEncoder().encode(configText(opts.lang, opts.layout)));
  // main.obb is the user's own file, copied byte for byte; runs in the background while the workers convert
  let obbPct = -1;
  const obbCopy = sink.putFile("main.obb", inputs.obb, f => { const p = Math.floor(f * 100); if (p !== obbPct) { obbPct = p; progress({ phase: "obb", frac: f }); } });
  obbCopy.catch(() => undefined);
  progress({ phase: "extract", frac: 1 });
  lap("extract");

  // ---- 3. enumerate every serialized file (gid assignment needs all of them)
  const src = new UnitySource(apkZip, obbZip);
  await pool.broadcast({ t: "init", apk: inputs.apk, obb: inputs.obb });
  const { names, sizes } = src.listFiles();
  const enumRes: EnumResult[] = new Array(names.length);
  const CH = 48;
  let done = 0;
  const jobs: Promise<void>[] = [];
  for (let i = 0; i < names.length; i += CH) {
    jobs.push(pool.run<EnumResult[]>({ t: "enum", names: names.slice(i, i + CH) }).then(r => {
      r.forEach((x, k) => { enumRes[i + k] = x; });
      done += r.length; progress({ phase: "enumerate", frac: done / names.length });
    }));
  }
  await Promise.all(jobs);

  const reg = new Registry();
  reg.files = new Set(names); reg.fileList = names;
  const classOf = new Map<string, Map<number, number>>();       // for the container check
  for (const r of enumRes) {
    const m = new Map<number, number>();
    const shaders = new Map(r.shaders);
    r.pids.forEach((pid, i) => {
      m.set(pid, r.classes[i]);
      const k = EXPORT_KIND[CLASS_NAMES[r.classes[i]]];
      if (k) {
        const g = reg.add(r.name, pid, k);
        if (k === B.K_SHADER) reg.shaderNames.set(g, shaders.get(pid) || "");
      }
    });
    classOf.set(r.name, m);
  }
  // prefab roots = GameObjects named by the Resources container; the two player scenes
  const container = await loadContainer(src);
  for (const [, fname, pid] of container) {
    const cn = classOf.get(fname)?.get(pid);
    if (cn !== undefined && CLASS_NAMES[cn] === "GameObject") reg.add(fname, pid, B.K_PREFAB);
  }
  for (const s of SCENE_FILES) reg.sceneGid.set(s, reg.add(s, 0, B.K_PREFAB));
  classOf.clear(); enumRes.length = 0;
  lap("enumerate");

  // ---- 4. convert in parallel, pass 1: sizes and hashes of every blob (the layout needs them), blobs of the small paks are kept
  await pool.broadcast({ t: "reg", data: reg.toData() });
  const work = names.filter(n => reg.byFile.has(n)).sort((a, b) => sizes.get(b)! - sizes.get(a)! || (a < b ? -1 : 1));
  const results = new Map<number, PackObj>();
  const shaderDumps = new Map<string, number>();
  const failures: string[] = [];
  let warnings = 0, filesDone = 0;
  const groups: string[][] = [];
  for (let i = 0; i < work.length;) {
    const g: string[] = [];
    let bytes = 0;
    while (i < work.length && g.length < 8 && (g.length === 0 || bytes + sizes.get(work[i])! < (3 << 20))) { bytes += sizes.get(work[i])!; g.push(work[i++]); }
    groups.push(g);
  }
  await Promise.all(groups.map(g => pool.run<{ out: ConvOut[]; warns: string[]; fails: [string, number, string][] }>({ t: "convert", names: g }).then(r => {
    for (const o of r.out) {
      results.set(o.gid, { kind: o.kind, name: o.name, aux: o.aux, size: o.size, blob: o.blob, sha1: o.sha1 });
      if (o.shader) {       // two different shader texts with the same dump name would need the Python ".N.txt" disambiguation
        const prev = shaderDumps.get(o.shader.dump);
        if (prev !== undefined && prev !== o.shader.textLen) failures.push(`shader dump name clash: ${o.shader.dump}`);
        shaderDumps.set(o.shader.dump, o.shader.textLen);
      }
    }
    warnings += r.warns.length;
    for (const f of r.fails) failures.push(`${f[0]}:${f[1]} ${f[2]}`);
    filesDone += g.length; progress({ phase: "convert", frac: filesDone / work.length, detail: `${results.size}` });
  })));
  lap("convert");

  // ---- 5. layout (offsets, index.bin) from the sizes/hashes alone
  progress({ phase: "pack", frac: 0 });
  const layout = layoutPack(reg, results, container);
  progress({ phase: "pack", frac: 1 });
  lap("pack");

  // ---- 6. stream the pack files to the sink in their final order, verifying sha256 on the way.
  // Textures (150 MB) were not kept: they are converted a second time, in file order, a few batches ahead of the writer.
  const totalBytes = layout.index.length + layout.sizes.reduce((a, b) => a + b, 0);
  let wrote = 0;
  const onBytes = (n: number) => { wrote += n; progress({ phase: "write", frac: Math.min(0.999, wrote / totalBytes) }); };
  const sha: Record<string, string> = {}, psizes: Record<string, number> = {};
  let identical = true;
  const finish = async (file: string, pw: PakWriter, expect: number) => {
    sha[file] = await pw.close(); psizes[file] = pw.written;
    if (pw.written !== expect) throw new Error(`${file}: wrote ${pw.written} bytes, layout says ${expect}`);
    if (sha[file] !== PACK_SHA256[file]) identical = false;
  };
  const open = async (file: string) => new PakWriter(await sink.open("pack/" + file), onBytes);

  let pw = await open("index.bin");
  await pw.push(layout.index); await finish("index.bin", pw, layout.index.length);
  for (let df = 0; df < 4; df++) {
    const file = B.DF_NAMES[df], plcs = layout.placements[df];
    pw = await open(file);
    if (df === B.DF_TEX) {
      for await (const { p, blob } of textureBlobs(plcs, reg, pool)) await pw.blobAt(p.off, blob);
    } else {
      for (const p of plcs) { const r = results.get(p.gid)!; await pw.blobAt(p.off, r.blob!); r.blob = null; }    // freed as soon as written
    }
    await finish(file, pw, layout.sizes[df]);
  }
  await obbCopy;
  progress({ phase: "write", frac: 1 });
  await sink.close();
  lap("write");
  return { seconds, sha256: sha, identical, objects: results.size, failures, warnings, sizes: psizes };
}

/** Re-convert the texture blobs in pack order. Work is dispatched in batches (bounded by count and bytes), at most 2 per worker ahead. */
async function* textureBlobs(plcs: Placement[], reg: Registry, pool: Pool): AsyncGenerator<{ p: Placement; blob: Uint8Array }> {
  const batches: Placement[][] = [];
  for (let i = 0; i < plcs.length;) {
    const b: Placement[] = [];
    let bytes = 0;
    while (i < plcs.length && b.length < 24 && (b.length === 0 || bytes + plcs[i].size <= (6 << 20))) { bytes += plcs[i].size; b.push(plcs[i++]); }
    batches.push(b);
  }
  const ahead: Promise<Uint8Array[]>[] = [];
  let next = 0;
  const launch = () => {
    while (ahead.length < pool.size * 2 && next < batches.length)
      { const r = pool.run<Uint8Array[]>({ t: "objs", items: batches[next++].map(p => [reg.where[p.gid][0], p.gid]) }); r.catch(() => undefined); ahead.push(r); }
  };
  launch();
  for (const b of batches) {
    const blobs = await ahead.shift()!;
    launch();
    for (let i = 0; i < b.length; i++) {
      if (blobs[i].length !== b[i].size) throw new Error(`texture ${b[i].gid} re-converted to ${blobs[i].length} bytes, expected ${b[i].size}`);
      yield { p: b[i], blob: blobs[i] };
    }
  }
}

/** ResourceManager.m_Container of mainData: Resources.Load key -> (file, path id). */
async function loadContainer(src: UnitySource): Promise<[string, string, number][]> {
  const sf = await src.load("mainData");
  const rm = [...sf.objects.values()].find(o => CLASS_NAMES[o.classId] === "ResourceManager");
  if (!rm) throw new InputError("wrong_apk", "apk", "no ResourceManager in mainData");
  const t = rm.read();
  return t.m_Container.map(([key, pp]: [string, any]) => [key, pp.m_FileID === 0 ? "mainData" : sf.externals[pp.m_FileID - 1], pp.m_PathID]);
}

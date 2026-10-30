// Node specifics: file sources, worker_threads pool, directory sink. Used by the headless test harness only.
import { open } from "node:fs/promises";
import { Worker, parentPort } from "node:worker_threads";
import { setOpenHandle, type ByteSource } from "./source";
import type { WorkerLike } from "./pool";

const keep: unknown[] = [];   // file handles live for the whole run (closing them on GC is deprecated)
setOpenHandle(async h => {
  if (typeof h !== "string") throw new Error("Node sources are file paths");
  const fh = await open(h, "r");
  keep.push(fh);
  const size = (await fh.stat()).size;
  const src: ByteSource = {
    size,
    async read(offset, length) {
      const b = new Uint8Array(length);
      let got = 0;
      while (got < length) {
        const { bytesRead } = await fh.read(b, got, length - got, offset + got);
        if (!bytesRead) throw new Error("unexpected end of file " + h);
        got += bytesRead;
      }
      return b;
    },
  };
  return src;
});

export function spawnNodeWorker(url: string | URL): WorkerLike {
  // a small old-generation limit makes V8 collect garbage early instead of growing every worker to hundreds of MB
  const w = new Worker(url, { resourceLimits: { maxOldGenerationSizeMb: 96, maxYoungGenerationSizeMb: 8} });
  const wl: WorkerLike = {
    postMessage: (m, t) => w.postMessage(m, t),
    terminate: () => w.terminate(),
    onmessage: null, onerror: null,
  };
  w.on("message", m => wl.onmessage?.(m));
  w.on("error", e => wl.onerror?.(e));
  return wl;
}

export { parentPort };

import { mkdir, writeFile, copyFile } from "node:fs/promises";
import { dirname, join } from "node:path";
import { BaseSink, ZipSink, type SinkWriter, type ZipOut } from "./sinks";

/** Writes the output tree straight into a directory. */
export class NodeDirSink extends BaseSink {
  constructor(private root: string) { super(); }
  async open(path: string): Promise<SinkWriter> {
    const p = join(this.root, path); await mkdir(dirname(p), { recursive: true });
    const fh = await open(p, "w");
    return { write: async chunk => { await fh.write(chunk, 0, chunk.length); }, close: () => fh.close() };
  }
  override async putFile(path: string, file: any, onProgress?: (f: number) => void) {
    const p = join(this.root, path); await mkdir(dirname(p), { recursive: true });
    await copyFile(file as string, p); onProgress?.(1);
  }
}

/** ZIP output to a file (exercises the streaming ZipSink path that the browser uses with showSaveFilePicker). */
export class NodeFileZipOut implements ZipOut {
  private fh: import("node:fs/promises").FileHandle | null = null;
  constructor(private path: string) {}
  async write(chunk: Uint8Array) { this.fh ??= await open(this.path, "w"); await this.fh.write(chunk, 0, chunk.length); }
  async finish() { await this.fh?.close(); }
}
export const nodeZipSink = (path: string) => new ZipSink(new NodeFileZipOut(path), "dqiv/");

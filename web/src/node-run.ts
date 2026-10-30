// Headless driver (Node): the same pipeline as the page, with worker_threads and a directory sink.
import "./node-env";
import { cpus } from "node:os";
import { Pool } from "./pool";
import { spawnNodeWorker, NodeDirSink, nodeZipSink } from "./node-env";
import { convertAll, type Options, type Progress } from "./pipeline";

export async function runNode(apk: string, obb: string, outdir: string, opts: Options, jobs = Math.min(2, cpus().length), onProgress?: (p: Progress) => void, zipPath?: string) {
  const url = new URL("./node-worker.mjs", import.meta.url);
  const pool = new Pool(jobs, () => spawnNodeWorker(url));
  try {
    return await convertAll({ apk, obb }, opts, zipPath ? nodeZipSink(zipPath) : new NodeDirSink(outdir), pool, onProgress ?? (() => {}));
  } finally {
    await pool.close();
  }
}

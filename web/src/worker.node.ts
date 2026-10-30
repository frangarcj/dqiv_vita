// Node worker_threads entry point (test harness).
import { handle } from "./worker-core";
import { parentPort } from "./node-env";

parentPort!.on("message", async (data: any) => {
  const { id, ...msg } = data;
  try {
    const r = await handle(msg, p => parentPort!.postMessage({ id, progress: p }));
    parentPort!.postMessage({ id, ok: true, result: r.result }, r.transfer ?? []);
  } catch (err: any) {
    parentPort!.postMessage({ id, ok: false, error: String(err?.stack ?? err) });
  }
});

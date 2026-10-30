// Browser Web Worker entry point (bundled to dist/worker.js and inlined into app.js as a Blob URL so the page also works from file://).
import { handle } from "./worker-core";

const ctx: any = self;
ctx.onmessage = async (e: MessageEvent) => {
  const { id, ...msg } = e.data;
  try {
    const r = await handle(msg, p => ctx.postMessage({ id, progress: p }));
    ctx.postMessage({ id, ok: true, result: r.result }, r.transfer ?? []);
  } catch (err: any) {
    ctx.postMessage({ id, ok: false, error: String(err?.stack ?? err) });
  }
};

// Tiny worker pool over an abstract worker (browser Worker or Node worker_threads), request/response with progress events.

export interface WorkerLike {
  postMessage(m: any, transfer?: any[]): void;
  terminate(): void | Promise<unknown>;
  onmessage: ((m: any) => void) | null;
  onerror: ((e: any) => void) | null;
}

interface Job { msg: any; transfer?: any[]; worker?: number; onProgress?: (p: any) => void; resolve(v: any): void; reject(e: any): void; }

export class Pool {
  private workers: WorkerLike[] = [];
  private busy: (Job | null)[] = [];
  private queue: Job[] = [];
  private nextId = 1;
  private pending = new Map<number, Job>();
  private closed = false;

  constructor(readonly size: number, spawn: () => WorkerLike) {
    for (let i = 0; i < size; i++) {
      const w = spawn();
      w.onmessage = m => this.onMessage(i, m);
      w.onerror = e => this.fail(new Error("worker crashed: " + (e?.message ?? e)));
      this.workers.push(w); this.busy.push(null);
    }
  }

  /** Run `msg` on any idle worker (or on worker `worker`). */
  run<T = any>(msg: any, opts: { transfer?: any[]; worker?: number; onProgress?: (p: any) => void } = {}): Promise<T> {
    return new Promise((resolve, reject) => {
      this.queue.push({ msg, transfer: opts.transfer, worker: opts.worker, onProgress: opts.onProgress, resolve, reject });
      this.pump();
    });
  }
  broadcast<T = any>(msg: any): Promise<T[]> { return Promise.all(this.workers.map((_, i) => this.run<T>(msg, { worker: i }))); }

  private pump() {
    for (let i = 0; i < this.workers.length; i++) {
      if (this.busy[i]) continue;
      const k = this.queue.findIndex(j => j.worker === undefined || j.worker === i);
      if (k < 0) continue;
      const [job] = this.queue.splice(k, 1);
      const id = this.nextId++;
      this.busy[i] = job; this.pending.set(id, job);
      this.workers[i].postMessage({ id, ...job.msg }, job.transfer);
    }
  }

  private onMessage(i: number, m: any) {
    const job = this.pending.get(m.id);
    if (!job) return;
    if (m.progress !== undefined) { job.onProgress?.(m.progress); return; }
    this.pending.delete(m.id); this.busy[i] = null;
    if (m.ok) job.resolve(m.result); else job.reject(new Error(m.error));
    this.pump();
  }

  private fail(e: Error) {
    for (const j of [...this.pending.values(), ...this.queue]) j.reject(e);
    this.pending.clear(); this.queue = [];
  }

  async close() {
    if (this.closed) return;
    this.closed = true;
    await Promise.all(this.workers.map(w => w.terminate()));
  }
}

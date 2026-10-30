// Output sinks. Files are written as streams of chunks, so nothing but the chunk in flight is ever held in memory:
//   DirSink   a folder picked with the File System Access API
//   ZipSink   a store-only ZIP, streamed either into a file picked with showSaveFilePicker or (Firefox/Safari) accumulated as a Blob
import { openHandle, type SourceHandle } from "./source";

export interface SinkWriter {
  /** The chunk may be reused by the caller as soon as the promise resolves. */
  write(chunk: Uint8Array): Promise<void>;
  close(): Promise<void>;
  /** Called instead of close() when the write failed (releases whatever the sink holds). */
  abort?(): void;
}

export interface Sink {
  open(path: string): Promise<SinkWriter>;
  put(path: string, data: Uint8Array): Promise<void>;
  /** Copy a user file unchanged (never held in memory as a whole). */
  putFile(path: string, file: SourceHandle, onProgress?: (frac: number) => void): Promise<void>;
  close(): Promise<void>;
}

const BLOCK = 4 << 20;

export abstract class BaseSink implements Sink {
  abstract open(path: string): Promise<SinkWriter>;
  async put(path: string, data: Uint8Array) {
    const w = await this.open(path);
    try {
      for (let o = 0; o < data.length; o += BLOCK) await w.write(data.subarray(o, o + BLOCK));
      await w.close();
    } catch (e) { w.abort?.(); throw e; }
  }
  async putFile(path: string, file: SourceHandle, onProgress?: (frac: number) => void) {
    const src = await openHandle(file);
    const w = await this.open(path);
    try {
      for (let o = 0; o < src.size; o += BLOCK) {
        await w.write(await src.read(o, Math.min(BLOCK, src.size - o)));
        onProgress?.(Math.min(1, (o + BLOCK) / src.size));
      }
      await w.close();
    } catch (e) { w.abort?.(); throw e; }
  }
  async close() {}
}

export class DirSink extends BaseSink {
  constructor(private root: FileSystemDirectoryHandle) { super(); }
  async open(path: string): Promise<SinkWriter> {
    const parts = path.split("/");
    let d = this.root;
    for (const p of parts.slice(0, -1)) d = await d.getDirectoryHandle(p, { create: true });
    const w = await (await d.getFileHandle(parts[parts.length - 1], { create: true })).createWritable();
    return { write: chunk => w.write(chunk as Uint8Array<ArrayBuffer>), close: () => w.close() };
  }
}

// ------------------------------------------------------------------ ZIP
const crcTable = new Int32Array(256);
for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; crcTable[n] = c; }
export function crc32(data: Uint8Array, crc = 0): number {
  let c = ~crc;
  for (let i = 0; i < data.length; i++) c = crcTable[(c ^ data[i]) & 255] ^ (c >>> 8);
  return ~c >>> 0;
}

/** Where the zip bytes go. */
export interface ZipOut {
  write(chunk: Uint8Array): Promise<void>;
  /** Append a user file; a Blob target keeps a reference instead of a copy (the bytes are only read to compute the CRC). */
  writeFile?(file: Blob): void;
  finish(): Promise<void>;
}

/** Streams straight into a writable file (showSaveFilePicker): no zip data is kept in memory. */
export class FileZipOut implements ZipOut {
  constructor(private w: FileSystemWritableFileStream) {}
  write(chunk: Uint8Array) { return this.w.write(chunk as Uint8Array<ArrayBuffer>); }
  finish() { return this.w.close(); }
}

/** Accumulates the zip as a Blob; browsers keep large blobs outside the JS heap (and may page them to disk). */
export class BlobZipOut implements ZipOut {
  blob = new Blob([]);
  result: Blob | null = null;
  async write(chunk: Uint8Array) { this.blob = new Blob([this.blob, new Blob([chunk as BlobPart])]); }   // Blob() copies: the chunk can be reused
  writeFile(file: Blob) { this.blob = new Blob([this.blob, file]); }
  async finish() { this.result = new Blob([this.blob], { type: "application/zip" }); }
}

/** Store-only ZIP writer using data descriptors (so entries can be streamed). Entries are written one at a time. */
export class ZipSink extends BaseSink {
  private size = 0;
  private central: Uint8Array[] = [];
  private count = 0;
  private lock: Promise<unknown> = Promise.resolve();
  private enc = new TextEncoder();
  private dos: { time: number; date: number };

  constructor(private out: ZipOut, private prefix = "") {
    super();
    const d = new Date();
    this.dos = { time: d.getHours() << 11 | d.getMinutes() << 5 | d.getSeconds() >> 1, date: (d.getFullYear() - 1980) << 9 | (d.getMonth() + 1) << 5 | d.getDate() };
  }

  async open(path: string): Promise<SinkWriter & { addRef(crc: number, size: number): void }> {
    let release!: () => void;
    const prev = this.lock;
    this.lock = new Promise<void>(r => (release = r));
    await prev;                                         // one entry at a time
    const name = this.enc.encode(this.prefix + path);
    const lh = new Uint8Array(30 + name.length), v = new DataView(lh.buffer);
    v.setUint32(0, 0x04034b50, true); v.setUint16(4, 20, true); v.setUint16(6, 0x0808, true);      // data descriptor + utf-8 names
    v.setUint16(10, this.dos.time, true); v.setUint16(12, this.dos.date, true); v.setUint16(26, name.length, true);
    lh.set(name, 30);
    const offset = this.size;
    await this.out.write(lh); this.size += lh.length;
    let crc = 0, size = 0;
    const finish = async () => {
      if (size >= 0xffffffff || offset >= 0xffffffff) throw new Error("zip64 is not supported: split the output");
      const dd = new Uint8Array(16), dv = new DataView(dd.buffer);
      dv.setUint32(0, 0x08074b50, true); dv.setUint32(4, crc, true); dv.setUint32(8, size, true); dv.setUint32(12, size, true);
      await this.out.write(dd); this.size += 16;
      const ch = new Uint8Array(46 + name.length), c = new DataView(ch.buffer);
      c.setUint32(0, 0x02014b50, true); c.setUint16(4, 20, true); c.setUint16(6, 20, true); c.setUint16(8, 0x0808, true);
      c.setUint16(12, this.dos.time, true); c.setUint16(14, this.dos.date, true);
      c.setUint32(16, crc, true); c.setUint32(20, size, true); c.setUint32(24, size, true); c.setUint16(28, name.length, true); c.setUint32(42, offset, true);
      ch.set(name, 46);
      this.central.push(ch); this.count++;
      release();
    };
    return {
      write: async chunk => { crc = crc32(chunk, crc); size += chunk.length; this.size += chunk.length; await this.out.write(chunk); },
      close: finish,
      abort: release,
      addRef: (c: number, n: number) => { crc = c; size += n; this.size += n; },
    };
  }

  override async putFile(path: string, file: SourceHandle, onProgress?: (f: number) => void) {
    if (!this.out.writeFile || typeof file === "string") return super.putFile(path, file, onProgress);
    const src = await openHandle(file);
    const w = await this.open(path);
    try {
      // CRC pass only; the File itself is appended by reference
      let crc = 0;
      for (let o = 0; o < src.size; o += BLOCK) {
        crc = crc32(await src.read(o, Math.min(BLOCK, src.size - o)), crc);
        onProgress?.(Math.min(1, (o + BLOCK) / src.size));
      }
      this.out.writeFile(file);
      w.addRef(crc, src.size);
      await w.close();
    } catch (e) { w.abort?.(); throw e; }
  }

  async close() {
    await this.lock;
    const cd = this.central.reduce((s, c) => s + c.length, 0);
    for (const c of this.central) await this.out.write(c);
    const end = new Uint8Array(22), v = new DataView(end.buffer);
    v.setUint32(0, 0x06054b50, true); v.setUint16(8, this.count, true); v.setUint16(10, this.count, true);
    v.setUint32(12, cd, true); v.setUint32(16, this.size, true);
    await this.out.write(end);
    await this.out.finish();
  }
}

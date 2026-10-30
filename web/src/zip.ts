// Minimal random-access ZIP reader (central directory + stored/deflate entries) on top of a ByteSource.
// Inflation uses the vendored fflate; only the entries that are asked for are ever read.
import { inflateSync } from "../vendor/fflate.js";
import type { ByteSource } from "./source";
import { decodeUtf8Escape } from "./util";

export interface ZipEntry { name: string; method: number; csize: number; usize: number; offset: number; nlen: number; }

export class ZipReader {
  readonly entries: ZipEntry[] = [];
  readonly byName = new Map<string, ZipEntry>();
  private constructor(private src: ByteSource) {}

  static async open(src: ByteSource): Promise<ZipReader> {
    const z = new ZipReader(src);
    const tailLen = Math.min(src.size, 65557);
    const tail = await src.read(src.size - tailLen, tailLen);
    const tv = new DataView(tail.buffer, tail.byteOffset, tail.byteLength);
    let e = tailLen - 22;
    while (e >= 0 && tv.getUint32(e, true) !== 0x06054b50) e--;
    if (e < 0) throw new Error("not a zip file (no end-of-central-directory record)");
    const count = tv.getUint16(e + 10, true), cdSize = tv.getUint32(e + 12, true), cdOff = tv.getUint32(e + 16, true);
    if (count === 0xffff || cdOff === 0xffffffff) throw new Error("zip64 archives are not supported");
    const cd = await src.read(cdOff, cdSize);
    const v = new DataView(cd.buffer, cd.byteOffset, cd.byteLength);
    for (let p = 0, i = 0; i < count; i++) {
      if (v.getUint32(p, true) !== 0x02014b50) throw new Error("corrupt zip central directory");
      const nlen = v.getUint16(p + 28, true), elen = v.getUint16(p + 30, true), clen = v.getUint16(p + 32, true);
      const ent: ZipEntry = {
        method: v.getUint16(p + 10, true), csize: v.getUint32(p + 20, true), usize: v.getUint32(p + 24, true),
        offset: v.getUint32(p + 42, true), nlen, name: decodeUtf8Escape(cd.subarray(p + 46, p + 46 + nlen)),
      };
      z.entries.push(ent); z.byName.set(ent.name, ent);
      p += 46 + nlen + elen + clen;
    }
    return z;
  }

  has(name: string) { return this.byName.has(name); }

  /** `outFor(size)` may supply the buffer to inflate into (reused by the caller; the result is then only valid until the next call). */
  async read(name: string | ZipEntry, outFor?: (size: number) => Uint8Array): Promise<Uint8Array> {
    const ent = typeof name === "string" ? this.byName.get(name) : name;
    if (!ent) throw new Error("zip entry not found: " + name);
    // one read for local header + data (the local extra field is normally tiny; fall back to a second read when it is not)
    const GUESS = 64;
    const buf = await this.src.read(ent.offset, Math.min(this.src.size - ent.offset, 30 + ent.nlen + GUESS + ent.csize));
    const lv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    if (lv.getUint32(0, true) !== 0x04034b50) throw new Error("corrupt zip local header: " + ent.name);
    const start = 30 + lv.getUint16(26, true) + lv.getUint16(28, true);
    const data = start + ent.csize <= buf.length ? buf.subarray(start, start + ent.csize) : await this.src.read(ent.offset + start, ent.csize);
    if (ent.method === 0) return data;
    if (ent.method !== 8) throw new Error("unsupported zip compression method " + ent.method + ": " + ent.name);
    return inflateSync(data, { out: outFor ? outFor(ent.usize) : new Uint8Array(ent.usize) });
  }
}

// Incremental MD5 (RFC 1321). crypto.subtle has no MD5 and no incremental hashing, and the OBB is too big to hash in one go.

const K = new Int32Array(64), S = [7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21];
for (let i = 0; i < 64; i++) K[i] = Math.floor(Math.abs(Math.sin(i + 1)) * 4294967296) | 0;

export class Md5 {
  private st = new Int32Array([0x67452301, 0xefcdab89 | 0, 0x98badcfe | 0, 0x10325476]);
  private buf = new Uint8Array(64);
  private n = 0;        // bytes in buf
  private total = 0;
  private m = new Int32Array(16);

  private block(b: Uint8Array, o: number) {
    const m = this.m;
    for (let i = 0; i < 16; i++) m[i] = b[o + 4 * i] | b[o + 4 * i + 1] << 8 | b[o + 4 * i + 2] << 16 | b[o + 4 * i + 3] << 24;
    let a = this.st[0], bb = this.st[1], c = this.st[2], d = this.st[3];
    for (let i = 0; i < 64; i++) {
      let f: number, g: number;
      if (i < 16) { f = (bb & c) | (~bb & d); g = i; }
      else if (i < 32) { f = (d & bb) | (~d & c); g = (5 * i + 1) & 15; }
      else if (i < 48) { f = bb ^ c ^ d; g = (3 * i + 5) & 15; }
      else { f = c ^ (bb | ~d); g = (7 * i) & 15; }
      const s = S[(i >> 4) * 4 + (i & 3)];
      const x = (a + f + K[i] + m[g]) | 0;
      a = d; d = c; c = bb;
      bb = (bb + ((x << s) | (x >>> (32 - s)))) | 0;
    }
    this.st[0] += a; this.st[1] += bb; this.st[2] += c; this.st[3] += d;
  }

  update(data: Uint8Array): this {
    this.total += data.length;
    let i = 0;
    if (this.n) {
      const k = Math.min(64 - this.n, data.length);
      this.buf.set(data.subarray(0, k), this.n); this.n += k; i = k;
      if (this.n < 64) return this;
      this.block(this.buf, 0); this.n = 0;
    }
    for (; i + 64 <= data.length; i += 64) this.block(data, i);
    if (i < data.length) { this.buf.set(data.subarray(i)); this.n = data.length - i; }
    return this;
  }

  hex(): string {
    const bits = this.total * 8;
    const pad = new Uint8Array(((this.n < 56 ? 56 : 120) - this.n) + 8);
    pad[0] = 0x80;
    const dv = new DataView(pad.buffer);
    dv.setUint32(pad.length - 8, bits >>> 0, true);
    dv.setUint32(pad.length - 4, Math.floor(bits / 4294967296), true);
    this.update(pad);
    let s = "";
    for (let i = 0; i < 4; i++) for (let j = 0; j < 4; j++) s += ((this.st[i] >>> (8 * j)) & 255).toString(16).padStart(2, "0");
    return s;
  }
}

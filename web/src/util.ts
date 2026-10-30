// Small byte/string helpers shared by the whole converter.

const utf8Strict = new TextDecoder("utf-8", { fatal: true, ignoreBOM: true });
const utf8Enc = new TextEncoder();

/** Python's bytes.decode("utf8", "surrogateescape"): invalid bytes become lone surrogates U+DC80..U+DCFF. */
export function decodeUtf8Escape(b: Uint8Array): string {
  try { return utf8Strict.decode(b); } catch { /* fall through to the slow path */ }
  let s = "";
  for (let i = 0; i < b.length;) {
    const c = b[i];
    let n = c < 0x80 ? 0 : c >= 0xc2 && c < 0xe0 ? 1 : c >= 0xe0 && c < 0xf0 ? 2 : c >= 0xf0 && c < 0xf5 ? 3 : -1;
    if (n >= 0) {
      try { s += utf8Strict.decode(b.subarray(i, i + n + 1)); i += n + 1; continue; } catch { /* invalid sequence */ }
    }
    s += String.fromCharCode(0xdc00 + c); i++;
  }
  return s;
}

/** Python's str.encode("utf-8", "replace"): lone surrogates become '?'. */
export function encodeUtf8Replace(s: string): Uint8Array {
  if (!/[\ud800-\udfff]/.test(s)) return utf8Enc.encode(s);
  s = s.replace(/[\ud800-\udbff][\udc00-\udfff]|[\ud800-\udfff]/g, m => (m.length === 2 ? m : "?"));
  return utf8Enc.encode(s);
}

export function concat(parts: Uint8Array[]): Uint8Array {
  let n = 0;
  for (const p of parts) n += p.length;
  const out = new Uint8Array(n);
  let o = 0;
  for (const p of parts) { out.set(p, o); o += p.length; }
  return out;
}

export function hex(b: Uint8Array): string {
  let s = "";
  for (let i = 0; i < b.length; i++) s += (b[i] < 16 ? "0" : "") + b[i].toString(16);
  return s;
}

/** Lexicographic byte comparison (Python bytes ordering). */
export function cmpBytes(a: Uint8Array, b: Uint8Array): number {
  const n = Math.min(a.length, b.length);
  for (let i = 0; i < n; i++) if (a[i] !== b[i]) return a[i] - b[i];
  return a.length - b.length;
}

export class ConvError extends Error {}

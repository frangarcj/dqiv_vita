// Types for the one function we use from the vendored fflate (MIT, https://github.com/101arrowz/fflate).
export function inflateSync(data: Uint8Array, opts?: { out?: Uint8Array; dictionary?: Uint8Array }): Uint8Array;

// Random-access byte source: a browser File/Blob (slice on demand, never loaded whole) or a Node file.

export interface ByteSource {
  readonly size: number;
  read(offset: number, length: number): Promise<Uint8Array>;
}

export function blobSource(b: Blob): ByteSource {
  return {
    size: b.size,
    async read(offset, length) { return new Uint8Array(await b.slice(offset, offset + length).arrayBuffer()); },
  };
}

/** What crosses the main-thread/worker boundary: a Blob/File in the browser, a path string in Node. */
export type SourceHandle = Blob | string;

export let openHandle: (h: SourceHandle) => Promise<ByteSource> = async h => {
  if (typeof h === "string") throw new Error("path sources need the Node entry point");
  return blobSource(h);
};
export function setOpenHandle(f: typeof openHandle) { openHandle = f; }

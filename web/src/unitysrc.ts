// Where the converter finds the Unity serialized files: some in the APK, most in the OBB (see tools/make_data.py stage_unity).
import { SerializedFile } from "./unity";
import { ZipReader } from "./zip";
import { concat } from "./util";
import { SCENE_FILES, SHARED_FILES, TOPLEVEL } from "./registry";

export const DATA = "assets/bin/Data/";
export const HEX32 = /^assets\/bin\/Data\/([0-9a-f]{32})$/;

export class UnitySource {
  private cache = new Map<string, SerializedFile>();
  private scratch = new Uint8Array(1 << 22);      // inflate target reused for the transient per-file data (no 16 MB allocation per texture file)
  constructor(public apk: ZipReader, public obb: ZipReader) {}

  /** The serialized files, in the converter's order: shared assets, scenes, then every <guid> file sorted by name. */
  listFiles(): { names: string[]; sizes: Map<string, number> } {
    const sizes = new Map<string, number>();
    for (const e of this.obb.entries) {
      const m = HEX32.exec(e.name);
      if (m) sizes.set(m[1], e.usize);
    }
    const guids = [...sizes.keys()].sort();
    sizes.set("mainData", this.apk.byName.get(DATA + "mainData")?.usize ?? 0);
    sizes.set("sharedassets0.assets", this.apkSplits().reduce((s, n) => s + this.apk.byName.get(n)!.usize, 0));
    for (const n of ["level0", "sharedassets1.assets"]) sizes.set(n, this.obb.byName.get(DATA + n)?.usize ?? 0);
    return { names: [...SHARED_FILES, ...SCENE_FILES, ...guids], sizes };
  }

  private apkSplits(): string[] {
    return this.apk.entries.map(e => e.name).filter(n => n.startsWith(DATA + "sharedassets0.assets.split"))
      .sort((a, b) => parseInt(a.split("split")[1]) - parseInt(b.split("split")[1]));
  }

  async bytes(name: string, reuse = false): Promise<Uint8Array> {
    if (name === "mainData") return this.apk.read(DATA + "mainData");
    if (name === "sharedassets0.assets") {
      const parts = this.apkSplits();
      if (!parts.length) throw new Error("APK has no sharedassets0.assets.split* entries (wrong file?)");
      return concat(await Promise.all(parts.map(p => this.apk.read(p))));
    }
    return this.obb.read(DATA + name, reuse ? n => {
      if (this.scratch.length < n) this.scratch = new Uint8Array(Math.max(n, this.scratch.length * 2));
      return this.scratch.subarray(0, n);
    } : undefined);
  }

  /** `transient`: the file is only used until the next transient load (its bytes live in the shared scratch buffer). */
  async load(name: string, transient = false): Promise<SerializedFile> {
    let sf = this.cache.get(name);
    if (!sf) {
      const top = TOPLEVEL.has(name);
      sf = new SerializedFile(name, await this.bytes(name, transient && !top));
      if (top || !transient) this.cache.set(name, sf);
    }
    return sf;
  }
  cached(name: string) { return this.cache.get(name); }
}

// Unity 4.7 serialized file (format 9) reader + TypeTree value reader.
// The value semantics mirror UnityPy 1.25's helpers/TypeTreeHelper.py (the reference converter's reader).
import { CLASS_NAMES } from "./classids";
import { TYPE_TREES } from "./typetrees";
import { decodeUtf8Escape } from "./util";

export interface TypeNode { type: string; name: string; byteSize: number; metaFlag: number; children: TypeNode[]; }

/** Boxed float: lets the generic component flattener tell Python floats from ints (only with boxFloats). */
export class PyFloat { constructor(public v: number) {} }

const treeCache = new Map<number, TypeNode>();
function mkNode(a: any): TypeNode { return { type: a[0], name: a[1], byteSize: 0, metaFlag: a[2], children: (a[3] ?? []).map(mkNode) }; }
/** The game's files carry no type trees (stripped player build): like UnityPy we use the Unity 4.7.2 class database (typetrees.ts). */
function classTree(classId: number): TypeNode | undefined {
  let n = treeCache.get(classId);
  if (!n && TYPE_TREES[classId]) treeCache.set(classId, n = mkNode(TYPE_TREES[classId]));
  return n;
}

export class UObject {
  constructor(public sf: SerializedFile, public pid: number, public classId: number, public typeId: number,
              public start: number, public size: number) {}
  get className(): string { return CLASS_NAMES[this.classId] ?? `Unknown (${this.classId})`; }
  raw(): Uint8Array { return this.sf.data.subarray(this.start, this.start + this.size); }
  /** Python: obj.read_typetree() (dict), requires the whole object to be consumed. */
  read(boxFloats = false): any {
    const node = this.sf.types.get(this.classId) ?? classTree(this.classId);
    if (!node) throw new Error(`no type tree for class ${this.classId}`);
    const r = new TTReader(this.raw(), boxFloats);
    const v = r.value(node);
    if (r.pos !== this.size) throw new Error(`Expected to read ${this.size} bytes, but only read ${r.pos} bytes`);
    return v;
  }
}

export class SerializedFile {
  version = 0;
  unityVersion = "";
  types = new Map<number, TypeNode>();     // embedded type trees by class id (none in this game)
  objects = new Map<number, UObject>();
  externals: string[] = [];
  constructor(public name: string, public data: Uint8Array) {
    const dv = new DataView(data.buffer, data.byteOffset, data.byteLength);
    const metaSize = dv.getUint32(0), fileSize = dv.getUint32(4);
    this.version = dv.getUint32(8);
    const dataOffset = dv.getUint32(12);
    if (this.version !== 9) throw new Error(`${name}: unsupported serialized file version ${this.version}`);
    if (fileSize !== data.length || metaSize > data.length) throw new Error(`${name}: size mismatch`);
    if (data[16] !== 0) throw new Error(`${name}: big-endian serialized files are not supported`);
    const r = new Cursor(data, 20);
    this.unityVersion = r.cstr();
    r.i32();                                   // build target
    const nTypes = r.i32();
    for (let i = 0; i < nTypes; i++) {
      const classId = r.i32();
      const node = r.typeTree();
      if (!this.types.has(classId)) this.types.set(classId, node);
    }
    const bigIds = r.i32();
    const nObj = r.i32();
    for (let i = 0; i < nObj; i++) {
      const pid = bigIds ? Number(r.i64()) : r.i32();
      const start = r.u32() + dataOffset, size = r.u32();
      const typeId = r.i32(), classId = r.u16();
      r.u16();                                 // is_destroyed
      this.objects.set(pid, new UObject(this, pid, classId, typeId, start, size));
    }
    const nExt = r.i32();
    for (let i = 0; i < nExt; i++) {
      r.cstr(); r.pos += 16; r.i32();          // temp_empty, guid, type
      this.externals.push(r.cstr());
    }
  }
}

class Cursor {
  dv: DataView;
  constructor(public b: Uint8Array, public pos = 0) { this.dv = new DataView(b.buffer, b.byteOffset, b.byteLength); }
  i32() { const v = this.dv.getInt32(this.pos, true); this.pos += 4; return v; }
  u32() { const v = this.dv.getUint32(this.pos, true); this.pos += 4; return v; }
  u16() { const v = this.dv.getUint16(this.pos, true); this.pos += 2; return v; }
  i64() { const v = this.dv.getBigInt64(this.pos, true); this.pos += 8; return v; }
  cstr(): string {
    let e = this.pos;
    while (this.b[e] !== 0) { if (e >= this.b.length) throw new Error("unterminated string"); e++; }
    const s = decodeUtf8Escape(this.b.subarray(this.pos, e)); this.pos = e + 1; return s;
  }
  /** Old (format < 12) recursive type tree. */
  typeTree(): TypeNode {
    const type = this.cstr(), name = this.cstr();
    const byteSize = this.i32(); this.i32();   // size, index
    this.i32(); this.i32();                    // type flags (isArray), version
    const metaFlag = this.i32();
    const n = this.i32();
    const children: TypeNode[] = [];
    for (let i = 0; i < n; i++) children.push(this.typeTree());
    return { type, name, byteSize, metaFlag, children };
  }
}

const kAlign = 0x4000;
const aligned = (n: TypeNode) => (n.metaFlag & kAlign) !== 0;
type Prim = (r: TTReader) => any;
const PRIMS: Record<string, Prim> = {
  SInt8: r => r.dv.getInt8(r.pos++), UInt8: r => r.b[r.pos++], char: r => r.b[r.pos++],
  short: r => r.num(2, "getInt16"), SInt16: r => r.num(2, "getInt16"), "unsigned short": r => r.num(2, "getUint16"), UInt16: r => r.num(2, "getUint16"),
  int: r => r.num(4, "getInt32"), SInt32: r => r.num(4, "getInt32"), "unsigned int": r => r.num(4, "getUint32"), UInt32: r => r.num(4, "getUint32"), "Type*": r => r.num(4, "getUint32"),
  "long long": r => r.big(8, "getBigInt64"), SInt64: r => r.big(8, "getBigInt64"),
  "unsigned long long": r => r.big(8, "getBigUint64"), UInt64: r => r.big(8, "getBigUint64"), FileSize: r => r.big(8, "getBigUint64"),
  float: r => r.flt(4, "getFloat32"), double: r => r.flt(8, "getFloat64"),
  bool: r => r.b[r.pos++] !== 0,
  string: r => r.astring(), TypelessData: r => r.bytes(r.num(4, "getInt32")),
};
const ARR_BYTES = new Set(["SInt8", "UInt8", "char"]);

export class TTReader {
  dv: DataView;
  pos = 0;
  constructor(public b: Uint8Array, public boxFloats = false) { this.dv = new DataView(b.buffer, b.byteOffset, b.byteLength); }
  num(n: number, m: "getInt16" | "getUint16" | "getInt32" | "getUint32"): number { const v = this.dv[m](this.pos, true); this.pos += n; return v; }
  big(n: number, m: "getBigInt64" | "getBigUint64"): number {
    const v = this.dv[m](this.pos, true); this.pos += n;
    if (v > 9007199254740991n || v < -9007199254740991n) throw new Error("64-bit value out of the exact double range");
    return Number(v);
  }
  flt(n: number, m: "getFloat32" | "getFloat64"): any { const v = this.dv[m](this.pos, true); this.pos += n; return this.boxFloats ? new PyFloat(v) : v; }
  bytes(n: number): Uint8Array {
    if (n < 0 || this.pos + n > this.b.length) throw new Error("read past end of object");
    const s = this.b.subarray(this.pos, this.pos + n); this.pos += n; return s;      // a view: the object bytes outlive the conversion only briefly
  }
  align() { this.pos += (4 - (this.pos % 4)) % 4; }
  astring(): string {
    const n = this.num(4, "getInt32");
    if (n > 0 && n <= this.b.length - this.pos) {
      const s = decodeUtf8Escape(this.b.subarray(this.pos, this.pos + n)); this.pos += n; this.align(); return s;
    }
    return "";
  }

  value(node: TypeNode): any {
    let al = aligned(node), v: any;
    const prim = PRIMS[node.type];
    if (prim) v = prim(this);
    else if (node.type === "pair") { const a = this.value(node.children[0]); v = [a, this.value(node.children[1])]; }
    else if (node.children.length && node.children[0].type === "Array") {
      const arr = node.children[0];
      if (aligned(arr)) al = true;
      const size = this.num(4, "getInt32");
      if (size < 0) throw new Error("Negative length read from TypeTree");
      const sub = arr.children[1];
      if (aligned(sub)) v = this.array(sub, size);
      else if (!this.boxFloats && ARR_BYTES.has(sub.type)) { v = this.bytes(size); }      // fast path (UnityPy: a list of ints)
      else { v = new Array(size); for (let i = 0; i < size; i++) v[i] = this.value(sub); }
    } else {
      v = {};
      for (const c of node.children) v[c.name] = this.value(c);
    }
    if (al) this.align();
    return v;
  }

  /** UnityPy read_value_array: `size` elements of an aligned element type, alignment only after the whole array. */
  private array(sub: TypeNode, size: number): any {
    let v: any;
    const t = sub.type;
    if (ARR_BYTES.has(t)) {
      if (this.pos + size > this.b.length) throw new Error("read past end of object");
      v = this.b.subarray(this.pos, this.pos + size); this.pos += size;
    } else if (PRIMS[t] && t !== "string" && t !== "TypelessData") {
      v = new Array(size); const p = PRIMS[t];
      for (let i = 0; i < size; i++) v[i] = p(this);
    } else if (t === "string") {
      v = new Array(size); for (let i = 0; i < size; i++) v[i] = this.astring();
    } else if (t === "TypelessData") {
      v = new Array(size); for (let i = 0; i < size; i++) v[i] = this.bytes(this.num(4, "getInt32"));
    } else if (t === "pair") {
      const [k, w] = sub.children;
      v = new Array(size);
      for (let i = 0; i < size; i++) { const a = PRIMS[k.type] ? PRIMS[k.type](this) : this.value(k); v[i] = [a, PRIMS[w.type] ? PRIMS[w.type](this) : this.value(w)]; }
    } else if (sub.children.length && sub.children[0].type === "Array") {
      const inner = sub.children[0];
      const isub = inner.children[1];
      v = new Array(size);
      for (let i = 0; i < size; i++) {
        const n = this.num(4, "getInt32");
        if (aligned(isub)) v[i] = this.array(isub, n);
        else { const l = new Array(n); for (let j = 0; j < n; j++) l[j] = this.value(isub); v[i] = l; }
      }
    } else {
      v = new Array(size);
      for (let i = 0; i < size; i++) { const d: any = {}; for (const c of sub.children) d[c.name] = this.value(c); v[i] = d; }
    }
    this.align();
    return v;
  }
}

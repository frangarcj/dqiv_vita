// Mesh -> DQMS blob: interleaved GPU-friendly vertex buffer, u16 indices (port of dqivconv/mesh.py).
import * as B from "./blob";

const MF_NRM = 1, MF_COL = 2, MF_UV0 = 4, MF_UV1 = 8, MF_TAN = 16, MF_SKIN = 32;
const TOPO_TRIS = 0, TOPO_QUADS = 2;

interface Chan { dim: number; fmt: number; off: number; stride: number; }

function chan(vc: number, ch: any, stream: any): Chan | null {
  if (ch.dimension === 0) return null;
  if (ch.format > 2) throw new Error(`unsupported vertex channel format ${ch.format}`);
  return { dim: ch.dimension, fmt: ch.format, off: stream.offset + ch.offset, stride: stream.stride };
}

function half(h: number): number {
  const s = h & 0x8000 ? -1 : 1, e = (h >> 10) & 31, f = h & 1023;
  if (e === 0) return s * f * 2 ** -24;
  if (e === 31) return f ? NaN : s * Infinity;
  return s * (1 + f / 1024) * 2 ** (e - 15);
}

export function convertMesh(m: any): { blob: Uint8Array; aux: number; info: { verts: number; indices: number; subs: number; skinned: boolean } } {
  if (m.m_MeshCompression || m.m_CompressedMesh.m_Vertices.m_NumItems) throw new Error(`compressed mesh (m_MeshCompression=${m.m_MeshCompression}) not supported`);
  const vd = m.m_VertexData, vc: number = vd.m_VertexCount;
  const data: Uint8Array = vd.m_DataSize;
  const dv = new DataView(data.buffer, data.byteOffset, data.byteLength);
  const chans = vd.m_Channels, streams = vd.m_Streams;
  const ch: (Chan | null)[] = [];
  for (let i = 0; i < 6; i++) ch.push(i < chans.length ? chan(vc, chans[i], streams[chans[i].stream]) : null);
  const [pos, nrm, col, uv0, uv1, tan] = ch;
  if (!pos) throw new Error("mesh without positions");

  // attribute layout in this order: pos nrm tan col uv0 uv1
  const layout: { name: string; flag: number; c: Chan; n: number; bytes: number; off: number }[] = [];
  let stride = 0, flags = 0;
  const add = (name: string, flag: number, c: Chan | null, n: number, bytes: number) => {
    if (!c) return;
    layout.push({ name, flag, c, n, bytes, off: stride }); stride += bytes; flags |= flag;
  };
  add("pos", 0, pos, 3, 12);
  add("nrm", MF_NRM, nrm, 3, 12);
  add("tan", MF_TAN, tan, 4, 16);
  add("col", MF_COL, col, 4, 4);
  add("uv0", MF_UV0, uv0, 2, 8);
  add("uv1", MF_UV1, uv1, 2, 8);
  const offOf = (n: string) => layout.find(l => l.name === n)?.off ?? -1;

  const vb = new Uint8Array(vc * stride);
  const vdv = new DataView(vb.buffer);
  for (const l of layout) {
    const { c } = l;
    for (let v = 0; v < vc; v++) {
      const src = c.off + v * c.stride, dst = v * stride + l.off;
      if (l.name === "col") {                       // 4 x unorm8 (R,G,B,A) whatever the declared dimension
        if (c.fmt !== 2) throw new Error("colour channel must be a byte format");
        for (let k = 0; k < 4; k++) vb[dst + k] = data[src + k];
      } else {
        for (let k = 0; k < l.n; k++) {
          // a channel with fewer components than the attribute is zero padded (tangent only)
          let val = 0;
          if (k < c.dim) val = c.fmt === 0 ? dv.getFloat32(src + 4 * k, true) : c.fmt === 1 ? half(dv.getUint16(src + 2 * k, true)) : dv.getUint8(src + k);
          vdv.setFloat32(dst + 4 * k, val, true);
        }
      }
    }
  }

  const ibRaw: Uint8Array = m.m_IndexBuffer;
  const idx = new Uint16Array(ibRaw.buffer.slice(ibRaw.byteOffset, ibRaw.byteOffset + (ibRaw.length & ~1)));
  const subs: any[] = [];
  const outIdx: Uint16Array[] = [];
  let total = 0;
  for (const s of m.m_SubMeshes) {
    const first = s.firstByte >> 1, cnt = s.indexCount;
    let ii = idx.slice(first, first + cnt);
    let topo = s.topology;
    if (topo === TOPO_QUADS) {                       // GLES has no quads -> triangles
      const q = ii, t = new Uint16Array(q.length / 4 * 6);
      for (let i = 0, o = 0; i < q.length; i += 4, o += 6) { t[o] = q[i]; t[o + 1] = q[i + 1]; t[o + 2] = q[i + 2]; t[o + 3] = q[i]; t[o + 4] = q[i + 2]; t[o + 5] = q[i + 3]; }
      ii = t; topo = TOPO_TRIS;
    }
    let mx = -1;
    for (let i = 0; i < ii.length; i++) if (ii[i] > mx) mx = ii[i];
    if (ii.length && mx >= vc) throw new Error("index out of range");
    const ab = s.localAABB;
    subs.push([total, ii.length, topo, s.firstVertex, s.vertexCount, [ab.m_Center.x, ab.m_Center.y, ab.m_Center.z], [ab.m_Extent.x, ab.m_Extent.y, ab.m_Extent.z]]);
    outIdx.push(ii);
    total += ii.length;
  }

  const bind: any[] = m.m_BindPose || [];
  const skin: any[] = m.m_Skin || [];
  if (skin.length && skin.length !== vc) throw new Error("skin/vertex count mismatch");
  if (skin.length && bind.length) flags |= MF_SKIN;

  const w = new B.W(96 + vb.length + total * 2 + subs.length * 48 + bind.length * 64 + skin.length * 20 + 128);
  w.u32(B.MAGIC[B.K_MESH]); w.u32(0);
  w.u32(vc); w.u32(stride); w.u32(total);
  w.u32(subs.length); w.u32(bind.length); w.u32(flags);
  for (const n of ["pos", "nrm", "col", "uv0", "uv1", "tan"]) w.i16(offOf(n));
  w.u32(0);
  const pv = w.length;
  for (let i = 0; i < 5; i++) w.u32(0);
  const lae = m.m_LocalAABB;
  w.f32(lae.m_Center.x, lae.m_Center.y, lae.m_Center.z, lae.m_Extent.x, lae.m_Extent.y, lae.m_Extent.z);
  w.zeros(96 - w.length);
  const here = (slot: number) => { w.align(16); w.setU32(pv + 4 * slot, w.length); };
  here(0); w.raw(vb);
  here(1); for (const ii of outIdx) w.raw(new Uint8Array(ii.buffer, ii.byteOffset, ii.byteLength));
  here(2);
  for (const [fi, ic, topo, fv, vcnt, c, e] of subs) { w.u32(fi); w.u32(ic); w.u32(topo); w.u32(fv); w.u32(vcnt); w.f32(...c, ...e); w.u32(0); }
  if (flags & MF_SKIN) {
    here(3);
    for (const p of bind) for (let c = 0; c < 4; c++) for (let r = 0; r < 4; r++) w.f32(p[`e${r}${c}`]);
    here(4);
    for (const s of skin) {
      for (let i = 0; i < 4; i++) w.u8(s[`boneIndex[${i}]`]);
      for (let i = 0; i < 4; i++) w.f32(s[`weight[${i}]`]);
    }
  }
  return { blob: w.finish(), aux: vc, info: { verts: vc, indices: total, subs: subs.length, skinned: !!(flags & MF_SKIN) } };
}

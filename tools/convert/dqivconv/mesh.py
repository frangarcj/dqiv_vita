"""Mesh -> DQMS blob (interleaved GPU-friendly vertex buffer, u16 indices)."""
import numpy as np
from . import blob as B

MF_NRM, MF_COL, MF_UV0, MF_UV1, MF_TAN, MF_SKIN = 1, 2, 4, 8, 16, 32
TOPO_TRIS, TOPO_STRIP, TOPO_QUADS, TOPO_LINES, TOPO_LINESTRIP, TOPO_POINTS = range(6)

def _chan(data, vc, ch, stream):
    dim = ch["dimension"]
    if dim == 0:
        return None
    fmt = ch["format"]
    off = stream["offset"] + ch["offset"]
    stride = stream["stride"]
    if fmt == 0:      # float32
        return np.ndarray((vc, dim), "<f4", data, off, (stride, 4))
    if fmt == 1:      # float16
        return np.ndarray((vc, dim), "<f2", data, off, (stride, 2)).astype("<f4")
    if fmt == 2:      # 4 x unorm8 colour (R,G,B,A byte order)
        return np.ndarray((vc, 4), "u1", data, off, (stride, 1))
    raise ValueError("unsupported vertex channel format %d" % fmt)

def _vec(d, keys):
    return [float(d[k]) for k in keys]

def convert_mesh(m, opts):
    if m.get("m_MeshCompression", 0) or m["m_CompressedMesh"]["m_Vertices"]["m_NumItems"]:
        raise ValueError("compressed mesh (m_MeshCompression=%s) not supported" % m.get("m_MeshCompression"))
    vd = m["m_VertexData"]
    vc = vd["m_VertexCount"]
    data = bytes(vd["m_DataSize"])
    chans, streams = vd["m_Channels"], vd["m_Streams"]
    ch = [_chan(data, vc, chans[i], streams[chans[i]["stream"]]) if i < len(chans) else None for i in range(6)]
    pos, nrm, col, uv0, uv1, tan = ch
    if pos is None:
        raise ValueError("mesh without positions")
    cols = []        # (flag, offset, array-as-bytes (vc,n))
    stride = 0
    offs = {}
    def add(name, flag, arr, nbytes, conv):
        nonlocal stride
        offs[name] = (flag, stride, conv(arr))
        stride += nbytes
    f32 = lambda a: np.ascontiguousarray(a, "<f4").view(np.uint8).reshape(vc, -1)
    u8 = lambda a: np.ascontiguousarray(a, "u1").reshape(vc, -1)
    add("pos", 0, pos[:, :3], 12, f32)
    flags = 0
    if nrm is not None: add("nrm", MF_NRM, nrm[:, :3], 12, f32)
    if tan is not None: add("tan", MF_TAN, tan[:, :4] if tan.shape[1] >= 4 else np.pad(tan, ((0, 0), (0, 4 - tan.shape[1]))), 16, f32)
    if col is not None: add("col", MF_COL, col, 4, u8)
    if uv0 is not None: add("uv0", MF_UV0, uv0[:, :2], 8, f32)
    if uv1 is not None: add("uv1", MF_UV1, uv1[:, :2], 8, f32)
    vb = np.zeros((vc, stride), np.uint8)
    for name, (flag, o, a) in offs.items():
        vb[:, o:o + a.shape[1]] = a
        flags |= flag
    def off(n):
        return offs[n][1] if n in offs else -1

    idx = np.frombuffer(bytes(m["m_IndexBuffer"]), "<u2")
    subs = []
    out_idx = []
    total = 0
    for s in m["m_SubMeshes"]:
        first = s["firstByte"] // 2
        cnt = s["indexCount"]
        ii = idx[first:first + cnt]
        topo = s["topology"]
        if topo == TOPO_QUADS:      # GLES has no quads -> triangles
            q = ii.reshape(-1, 4)
            ii = q[:, [0, 1, 2, 0, 2, 3]].reshape(-1)
            topo = TOPO_TRIS
        if len(ii) and int(ii.max()) >= vc:
            raise ValueError("index out of range")
        ab = s["localAABB"]
        subs.append((total, len(ii), topo, s["firstVertex"], s["vertexCount"],
                     _vec(ab["m_Center"], "xyz"), _vec(ab["m_Extent"], "xyz")))
        out_idx.append(ii)
        total += len(ii)
    ib = np.concatenate(out_idx).astype("<u2").tobytes() if out_idx else b""

    bind = m.get("m_BindPose") or []
    skin = m.get("m_Skin") or []
    if skin and len(skin) != vc:
        raise ValueError("skin/vertex count mismatch")
    if skin and bind:
        flags |= MF_SKIN

    w = B.W()
    w.u32(B.MAGIC[B.K_MESH]); w.u32(0)                    # 0,4
    w.u32(vc); w.u32(stride); w.u32(total)                # 8,12,16
    w.u32(len(subs)); w.u32(len(bind)); w.u32(flags)      # 20,24,28
    for n in ("pos", "nrm", "col", "uv0", "uv1", "tan"):  # 32..43 : i16 offsets
        w.i16(off(n))
    w.u32(0)                                              # 44 reserved
    pv = w.b.__len__()                                    # 48: offsets
    w.u32(0); w.u32(0); w.u32(0); w.u32(0); w.u32(0)      # vertex_off,index_off,submesh_off,bindpose_off,skin_off
    lae = m["m_LocalAABB"]
    w.f32(*_vec(lae["m_Center"], "xyz"), *_vec(lae["m_Extent"], "xyz"))   # 68..91
    w.zeros(96 - len(w))                                  # header = 96 bytes
    assert len(w) == 96
    def here(slot):
        w.align(16)
        w.set_u32(pv + 4 * slot, len(w))
    here(0); w.raw(vb.tobytes())
    here(1); w.raw(ib)
    here(2)
    for (fi, ic, topo, fv, vcnt, c, e) in subs:
        w.u32(fi); w.u32(ic); w.u32(topo); w.u32(fv); w.u32(vcnt); w.f32(*c, *e); w.u32(0)
    if flags & MF_SKIN:
        here(3)
        for p in bind:
            # Unity Matrix4x4 e<row><col> -> stored column-major (GL layout): m[col*4+row]
            for c in range(4):
                for r in range(4):
                    w.f32(float(p["e%d%d" % (r, c)]))
        here(4)
        for s in skin:
            for i in range(4): w.u8(s["boneIndex[%d]" % i])
            for i in range(4): w.f32(float(s["weight[%d]" % i]))
    return w.finish(), vc, dict(verts=vc, indices=total, subs=len(subs), skinned=bool(flags & MF_SKIN))

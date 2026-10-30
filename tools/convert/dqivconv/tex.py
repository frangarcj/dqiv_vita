"""Texture2D -> DQTX blob.  Pixel rows are stored in Unity/GL order (row 0 = BOTTOM row, v=0)."""
import struct
import numpy as np
from . import blob as B

# output formats
TF_A8, TF_RGB565, TF_RGBA4444, TF_RGBA8888 = 1, 2, 3, 4
# flags
TFL_NPOT, TFL_HAS_ALPHA, TFL_HAD_MIPS, TFL_NOT_RESIZED = 1, 2, 4, 8

# Unity TextureFormat ids
UF = dict(Alpha8=1, ARGB4444=2, RGB24=3, RGBA32=4, ARGB32=5, RGB565=7, DXT1=10, DXT5=12, RGBA4444=13,
          BGRA32=14, ETC_RGB4=34)

def _to565(rgb):  # rgb: (n,3) uint8
    r = rgb[:, 0].astype(np.uint16); g = rgb[:, 1].astype(np.uint16); b = rgb[:, 2].astype(np.uint16)
    return (((r * 31 + 127) // 255) << 11 | ((g * 63 + 127) // 255) << 5 | ((b * 31 + 127) // 255)).astype("<u2")

def _to4444(rgba):
    r, g, b, a = [rgba[:, i].astype(np.uint16) for i in range(4)]
    return (((r * 15 + 127) // 255) << 12 | ((g * 15 + 127) // 255) << 8 |
            ((b * 15 + 127) // 255) << 4 | ((a * 15 + 127) // 255)).astype("<u2")

def decode_to_rgba8(m, data):
    """Fallback decoders returning (h*w,4) uint8 RGBA in storage (bottom-up) order, or None."""
    fmt, w, h = m["m_TextureFormat"], m["m_Width"], m["m_Height"]
    import texture2ddecoder as td
    if fmt == UF["ETC_RGB4"]:
        bgra = td.decode_etc1(data, w, h)
    elif fmt == UF["DXT1"]:
        bgra = td.decode_bc1(data, w, h)
    elif fmt == UF["DXT5"]:
        bgra = td.decode_bc3(data, w, h)
    else:
        return None
    a = np.frombuffer(bgra, np.uint8).reshape(-1, 4)
    return a[:, [2, 1, 0, 3]]

def convert_texture(m, opts):
    """m: typetree dict. Returns (blob, aux, info) ; info = dict for stats."""
    fmt, w, h = m["m_TextureFormat"], m["m_Width"], m["m_Height"]
    data = bytes(m["image data"])
    n = w * h
    has_alpha = False
    if fmt == UF["Alpha8"]:
        out_fmt, px = TF_A8, np.frombuffer(data, np.uint8, n); has_alpha = True
    elif fmt == UF["RGB24"]:
        a = np.frombuffer(data, np.uint8, n * 3).reshape(n, 3)
        out_fmt, px = TF_RGB565, _to565(a)
        if opts.get("rgb24") == "rgba8888":
            rgba = np.empty((n, 4), np.uint8); rgba[:, :3] = a; rgba[:, 3] = 255
            out_fmt, px = TF_RGBA8888, rgba
    elif fmt in (UF["RGBA32"], UF["ARGB32"], UF["BGRA32"]):
        a = np.frombuffer(data, np.uint8, n * 4).reshape(n, 4)
        if fmt == UF["ARGB32"]: a = a[:, [1, 2, 3, 0]]
        elif fmt == UF["BGRA32"]: a = a[:, [2, 1, 0, 3]]
        has_alpha = bool((a[:, 3] != 255).any())
        if not has_alpha and opts.get("demote_opaque"):
            out_fmt, px = TF_RGB565, _to565(a[:, :3])
        elif opts.get("rgba32") == "rgba4444":
            out_fmt, px = TF_RGBA4444, _to4444(a)
        else:
            out_fmt, px = TF_RGBA8888, np.ascontiguousarray(a)
    elif fmt == UF["RGB565"]:
        out_fmt, px = TF_RGB565, np.frombuffer(data, "<u2", n)
    elif fmt == UF["RGBA4444"]:
        out_fmt, px = TF_RGBA4444, np.frombuffer(data, "<u2", n)
        has_alpha = bool(((px & 0xF) != 0xF).any())
    elif fmt == UF["ARGB4444"]:
        v = np.frombuffer(data, "<u2", n)
        out_fmt, px = TF_RGBA4444, (((v & 0x0FFF) << 4) | (v >> 12)).astype("<u2")
        has_alpha = bool(((px & 0xF) != 0xF).any())
    else:
        rgba = decode_to_rgba8(m, data)
        if rgba is None:
            raise ValueError("unsupported texture format %d" % fmt)
        if fmt == UF["ETC_RGB4"]:
            out_fmt, px = TF_RGB565, _to565(rgba[:, :3])
        else:
            has_alpha = bool((rgba[:, 3] != 255).any())
            out_fmt, px = TF_RGBA8888, np.ascontiguousarray(rgba)
    pix = np.ascontiguousarray(px).tobytes()
    ts = m.get("m_TextureSettings", {})
    flags = 0
    if (w & (w - 1)) or (h & (h - 1)): flags |= TFL_NPOT
    if has_alpha: flags |= TFL_HAS_ALPHA
    if m.get("m_MipMap"): flags |= TFL_HAD_MIPS
    flags |= TFL_NOT_RESIZED
    wr = B.W()
    wr.u32(B.MAGIC[B.K_TEX]); wr.u32(0)
    wr.u32(out_fmt)
    wr.u16(w); wr.u16(h); wr.u16(w); wr.u16(h)       # stored size, original size
    wr.u32(flags)
    wr.u8(ts.get("m_WrapMode", 0)); wr.u8(ts.get("m_FilterMode", 1)); wr.u8(max(1, ts.get("m_Aniso", 1))); wr.u8(fmt)
    wr.u32(48); wr.u32(len(pix)); wr.u32(1); wr.f32(float(ts.get("m_MipBias", 0.0))); wr.u32(0)
    assert len(wr) == 48
    wr.raw(pix)
    info = dict(fmt=fmt, out=out_fmt, w=w, h=h, alpha=has_alpha)
    return wr.finish(), (w << 16) | h, info

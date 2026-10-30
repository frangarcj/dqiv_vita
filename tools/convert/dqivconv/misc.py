"""TextAsset -> DQTA, Font -> DQFN, AnimationClip -> DQAN."""
import struct
from . import blob as B

def textasset_bytes(raw):
    """Raw TextAsset object bytes -> (name, payload). Layout: str m_Name, bytes m_Script, str m_PathName."""
    n = struct.unpack_from("<i", raw, 0)[0]
    name = raw[4:4 + n].decode("utf-8", "replace")
    p = (4 + n + 3) & ~3
    ln = struct.unpack_from("<i", raw, p)[0]
    return name, raw[p + 4:p + 4 + ln]

def convert_text(raw):
    name, data = textasset_bytes(raw)
    w = B.W()
    w.u32(B.MAGIC[B.K_TEXT]); w.u32(0); w.u32(len(data)); w.u32(0)
    w.raw(data)
    return w.finish(), len(data), dict(size=len(data), name=name)

def convert_font(m, res):
    rects = m.get("m_CharacterRects") or []
    kern = m.get("m_KerningValues") or []
    ttf = bytes(m.get("m_FontData") or b"")
    names = m.get("m_FontNames") or []
    w = B.W()
    w.u32(B.MAGIC[B.K_FONT]); w.u32(0)                                    # 0,4
    w.f32(m["m_FontSize"], m["m_LineSpacing"], m["m_Ascent"], m["m_Kerning"], m["m_PixelScale"])   # 8..27
    w.i32(m["m_CharacterSpacing"]); w.i32(m["m_CharacterPadding"]); w.i32(m["m_ConvertCase"])      # 28..39
    w.i32(m["m_AsciiStartOffset"]); w.i32(m["m_DefaultStyle"]); w.i32(m["m_FontRenderingMode"])   # 40..51
    w.u32(res.ref(m["m_Texture"], B.K_TEX)); w.u32(res.ref(m["m_DefaultMaterial"], B.K_MAT))      # 52,56
    w.u32(len(rects)); w.u32(0)            # 60 rect_count, 64 rects_off
    w.u32(len(kern)); w.u32(0)             # 68 kern_count, 72 kern_off
    w.u32(len(ttf)); w.u32(0)              # 76 ttf_size, 80 ttf_off
    w.str(names[0] if names else "")       # 84 first font name
    w.u32(len(m.get("m_FallbackFonts") or []))   # 88 fallback_count (ids not exported)
    w.u32(0)                               # 92 reserved
    assert len(w) == 96, len(w)
    w.align(4); w.set_u32(64, len(w))
    for r in rects:
        uv, vt = r["uv"], r["vert"]
        w.u32(r["index"]); w.f32(uv["x"], uv["y"], uv["width"], uv["height"], vt["x"], vt["y"], vt["width"], vt["height"], r["width"])
        w.u32(1 if r.get("flipped") else 0)
    w.align(4); w.set_u32(72, len(w))
    for k in kern:
        (a, b), v = (k[0], k[1]) if not isinstance(k, dict) else ((k["first"]["first"], k["first"]["second"]), k["second"])
        w.u16(a); w.u16(b); w.f32(float(v))
    w.align(16); w.set_u32(80, len(w))
    w.raw(ttf)
    return w.finish(), len(ttf), dict(rects=len(rects), ttf=len(ttf))

# ------------------------------------------------------------------ animation
CT_POS, CT_ROT, CT_SCALE, CT_FLOAT = 0, 1, 2, 3
NKEY = {CT_POS: 10, CT_ROT: 13, CT_SCALE: 10, CT_FLOAT: 4}

def _xyz(v): return [v["x"], v["y"], v["z"]]
def _xyzw(v): return [v["x"], v["y"], v["z"], v["w"]]

def convert_anim(m):
    curves = []   # (path, attr, type, classid, flat keys, count, pre, post)
    def add(lst, ctype, getv):
        for c in lst:
            cv = c["curve"]
            keys = []
            for k in cv["m_Curve"]:
                keys += [k["time"], *getv(k["value"]), *getv(k["inSlope"]), *getv(k["outSlope"])]
            curves.append((c["path"], "", ctype, 0, keys, len(cv["m_Curve"]),
                           cv.get("m_PreInfinity", 2), cv.get("m_PostInfinity", 2)))
    add(m.get("m_PositionCurves") or [], CT_POS, _xyz)
    add(m.get("m_RotationCurves") or [], CT_ROT, _xyzw)
    add(m.get("m_ScaleCurves") or [], CT_SCALE, _xyz)
    for c in m.get("m_FloatCurves") or []:
        cv = c["curve"]; keys = []
        for k in cv["m_Curve"]:
            keys += [k["time"], k["value"], k["inSlope"], k["outSlope"]]
        curves.append((c["path"], c.get("attribute", ""), CT_FLOAT, c.get("classID", 0), keys, len(cv["m_Curve"]),
                       cv.get("m_PreInfinity", 2), cv.get("m_PostInfinity", 2)))
    if m.get("m_CompressedRotationCurves"):
        raise ValueError("compressed rotation curves not supported")
    events = m.get("m_Events") or []
    duration = 0.0
    for c in curves:
        if c[5]:
            duration = max(duration, c[4][(c[5] - 1) * NKEY[c[2]]])
    w = B.W()
    w.u32(B.MAGIC[B.K_ANIM]); w.u32(0)                         # 0,4
    w.f32(float(m.get("m_SampleRate", 60.0)), duration)        # 8,12
    w.i32(m.get("m_WrapMode", 0)); w.u32(len(curves))          # 16,20
    w.u32(0); w.u32(len(events)); w.u32(0)                     # 24 curves_off, 28 event_count, 32 events_off
    bd = m.get("m_Bounds") or {"m_Center": {"x": 0, "y": 0, "z": 0}, "m_Extent": {"x": 0, "y": 0, "z": 0}}
    w.f32(*_xyz(bd["m_Center"]), *_xyz(bd["m_Extent"]))        # 36..59
    w.u32(0)                                                   # 60 reserved
    assert len(w) == 64
    w.align(4); w.set_u32(24, len(w))
    keyoff_pos = []
    for (path, attr, ct, cid, keys, n, pre, post) in curves:
        w.str(path); w.str(attr); w.u32(ct); w.u32(cid); w.u32(n)
        keyoff_pos.append(len(w)); w.u32(0)
        w.i32(pre); w.i32(post)                                # 32 bytes per curve
    w.align(4); w.set_u32(32, len(w))
    for e in events:
        w.f32(e["time"]); w.str(e.get("functionName", "")); w.str(e.get("data", ""))
        w.f32(float(e.get("floatParameter", 0.0))); w.i32(e.get("intParameter", 0)); w.u32(0)   # 24 bytes
    for (path, attr, ct, cid, keys, n, pre, post), pos in zip(curves, keyoff_pos):
        w.align(4); w.set_u32(pos, len(w))
        w.f32(*keys)
    return w.finish(), len(curves), dict(curves=len(curves), keys=sum(c[5] for c in curves), events=len(events))

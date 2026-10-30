"""Material -> DQMT blob; Shader -> DQSH blob (+ raw ShaderLab / GLES dumps)."""
import os, re
from . import blob as B

QUEUES = {"background": 1000, "geometry": 2000, "alphatest": 2450, "transparent": 3000, "overlay": 4000}
CMP = {"never": 0, "less": 1, "equal": 2, "lequal": 3, "greater": 4, "notequal": 5, "gequal": 6, "always": 7}
BLEND = {"zero": 0, "one": 1, "srccolor": 2, "srcalpha": 3, "dstcolor": 4, "dstalpha": 5, "oneminussrccolor": 6,
         "oneminussrcalpha": 7, "oneminusdstcolor": 8, "oneminusdstalpha": 9, "srcalphasaturate": 10}
CULL = {"off": 0, "front": 1, "back": 2}

def convert_material(m, res, reg, warn):
    sp = m["m_SavedProperties"]
    shader = res.ref(m["m_Shader"], B.K_SHADER)
    sname = reg.shader_names.get(shader, "")
    bname = ""
    if shader & B.BUILTIN_BIT:
        fn, pid = res.target(m["m_Shader"])
        bname = "builtin:%s:%d" % (fn, pid)
        sname = bname
    texs = [(n["name"], v) for n, v in sp["m_TexEnvs"]]
    floats = [((n["name"] if isinstance(n, dict) else n), v) for n, v in sp["m_Floats"]]
    cols = [(n["name"], v) for n, v in sp["m_Colors"]]
    kws = m.get("m_ShaderKeywords") or []
    if isinstance(kws, str):
        kws = kws.split()
    main_tex = 0
    w = B.W()
    w.u32(B.MAGIC[B.K_MAT]); w.u32(0)
    w.u32(shader if not (shader & B.BUILTIN_BIT) else 0)
    w.i32(m.get("m_CustomRenderQueue", -1))
    w.str(sname)
    base = len(w)
    for _ in range(8): w.u32(0)     # tex_count, tex_off, float_count, float_off, color_count, color_off, kw_count, kw_off
    w.u32(0)                         # main_tex gid (convenience)
    w.u32(0); w.u32(0)               # reserved
    assert len(w) == 64, len(w)
    # texenvs
    w.align(4); w.set_u32(base + 0, len(texs)); w.set_u32(base + 4, len(w))
    for name, v in texs:
        tid = res.ref(v["m_Texture"], B.K_TEX)
        if name == "_MainTex": main_tex = tid
        w.str(name); w.u32(tid); w.f32(v["m_Scale"]["x"], v["m_Scale"]["y"], v["m_Offset"]["x"], v["m_Offset"]["y"])
    w.set_u32(base + 32, main_tex)
    w.align(4); w.set_u32(base + 8, len(floats)); w.set_u32(base + 12, len(w))
    for name, v in floats:
        w.str(name); w.f32(float(v))
    w.align(4); w.set_u32(base + 16, len(cols)); w.set_u32(base + 20, len(w))
    for name, v in cols:
        w.str(name); w.f32(v["r"], v["g"], v["b"], v["a"])
    w.align(4); w.set_u32(base + 24, len(kws)); w.set_u32(base + 28, len(w))
    for k in kws: w.str(k)
    return w.finish(), 0, dict(shader=sname or bname or "?")

# ---------------------------------------------------------------- shaders
def _match_block(s, i):
    """s[i] == '{': return index after the matching '}' (quote aware)."""
    depth, n, inq = 0, len(s), False
    while i < n:
        c = s[i]
        if c == '"': inq = not inq
        elif not inq:
            if c == "{": depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0: return i + 1
        i += 1
    return n

def shader_declared_name(text, fallback):
    mt = re.search(r'Shader\s+"([^"]+)"', text)
    return mt.group(1) if mt else fallback

def _state(txt, st):
    """Parse fixed-function state lines in txt into dict st (in place)."""
    t = re.sub(r'//[^\n]*', '', txt)
    for mt in re.finditer(r'(?m)^\s*(ZWrite|ZTest|Cull|Blend|AlphaTest|ColorMask|Offset)\b([^\n]*)', t):
        k, a = mt.group(1), mt.group(2).strip().replace('"', '').split()
        try:
            if k == "ZWrite": st["zwrite"] = 1 if a[0].lower() == "on" else 0
            elif k == "ZTest": st["ztest"] = CMP.get(a[0].lower(), -1)
            elif k == "Cull": st["cull"] = CULL.get(a[0].lower(), -1)
            elif k == "Blend":
                if a[0].lower() == "off": st["blend"] = (-1, -1)
                else: st["blend"] = (BLEND[a[0].lower()], BLEND[a[1].lower().rstrip(",")])
            elif k == "AlphaTest":
                if a[0].lower() == "off": st["atest"] = (-1, 0.0)
                else: st["atest"] = (CMP[a[0].lower()], float(a[1]) if len(a) > 1 and not a[1].startswith("[") else 0.0)
            elif k == "ColorMask":
                mk = a[0]; st["cmask"] = 0 if mk == "0" else sum({"R": 1, "G": 2, "B": 4, "A": 8}.get(c, 0) for c in mk)
            elif k == "Offset": st["offset"] = (float(a[0].rstrip(",")), float(a[1]))
        except (KeyError, ValueError, IndexError):
            pass

def parse_shader(text):
    """Returns dict(name, queue, fallback, props, passes[list of state dict], gles[list of (kw,vs,fs)])."""
    info = dict(name=shader_declared_name(text, ""), queue=-1, fallback="", props=[], passes=[], gles=[])
    pm = re.search(r'Properties\s*\{', text)
    if pm:
        e = _match_block(text, pm.end() - 1)
        for mt in re.finditer(r'(?m)^\s*(?:\[[^\]]*\]\s*)*(\w+)\s*\(\s*"([^"]*)"\s*,\s*([^)]*?)\)\s*=\s*(.*)$', text[pm.end():e]):
            info["props"].append((mt.group(1), mt.group(3).strip(), mt.group(4).strip()))
    fm = re.search(r'Fallback\s+"([^"]*)"', text)
    if fm: info["fallback"] = fm.group(1)
    sm = re.search(r'SubShader\s*\{', text)
    if sm:
        e = _match_block(text, sm.end() - 1)
        sub = text[sm.end():e]
        # subshader-level state = text before first Pass
        pi = re.search(r'\bPass\s*\{', sub)
        head = sub[:pi.start()] if pi else sub
        tq = re.search(r'"QUEUE"\s*=\s*"(\w+)\s*([+-]\s*\d+)?"', head, re.I)
        if tq:
            info["queue"] = QUEUES.get(tq.group(1).lower(), 2000) + (int(tq.group(2).replace(" ", "")) if tq.group(2) else 0)
        base = {}
        _state(head, base)
        pos = 0
        for pm2 in re.finditer(r'\bPass\s*\{', sub):
            if pm2.start() < pos: continue
            pe = _match_block(sub, pm2.end() - 1)
            pos = pe
            body = sub[pm2.end():pe]
            progs = re.search(r'Program\s+"', body)
            st = dict(base)
            _state(body[:progs.start()] if progs else body, st)
            info["passes"].append(st)
    vp_pos = [m_.start() for m_ in re.finditer(r'Program\s+"vp"', text)]
    for mt in re.finditer(r'SubProgram\s+"gles\s*"\s*\{\s*(?:Keywords\s*\{([^}]*)\}\s*)?"!!GLES(.*?)"\s*\}', text, re.S):
        src = mt.group(2)
        kw = " ".join(re.findall(r'"([^"]*)"', mt.group(1) or ""))
        vs = fs = ""
        a = src.find("#ifdef VERTEX"); b = src.find("#ifdef FRAGMENT")
        if a >= 0: vs = src[a + len("#ifdef VERTEX"): b if b > a else len(src)]
        if b >= 0: fs = src[b + len("#ifdef FRAGMENT"):]
        if a < 0 and b < 0:
            continue          # stub variant (e.g. the "fp" program that only repeats the "vp" source)
        pre = src[:a] if a > 0 else ""
        npass = sum(1 for p_ in vp_pos if p_ < mt.start())
        info["gles"].append((npass - 1, kw, pre.strip(), vs.replace("#endif", "").strip(), fs.replace("#endif", "").strip()))
    return info

def safe_name(n):
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", n)

def convert_shader(m, shader_dir, warn):
    text = m.get("m_Script") or ""
    if isinstance(text, (bytes, bytearray)): text = bytes(text).decode("utf-8", "replace")
    info = parse_shader(text)
    name = info["name"] or m["m_Name"]
    fname = safe_name(name)
    dump = ""
    if shader_dir:
        os.makedirs(shader_dir, exist_ok=True)
        # several asset objects may share a name; disambiguate by content length
        p = os.path.join(shader_dir, fname + ".txt")
        n = 1
        while os.path.exists(p) and open(p, encoding="utf-8").read() != text:
            n += 1; p = os.path.join(shader_dir, "%s.%d.txt" % (fname, n))
        with open(p, "w", encoding="utf-8") as f: f.write(text)
        dump = os.path.basename(p)
        stem = dump[:-4]
        seen_pass = {}
        for (ps, kw, pre, vs, fs) in info["gles"]:
            vi = seen_pass[ps] = seen_pass.get(ps, -1) + 1
            base = "%s.p%d.v%d" % (stem, ps, vi)
            with open(os.path.join(shader_dir, base + ".vert"), "w") as f:
                f.write("// keywords: %s\n%s\n%s\n" % (kw, pre, vs))
            with open(os.path.join(shader_dir, base + ".frag"), "w") as f:
                f.write("// keywords: %s\n%s\n%s\n" % (kw, pre, fs))
    w = B.W()
    w.u32(B.MAGIC[B.K_SHADER]); w.u32(0)
    w.str(name); w.i32(info["queue"]); w.str(dump); w.str(info["fallback"])
    w.u32(len(info["passes"])); w.u32(len(info["props"])); w.u32(len(info["gles"]))
    po = len(w); w.u32(0); w.u32(0)                       # passes_off, props_off
    w.u32(0)                                              # reserved
    assert len(w) == 48, len(w)
    w.align(4); w.set_u32(po, len(w))
    for st in info["passes"]:
        bs, bd = st.get("blend", (-1, -1)); at, ar = st.get("atest", (-1, 0.0)); of = st.get("offset", (0.0, 0.0))
        w.i32(st.get("zwrite", -1)); w.i32(st.get("ztest", -1)); w.i32(st.get("cull", -1))
        w.i32(bs); w.i32(bd); w.i32(at); w.f32(ar); w.i32(st.get("cmask", 15)); w.f32(*of)   # 40 bytes
    w.align(4); w.set_u32(po + 4, len(w))
    for (pn, pt, pd) in info["props"]:
        w.str(pn); w.str(pt); w.str(pd)
    return w.finish(), 0, dict(name=name, passes=len(info["passes"]), gles=len(info["gles"]))

"""GameObject hierarchies (prefabs and scenes) -> DQPF blob."""
import struct
from . import blob as B

# component types == Unity class ids
C_TRANSFORM, C_CAMERA, C_MESHRENDERER, C_MESHFILTER, C_ANIMATION, C_MONOBEHAVIOUR, C_SKINNED = 4, 20, 23, 33, 111, 114, 137
# property types
P_INT, P_FLOAT, P_BOOL, P_STR, P_VEC2, P_VEC3, P_VEC4, P_ASSET, P_LOCAL = range(9)

SKIP_COMPS = {95}      # Animator (FBX importer default; the game uses the legacy Animation component)
HDR = 64
NODE = 80
COMP = 16

class Props:
    """Flattened (name, type, values) list for generic components."""
    def __init__(self, bld):
        self.bld = bld
        self.items = []
    def add(self, name, ptype, vals):
        self.items.append((name, ptype, vals))

    def pptr(self, name, pp):
        node = self.bld.local_ref(pp)
        if node is not None:
            self.add(name, P_LOCAL, [node[0], node[1], 0, 0])
        else:
            self.add(name, P_ASSET, [self.bld.res.ref(pp), 0, 0, 0])

    def flatten(self, prefix, v, depth=0):
        if depth > 6:
            return
        if isinstance(v, bool):
            self.add(prefix, P_BOOL, [int(v)])
        elif isinstance(v, int):
            self.add(prefix, P_INT, [v])
        elif isinstance(v, float):
            self.add(prefix, P_FLOAT, [v])
        elif isinstance(v, str):
            self.add(prefix, P_STR, [v])
        elif isinstance(v, (bytes, bytearray)):
            self.add(prefix + ".bytes", P_INT, [len(v)])
        elif isinstance(v, dict):
            k = set(v)
            if k == {"m_FileID", "m_PathID"}:
                self.pptr(prefix, v)
            elif k == {"x", "y", "z", "w"}:
                self.add(prefix, P_VEC4, [v["x"], v["y"], v["z"], v["w"]])
            elif k == {"x", "y", "z"}:
                self.add(prefix, P_VEC3, [v["x"], v["y"], v["z"]])
            elif k == {"x", "y"}:
                self.add(prefix, P_VEC2, [v["x"], v["y"]])
            elif k == {"r", "g", "b", "a"}:
                self.add(prefix, P_VEC4, [v["r"], v["g"], v["b"], v["a"]])
            elif k == {"x", "y", "width", "height"}:
                self.add(prefix, P_VEC4, [v["x"], v["y"], v["width"], v["height"]])
            else:
                for kk, vv in v.items():
                    if kk in ("m_GameObject",):
                        continue
                    self.flatten((prefix + "." if prefix else "") + kk, vv, depth + 1)
        elif isinstance(v, (list, tuple)):
            self.add(prefix + ".size", P_INT, [len(v)])
            for i, e in enumerate(v[:64]):
                self.flatten("%s[%d]" % (prefix, i), e, depth + 1)

def write_props(w, class_name, props):
    w.str(class_name)
    w.u32(len(props.items))
    for name, t, vals in props.items:
        w.str(name); w.u32(t)
        if t == P_STR:
            w.str(vals[0]); w.zeros(12)
        elif t in (P_FLOAT, P_VEC2, P_VEC3, P_VEC4):
            vv = [float(x) for x in vals] + [0.0] * (4 - len(vals))
            w.f32(*vv)
        else:
            vv = [int(x) & 0xFFFFFFFF for x in vals] + [0] * (4 - len(vals))
            for x in vv: w.u32(x)

# ------------------------------------------------------------------ known MonoBehaviour layouts (no typetree in the files)
MB_SCHEMAS = {
    "MainLoop": [],
    "TouchPad": [("m_siColDebug", "pptr"), ("m_siColMove", "pptr"), ("m_siMovePad", "pptr"),
                 ("m_siMoveCurNormal", "pptr"), ("m_siMoveCurBS", "pptr"), ("m_siMoveCurLight", "pptr"),
                 ("m_siMoveDirs", "pptr"), ("m_siMoveDirLight", "pptr"), ("m_siMoveDirNormal", "pptr"),
                 ("l_state", "int")],        # public ePadState l_state (DQ4 serializes it; DQ5's data had no such field)
    # DQ4 declaration order (DQ5 had m_quitmsg last)
    "Boot": [("m_logoTexture", "pptr[]"), ("m_logoShader", "pptr"), ("m_quitmsg", "pptr"), ("m_obbMenuParts", "pptr"),
             ("m_obbFont", "pptr[]"), ("m_obbMat", "pptr[]"), ("m_obbmsg", "pptr")],
}

def parse_mb_fields(raw, schema):
    """Decode serialized MonoBehaviour fields for the known schemas. Returns (list[(name, value)], leftover)."""
    p = 8 + 4 + 8                                  # m_GameObject, m_Enabled(+pad), m_Script
    n = struct.unpack_from("<i", raw, p)[0]
    p = p + 4 + ((n + 3) & ~3)                     # m_Name
    def pptr(p):
        f, i = struct.unpack_from("<ii", raw, p)
        return {"m_FileID": f, "m_PathID": i}, p + 8
    out = []
    for name, kind in schema:
        if kind == "pptr":
            v, p = pptr(p); out.append((name, v))
        elif kind == "int":
            out.append((name, struct.unpack_from("<i", raw, p)[0])); p += 4
        elif kind == "pptr[]":
            cnt = struct.unpack_from("<i", raw, p)[0]; p += 4
            arr = []
            for _ in range(cnt):
                v, p = pptr(p); arr.append(v)
            out.append((name, arr))
    return out, len(raw) - p

class Builder:
    def __init__(self, ctx, sf, fname, res):
        self.ctx, self.sf, self.fname, self.res = ctx, sf, fname, res
        self.objs = sf.objects
        self.nodes = []        # dict per node
        self.tmap = {}         # transform pid -> node idx
        self.local = {}        # component pid -> (node idx, class id)
        self._tt = {}

    def tt(self, pid):
        t = self._tt.get(pid)
        if t is None:
            t = self._tt[pid] = self.objs[pid].read_typetree()
        return t

    def local_ref(self, pp):
        if pp["m_FileID"] == 0 and pp["m_PathID"] in self.local:
            return self.local[pp["m_PathID"]]
        return None

    # ---- pass 1: hierarchy
    def visit(self, go_pid, parent):
        go = self.tt(go_pid)
        comps = [(cid, pp["m_PathID"]) for cid, pp in go["m_Component"]]
        tpid = next((p for c, p in comps if c == C_TRANSFORM), None)
        if tpid is None:
            raise ValueError("GameObject %d has no Transform" % go_pid)
        tr = self.tt(tpid)
        idx = len(self.nodes)
        n = dict(go_pid=go_pid, name=go["m_Name"], parent=parent, layer=go["m_Layer"], tag=go["m_Tag"],
                 active=1 if go["m_IsActive"] else 0, tr=tr,
                 comps=[(c, p) for c, p in comps if c != C_TRANSFORM and c not in SKIP_COMPS], first_child=-1, next=-1, children=[])
        self.nodes.append(n)
        self.tmap[tpid] = idx
        self.local[tpid] = (idx, C_TRANSFORM)
        for c, p in n["comps"]:
            self.local[p] = (idx, c)
        prev = -1
        for ch in tr["m_Children"]:
            if ch["m_FileID"] != 0 or ch["m_PathID"] == 0:
                continue
            ctr = self.tt(ch["m_PathID"])
            cgo = ctr["m_GameObject"]["m_PathID"]
            ci = self.visit(cgo, idx)
            if prev < 0: n["first_child"] = ci
            else: self.nodes[prev]["next"] = ci
            prev = ci
        return idx

    # ---- component payloads
    def comp_payload(self, w, cid, pid):
        """Write the payload of component at w's current (4-aligned) position. Returns enabled flag."""
        o = self.objs[pid]
        res = self.res
        if cid == C_MONOBEHAVIOUR:
            return self.mono(w, o)
        try:
            t = self.tt(pid)
        except Exception as e:
            self.ctx.warn("component class %d pid %d unreadable: %s" % (cid, pid, str(e)[:80]))
            props = Props(self)
            props.add("raw_hex", P_STR, [o.get_raw_data().hex()])
            write_props(w, o.type.name, props)
            return 1
        en = 1 if t.get("m_Enabled", 1) else 0
        if cid == C_MESHFILTER:
            w.u32(res.ref(t["m_Mesh"], B.K_MESH))
        elif cid == C_MESHRENDERER:
            w.u32((1 if t.get("m_CastShadows") else 0) | (2 if t.get("m_ReceiveShadows") else 0))
            w.i32(t.get("m_LightmapIndex", 255)); w.i32(t.get("m_SortingOrder", 0))
            mats = t["m_Materials"]
            w.u32(len(mats))
            for m in mats: w.u32(res.ref(m, B.K_MAT))
        elif cid == C_SKINNED:
            w.u32((1 if t.get("m_CastShadows") else 0) | (2 if t.get("m_ReceiveShadows") else 0) |
                  (4 if t.get("m_UpdateWhenOffscreen") else 0))
            w.u32(res.ref(t["m_Mesh"], B.K_MESH))
            rb = t["m_RootBone"]
            w.i32(self.tmap.get(rb["m_PathID"], -1) if rb["m_FileID"] == 0 else -1)
            mats, bones = t["m_Materials"], t["m_Bones"]
            w.u32(len(mats)); w.u32(len(bones))
            ab = t["m_AABB"]
            w.f32(ab["m_Center"]["x"], ab["m_Center"]["y"], ab["m_Center"]["z"],
                  ab["m_Extent"]["x"], ab["m_Extent"]["y"], ab["m_Extent"]["z"])
            w.i32(t.get("m_Quality", 0))
            for m in mats: w.u32(res.ref(m, B.K_MAT))
            for b in bones: w.i32(self.tmap.get(b["m_PathID"], -1) if b["m_FileID"] == 0 else -1)
        elif cid == C_ANIMATION:
            clips = t.get("m_Animations") or []
            w.i32(t.get("m_WrapMode", 0)); w.u32(1 if t.get("m_PlayAutomatically") else 0)
            w.i32(t.get("m_CullingType", 0)); w.u32(res.ref(t["m_Animation"], B.K_ANIM)); w.u32(len(clips))
            for c in clips: w.u32(res.ref(c, B.K_ANIM))
        elif cid == C_CAMERA:
            bg, vp = t["m_BackGroundColor"], t["m_NormalizedViewPortRect"]
            w.u32(t["m_ClearFlags"]); w.f32(bg["r"], bg["g"], bg["b"], bg["a"])
            w.f32(vp["x"], vp["y"], vp["width"], vp["height"])
            w.f32(t["near clip plane"], t["far clip plane"], t["field of view"], t["orthographic size"])
            w.u32(1 if t["orthographic"] else 0); w.f32(t["m_Depth"])
            w.u32(t["m_CullingMask"]["m_Bits"]); w.i32(t.get("m_RenderingPath", -1))
            w.u32(res.ref(t["m_TargetTexture"], B.K_TEX)); w.u32(1 if t.get("m_HDR") else 0)
            w.u32(1 if t.get("m_OcclusionCulling") else 0)
        else:
            props = Props(self)
            for k, v in t.items():
                if k in ("m_GameObject", "m_Enabled"):
                    continue
                props.flatten(k, v)
            write_props(w, o.type.name, props)
        return en

    def mono(self, w, o):
        raw = o.get_raw_data()
        # typetree of custom scripts is not stored in the files: read the fixed MonoBehaviour prefix by hand
        sfid, spid = struct.unpack_from("<ii", raw, 12)
        script = {"m_FileID": sfid, "m_PathID": spid}
        t = {"m_Enabled": raw[8]}
        cname = "?"
        tgt = self.res.target(script)
        if tgt[0] is not None:
            so = self.ctx.get_obj(*tgt)
            if so is not None and so.type.name == "MonoScript":
                cname = so.read_typetree().get("m_ClassName", "?")
        props = Props(self)
        schema = MB_SCHEMAS.get(cname)
        try:
            if schema is None:
                raise ValueError("no schema for MonoBehaviour class %s" % cname)
            fields, left = parse_mb_fields(raw, schema)
            if left:
                self.ctx.warn("MonoBehaviour %s: %d leftover bytes" % (cname, left))
                props.add("raw_hex", P_STR, [raw.hex()])
            for name, v in fields:
                if isinstance(v, int):
                    props.add(name, P_INT, [v])
                elif isinstance(v, list):
                    props.add(name + ".size", P_INT, [len(v)])
                    for i, e in enumerate(v): props.pptr("%s[%d]" % (name, i), e)
                else:
                    props.pptr(name, v)
        except Exception as e:
            self.ctx.warn("MonoBehaviour %s fields unreadable (%s); raw hex kept" % (cname, str(e)[:60]))
            props.add("raw_hex", P_STR, [raw.hex()])
        write_props(w, cname, props)
        return 1 if t.get("m_Enabled", 1) else 0

    def build(self, root_go_pids, is_scene):
        for g in root_go_pids:
            self.visit(g, -1)
        # sibling chain between roots
        roots = [i for i, n in enumerate(self.nodes) if n["parent"] < 0]
        for a, b in zip(roots, roots[1:]):
            self.nodes[a]["next"] = b
        comps_total = sum(len(n["comps"]) for n in self.nodes)
        w = B.W()
        w.u32(B.MAGIC[B.K_PREFAB]); w.u32(0)
        w.u32(len(self.nodes)); w.u32(comps_total)
        w.u32(HDR); w.u32(HDR + NODE * len(self.nodes))
        w.u32(len(roots)); w.u32(1 if is_scene else 0)
        w.zeros(HDR - len(w))
        cf = 0
        for n in self.nodes:
            tr = n["tr"]
            lp, lr, ls = tr["m_LocalPosition"], tr["m_LocalRotation"], tr["m_LocalScale"]
            w.str(n["name"]); w.i32(n["parent"]); w.i32(n["first_child"]); w.i32(n["next"])
            w.f32(lp["x"], lp["y"], lp["z"], lr["x"], lr["y"], lr["z"], lr["w"], ls["x"], ls["y"], ls["z"])
            w.u8(n["layer"]); w.u8(n["active"]); w.u16(n["tag"])
            w.u32(cf); w.u32(len(n["comps"])); w.u32(n["go_pid"]); w.u32(0); w.u32(0)
            cf += len(n["comps"])
            assert (len(w) - HDR) % NODE == 0
        comps_off = len(w)
        w.zeros(COMP * comps_total)
        ci = 0
        for n in self.nodes:
            for cid, pid in n["comps"]:
                w.align(4)
                off = len(w)
                en = self.comp_payload(w, cid, pid)
                w.align(4)
                rec = comps_off + ci * COMP
                struct.pack_into("<IIII", w.b, rec, cid, en, off, len(w) - off)
                ci += 1
        return w.finish(), len(self.nodes), dict(nodes=len(self.nodes), comps=comps_total)

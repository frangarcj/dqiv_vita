#!/usr/bin/env python
"""DQIV offline asset converter: Unity 4.7.2 serialized files -> DQIV pack (see FORMAT.md).

  /Users/frangar/Fun/dqiv/work/tools/venv/bin/python -I dqiv_convert.py [--out DIR] [--jobs N] ...

Pipeline
  1. read the ResourceManager container of mainData (Resources.Load path -> PPtr into the GUID files)
  2. enumerate every serialized file, assign a global object id (gid) to each exportable asset
  3. convert files in parallel (each worker emits final blobs with gid cross references)
  4. main process dedupes by content hash, lays blobs out sorted by Resources path (locality),
     writes meta.pak/mesh.pak/tex.pak/blob.pak + index.bin + index.txt + stats
"""
import argparse, collections, hashlib, json, multiprocessing as mp, os, struct, sys, time, traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import UnityPy
from dqivconv import blob as B
from dqivconv import unityenv as U
from dqivconv.tex import convert_texture
from dqivconv.mesh import convert_mesh
from dqivconv.material import convert_material, convert_shader, parse_shader
from dqivconv.misc import convert_text, convert_font, convert_anim
from dqivconv.prefab import Builder

G = {}   # per-worker globals: reg, opts (fork: inherited; spawn (Windows): set by _init_worker)

def _init_worker(reg, opts):
    G["reg"], G["opts"] = reg, opts

# --------------------------------------------------------------------------- stage 1/2
def enum_file(name):
    try:
        sf = U.load_sf(name)
    except Exception as e:
        return name, None, [], str(e)
    objs = []
    for pid, o in sf.objects.items():
        cn = o.type.name
        extra = None
        if cn == "Shader":
            try:
                t = o.read_typetree()
                extra = parse_shader_name(t["m_Script"], t["m_Name"])
            except Exception:
                extra = None
        objs.append((pid, cn, extra))
    return name, objs, U.ext_names(sf), None

def parse_shader_name(text, fallback):
    from dqivconv.material import shader_declared_name
    if isinstance(text, (bytes, bytearray)): text = bytes(text).decode("utf-8", "replace")
    return shader_declared_name(text, fallback)

def load_container():
    env = UnityPy.load(U.file_path("mainData"))
    sf = list(env.files.values())[0]
    names = U.ext_names(sf)
    for o in sf.objects.values():
        if o.type.name == "ResourceManager":
            t = o.read_typetree()
            out = []
            for key, pp in t["m_Container"]:
                fid, pid = pp["m_FileID"], pp["m_PathID"]
                fname = "mainData" if fid == 0 else names[fid - 1]
                out.append((key, fname, pid))
            return out
    raise RuntimeError("no ResourceManager in mainData")

# --------------------------------------------------------------------------- stage 3
class Ctx:
    def __init__(self, fname, sf, res):
        self.fname, self.sf, self.res = fname, sf, res
        self.msgs = []
        self._others = {}
    def warn(self, m): self.msgs.append(("warn", self.fname, m))
    def get_obj(self, fname, pid):
        if fname == self.fname:
            return self.sf.objects.get(pid)
        sf = self._others.get(fname)
        if sf is None:
            try: sf = self._others[fname] = U.load_sf(fname)
            except Exception: return None
        return sf.objects.get(pid)

def convert_file(fname):
    reg, opts = G["reg"], G["opts"]
    out, msgs, fails = [], [], []
    try:
        sf = U.load_sf(fname)
    except Exception as e:
        return [], [], [("*", 0, "load: " + str(e))]
    res = U.Resolver(reg, fname, sf)
    ctx = Ctx(fname, sf, res)
    for pid, gid, kind in reg.by_file.get(fname, []):
        o = sf.objects.get(pid) if pid else None
        name, src = "", 0
        res.unresolved = []
        try:
            if kind == B.K_TEXT:
                blob, aux, info = convert_text(o.get_raw_data()); name = info["name"]; src = o.byte_size
            elif kind == B.K_PREFAB:
                if pid:
                    bl = Builder(ctx, sf, fname, res)
                    blob, aux, info = bl.build([pid], False)
                    name = bl.nodes[0]["name"] if bl.nodes else ""
                    src = 0
                else:   # scene: every root GameObject
                    roots = []
                    for p2, o2 in sorted(sf.objects.items()):
                        if o2.type.name == "GameObject":
                            go = o2.read_typetree()
                            tp = next(pp["m_PathID"] for c, pp in go["m_Component"] if c == 4)
                            if sf.objects[tp].read_typetree()["m_Father"]["m_PathID"] == 0:
                                roots.append(p2)
                    bl = Builder(ctx, sf, fname, res)
                    blob, aux, info = bl.build(roots, True)
                    name = fname
                    src = sum(o2.byte_size for o2 in sf.objects.values())
            else:
                m = o.read_typetree(); src = o.byte_size; name = m.get("m_Name", "")
                if kind == B.K_TEX: blob, aux, info = convert_texture(m, opts)
                elif kind == B.K_MESH: blob, aux, info = convert_mesh(m, opts)
                elif kind == B.K_MAT: blob, aux, info = convert_material(m, res, reg, lambda s: ctx.warn(s))
                elif kind == B.K_SHADER: blob, aux, info = convert_shader(m, opts["shader_dir"], lambda s: ctx.warn(s))
                elif kind == B.K_FONT: blob, aux, info = convert_font(m, res)
                elif kind == B.K_ANIM: blob, aux, info = convert_anim(m)
                else: raise ValueError("kind")
            for u in res.unresolved:
                ctx.warn("unresolved ref: " + u)
            out.append((gid, kind, name, aux, blob, src, hashlib.sha1(blob).digest(), info))
        except Exception as e:
            fails.append((fname, pid, "%s: %s" % (type(e).__name__, str(e)[:200])))
            if opts.get("verbose"):
                traceback.print_exc()
    return out, ctx.msgs, fails

# --------------------------------------------------------------------------- stage 4
class StrPool:
    def __init__(self):
        self.b = bytearray(b"\0"); self.d = {"": 0}
    def add(self, s):
        o = self.d.get(s)
        if o is None:
            o = self.d[s] = len(self.b)
            self.b += s.encode("utf-8", "replace") + b"\0"
        return o

def write_pack(outdir, reg, results, container_entries, fails_gids, t0):
    os.makedirs(outdir, exist_ok=True)
    n = len(reg.kind) - 1
    # resource entries: (path, gid)
    res_entries = []
    for key, fname, pid in container_entries:
        g = reg.ids.get((fname, pid))
        if g is not None:
            res_entries.append((key, g))
    for sname, g in reg.scene_gid.items():
        res_entries.append(("scene/" + sname.lower(), g))
    res_entries.sort(key=lambda e: (e[0].encode("utf-8"), reg.kind[e[1]], e[1]))
    first_pos = {}
    for i, (k, g) in enumerate(res_entries):
        first_pos.setdefault(g, i)
    order = sorted(results.keys(), key=lambda g: (first_pos.get(g, 1 << 30), g))
    files = [open(os.path.join(outdir, nm), "wb") for nm in B.DF_NAMES]
    sizes = [0] * len(files)
    seen = {}
    ent = {}
    out_bytes = collections.Counter(); dedup_saved = collections.Counter()
    for g in order:
        kind, name, aux, blob, src, dig, info = results[g]
        df = B.KIND_DF[kind]
        key = (df, dig)
        off = seen.get(key)
        if off is None:
            pad = (-sizes[df]) % 16
            if pad: files[df].write(bytes(pad)); sizes[df] += pad
            off = sizes[df]
            files[df].write(blob); sizes[df] += len(blob)
            seen[key] = off
            out_bytes[kind] += len(blob)
        else:
            dedup_saved[kind] += len(blob)
        ent[g] = (kind, df, off, len(blob), name, aux)
    for f in files: f.close()
    sp = StrPool()
    objs = bytearray(struct.pack("<6I", 0, 0, 0, 0, 0, 0))
    for g in range(1, n + 1):
        e = ent.get(g)
        if e is None:      # failed / not converted: keep slot, size 0
            objs += struct.pack("<6I", reg.kind[g], 0, 0, 0, sp.add(""), 0)
        else:
            kind, df, off, size, name, aux = e
            objs += struct.pack("<6I", kind, df, off, size, sp.add(name), aux)
    resb = bytearray()
    for key, g in res_entries:
        resb += struct.pack("<4I", sp.add(key), g, reg.kind[g], 0)
    dfn = [sp.add(nm) for nm in B.DF_NAMES]
    HDR = 128
    objs_off = HDR
    res_off = objs_off + len(objs)
    str_off = res_off + len(resb)
    hdr = bytearray(struct.pack("<4sIIIIIII", b"DQIVP", 1, n + 1, len(res_entries), objs_off, res_off, str_off, len(sp.b)))
    hdr += struct.pack("<II", len(B.DF_NAMES), 0)
    hdr += bytes(64 - len(hdr))
    for i in range(8):
        hdr += struct.pack("<II", dfn[i] if i < len(dfn) else 0, sizes[i] if i < len(sizes) else 0)
    assert len(hdr) == HDR
    with open(os.path.join(outdir, "index.bin"), "wb") as f:
        f.write(hdr); f.write(objs); f.write(resb); f.write(sp.b)
    with open(os.path.join(outdir, "index.txt"), "w", encoding="utf-8") as f:
        f.write("# path\tkind\tid\tsize\tdatafile:offset\taux\n")
        for key, g in res_entries:
            e = ent.get(g)
            if e: f.write("%s\t%s\t%d\t%d\t%s:%d\t%d\n" % (key, B.KIND_NAMES[e[0]], g, e[3], B.DF_NAMES[e[1]], e[2], e[5]))
            else: f.write("%s\t%s\t%d\tFAILED\n" % (key, B.KIND_NAMES[reg.kind[g]], g))
    return res_entries, ent, out_bytes, dedup_saved, sizes

def dir_size(p):
    t = 0
    for fn in os.listdir(p):
        fp = os.path.join(p, fn)
        if os.path.isfile(fp): t += os.path.getsize(fp)
    return t

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=os.path.join(os.environ.get("DQIV_UNITY_ROOT", "/Users/frangar/Fun/dqiv/work/extract/unity"), "..", "pack"))
    ap.add_argument("--shaders-out", default=os.path.join(os.environ.get("DQIV_UNITY_ROOT", "/Users/frangar/Fun/dqiv/work/extract/unity"), "..", "pack_shaders"))
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--rgb24", choices=["rgb565", "rgba8888"], default="rgb565",
                    help="output format for Unity RGB24 textures (default rgb565)")
    ap.add_argument("--rgba32", choices=["rgba8888", "rgba4444"], default="rgba8888",
                    help="output format for Unity RGBA32 textures (default rgba8888)")
    ap.add_argument("--demote-opaque", action="store_true",
                    help="store RGBA32 textures whose alpha is all 255 as RGB565 (saves RAM; default off)")
    ap.add_argument("--limit-files", type=int, default=0, help="debug: only convert the first N obb files")
    ap.add_argument("--only-prefix", default="", help="debug: only convert files holding Resources paths with this prefix")
    ap.add_argument("-v", "--verbose", action="store_true")
    a = ap.parse_args()
    t0 = time.time()
    opts = dict(rgb24=a.rgb24, rgba32=a.rgba32, demote_opaque=a.demote_opaque, shader_dir=a.shaders_out, verbose=a.verbose)

    print("[1/4] reading Resources container ...", flush=True)
    container = load_container()
    print("      %d container entries, %d unique paths" % (len(container), len({c[0] for c in container})))

    print("[2/4] enumerating serialized files ...", flush=True)
    names = U.list_files()
    reg = U.Registry()
    reg.files = set(names)
    classes = {}
    ctx_ = mp.get_context("fork" if hasattr(os, "fork") else "spawn")
    with ctx_.Pool(a.jobs) as pool:
        for name, objs, ext, err in pool.imap(enum_file, names, chunksize=64):
            if err:
                print("      cannot load %s: %s" % (name, err)); continue
            classes[name] = {pid: cn for pid, cn, _ in objs}
            for pid, cn, extra in sorted(objs):
                k = U.EXPORT_KIND.get(cn)
                if k:
                    g = reg.add(name, pid, k)
                    if k == B.K_SHADER: reg.shader_names[g] = extra or ""
    skipped = collections.Counter()
    for fname, objs in classes.items():
        for pid, cn in objs.items():
            if cn in U.SKIP_CLASSES: skipped[cn] += 1
    # prefab roots: GameObjects named by the container; scenes
    bad_container = []
    for key, fname, pid in container:
        cn = classes.get(fname, {}).get(pid)
        if cn is None:
            bad_container.append((key, fname, pid, "object missing"))
        elif cn == "GameObject":
            reg.add(fname, pid, B.K_PREFAB)
    for s in U.SCENE_FILES:
        reg.scene_gid[s] = reg.add(s, 0, B.K_PREFAB)
    cnt = collections.Counter(B.KIND_NAMES[k] for k in reg.kind[1:])
    print("      %d files, %d exportable objects: %s" % (len(names), len(reg.kind) - 1, dict(cnt)))

    work = [n for n in names if n in reg.by_file]
    if a.only_prefix:
        want = {f for k, f, p in container if k.startswith(a.only_prefix.lower())} | set(U.SHARED_FILES) | set(U.SCENE_FILES)
        work = [n for n in work if n in want]
        # referenced externals still need to be converted for completeness only if requested
    if a.limit_files:
        work = [n for n in work if n in U.TOPLEVEL] + [n for n in work if n not in U.TOPLEVEL][:a.limit_files]
    G["reg"], G["opts"] = reg, opts
    if os.path.isdir(a.shaders_out):
        for fn in os.listdir(a.shaders_out):
            fp = os.path.join(a.shaders_out, fn)
            if os.path.isfile(fp): os.remove(fp)

    print("[3/4] converting %d files with %d workers ..." % (len(work), a.jobs), flush=True)
    results, msgs, fails = {}, [], []
    done = 0
    stat_src = collections.Counter(); stat_n = collections.Counter(); infos = collections.defaultdict(collections.Counter)
    t1 = time.time()
    with ctx_.Pool(a.jobs, initializer=_init_worker, initargs=(reg, opts)) as pool:
        for out, ms, fl in pool.imap_unordered(convert_file, sorted(work, key=lambda n: -(os.path.getsize(U.file_path(n)))), chunksize=8):
            for (gid, kind, name, aux, blob, src, dig, info) in out:
                results[gid] = (kind, name, aux, blob, src, dig, info)
                stat_src[kind] += src; stat_n[kind] += 1
                if kind == B.K_TEX:
                    infos["tex_in_fmt"][info["fmt"]] += 1; infos["tex_out_fmt"][info["out"]] += 1
                    infos["tex_misc"]["max_dim"] = max(infos["tex_misc"]["max_dim"], info["w"], info["h"])
                    if (info["w"] & (info["w"] - 1)) or (info["h"] & (info["h"] - 1)): infos["tex_misc"]["npot"] += 1
                    if info["w"] == 0 or info["h"] == 0: infos["tex_misc"]["empty_0x0"] += 1
                    if info["fmt"] in (4, 5, 14) and not info["alpha"]:
                        infos["tex_misc"]["rgba32_fully_opaque"] += 1
                        infos["tex_misc"]["rgba32_fully_opaque_bytes"] += info["w"] * info["h"] * 4
                if kind == B.K_MESH:
                    infos["mesh_misc"]["vertices"] += info["verts"]; infos["mesh_misc"]["indices"] += info["indices"]
                    infos["mesh_misc"]["skinned_meshes"] += 1 if info["skinned"] else 0
                if kind == B.K_MAT: infos["material_shader"][info["shader"]] += 1
            msgs += ms; fails += fl
            done += 1
            if done % 2000 == 0:
                print("      %d/%d files, %.0fs" % (done, len(work), time.time() - t1), flush=True)
    print("      converted %d objects in %.0fs; %d failures" % (len(results), time.time() - t1, len(fails)))

    print("[4/4] writing pack to %s ..." % a.out, flush=True)
    res_entries, ent, out_bytes, dedup_saved, sizes = write_pack(a.out, reg, results, container, set(), t0)

    # ---- reports
    src_total_files = sum(os.path.getsize(U.file_path(n)) for n in work)
    out_total = dir_size(a.out)
    wc = collections.Counter(m for _, _, m in msgs)
    unres = collections.Counter(m.split(" (")[0][:60] for m in wc.elements() if m.startswith("unresolved"))
    stats = dict(
        seconds=round(time.time() - t0, 1), files_converted=len(work), objects_converted=len(results),
        objects_failed=len(fails), resource_entries=len(res_entries),
        unique_resource_paths=len({k for k, _ in res_entries}),
        per_kind_count={B.KIND_NAMES[k]: v for k, v in sorted(stat_n.items())},
        per_kind_src_bytes={B.KIND_NAMES[k]: v for k, v in sorted(stat_src.items())},
        per_kind_pack_bytes={B.KIND_NAMES[k]: v for k, v in sorted(out_bytes.items())},
        per_kind_dedup_saved_bytes={B.KIND_NAMES[k]: v for k, v in sorted(dedup_saved.items())},
        source_files_bytes=src_total_files, pack_bytes=out_total,
        data_file_sizes=dict(zip(B.DF_NAMES, sizes)),
        skipped_classes={k: dict(count=v, why=U.SKIP_CLASSES[k]) for k, v in skipped.items()},
        container_bad_entries=len(bad_container),
        tex_misc=dict(infos["tex_misc"]), mesh_misc=dict(infos["mesh_misc"]),
        tex_input_formats=dict(infos["tex_in_fmt"]), tex_output_formats=dict(infos["tex_out_fmt"]),
        material_shaders=dict(infos["material_shader"]),
        warnings=dict(collections.Counter(re_norm(m) for _, _, m in msgs).most_common(40)),
    )
    with open(os.path.join(a.out, "stats.json"), "w") as f: json.dump(stats, f, indent=1)
    with open(os.path.join(a.out, "failures.txt"), "w") as f:
        for fn, pid, why in fails: f.write("%s\t%d\t%s\n" % (fn, pid, why))
        for k, fn, pid, why in bad_container: f.write("container:%s\t%s:%d\t%s\n" % (k, fn, pid, why))
    with open(os.path.join(a.out, "warnings.txt"), "w") as f:
        for lvl, fn, m in msgs: f.write("%s\t%s\t%s\n" % (lvl, fn, m))
    print(json.dumps(stats, indent=1))

def re_norm(m):
    import re
    return re.sub(r"\d+", "N", m)[:120]

if __name__ == "__main__":
    main()

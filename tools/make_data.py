#!/usr/bin/env python3
"""make_data.py -- build the data directory of the native Dragon Quest IV port from YOUR OWN copy of the Android game.

    python3 tools/make_data.py <DQ4.apk> <main.105....obb> <outdir>  [--lang en|ja|ko|zh-tw|zh-cn] [--jobs N] [--keep-work] [--skip-md5]

<outdir> is then copied to the Vita as ux0:data/dqiv (FTP / USB / vitashell).  It receives:

    lib/libdq4core.so, lib/libsdlib_android.so   game logic + audio engine, taken unchanged from the APK
    assets/filelist.bin, assets/ct.inf           AAssetManager root files (from the APK)
    main.obb                                     your OBB, copied unchanged (the native code reads it directly)
    pack/{index.bin,meta.pak,mesh.pak,tex.pak,blob.pak}   the converted Unity assets (tools/convert, ~200 MB)
    cg/*.bin                                     precompiled GPU programs of the screen-effect shaders (from extras/cg)
    config.txt                                   user config (language ...), see README

Supported game version: Dragon Quest IV (Google Play) v1.0.3, OBB main.105 -- the md5 of both files is checked (--skip-md5 to override at your own risk).
The conversion needs Python >= 3.10 and the packages of tools/requirements.txt; unless --no-venv is given, this script creates tools/.venv and re-runs itself there.
Temporary files (~400 MB) go to <outdir>/.work and are removed at the end (--keep-work to keep them).  Works on Linux, macOS and Windows (spawn workers).
"""
import argparse, hashlib, os, re, shutil, subprocess, sys, time, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)

KNOWN = {   # md5 of the supported files
    "apk": ("c2d52842b467815c2f1f36ece53e151f", "Dragon Quest IV v1.0.3 APK (com.square_enix.android_googleplay.dq4)"),
    "obb": ("dd15c635f179d7cbf1deca1263313c16", "Dragon Quest IV OBB main.105"),
}
PACK_SHA256 = {   # sha256 of the pack produced from the supported APK/OBB with tools/requirements.txt (the conversion is deterministic)
    "index.bin": "f545c720de3fd605762e8f4b9368afac7a13109b6b2a1e5473e049975e38271d",
    "meta.pak": "56cac028071c9146217ba60d3e64899beb553d0e3a364c932798e4d410143a64",
    "mesh.pak": "c467d7f28000a0dc1e5c497826adcd3bcdbdf0eee3b6109d45f88ed3fc704449",
    "tex.pak": "9becf4f626f50408bbe66cf4379ad81d5a5e6a62f169cee75b820e1fab33c189",
    "blob.pak": "5fd6ae21586b87d8ae65ad7f41da03e0253bcec6b6fa92d49d6b120ad4b51cc8",
}
LIBS = ["libdq4core.so", "libsdlib_android.so"]
LANGS = ["en", "ja", "ko", "zh-tw", "zh-cn"]
HEX32 = re.compile(r"^assets/bin/Data/([0-9a-f]{32})$")

def log(msg): print(msg, flush=True)
def die(msg): print("ERROR: " + msg, file=sys.stderr); sys.exit(1)

def md5_of(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 22), b""): h.update(chunk)
    return h.hexdigest()

# ---------------------------------------------------------------------------------------------------------------- venv bootstrap
def bootstrap_venv(argv):
    if sys.version_info < (3, 10): die("Python >= 3.10 is required (found %s)" % sys.version.split()[0])
    venv = os.path.join(HERE, ".venv")
    py = os.path.join(venv, "Scripts" if os.name == "nt" else "bin", "python" + (".exe" if os.name == "nt" else ""))
    stamp = os.path.join(venv, ".requirements.sha1")
    req = os.path.join(HERE, "requirements.txt")
    want = hashlib.sha1(open(req, "rb").read()).hexdigest()
    if not (os.path.exists(py) and os.path.exists(stamp) and open(stamp).read() == want):
        log("[venv] creating %s and installing tools/requirements.txt (first run only) ..." % venv)
        import venv as venv_mod
        venv_mod.create(venv, with_pip=True)
        r = subprocess.run([py, "-m", "pip", "install", "--disable-pip-version-check", "-r", req])
        if r.returncode: die("pip install failed (see above). Python 3.12+ is the tested range; try --python-venv with another interpreter.")
        open(stamp, "w").write(want)
    r = subprocess.run([py, os.path.abspath(__file__), "--no-venv"] + argv)
    sys.exit(r.returncode)

# ---------------------------------------------------------------------------------------------------------------- steps
def check_inputs(apk, obb, skip_md5):
    for k, p in (("apk", apk), ("obb", obb)):
        if not os.path.isfile(p): die("%s not found: %s" % (k.upper(), p))
        try: zipfile.ZipFile(p).close()
        except zipfile.BadZipFile: die("%s is not a valid zip/APK/OBB file: %s" % (k.upper(), p))
        want, what = KNOWN[k]
        log("[check] md5 of %s ..." % os.path.basename(p))
        got = md5_of(p)
        if got != want:
            msg = "%s md5 mismatch: %s\n  expected %s (%s)\n  got      %s" % (k.upper(), p, want, what, got)
            if skip_md5: log("WARNING: " + msg + "\n  (--skip-md5: continuing, results are not guaranteed)")
            else: die(msg + "\n  This port supports exactly that version (Google Play v1.0.3, OBB main.105).")

def extract_libs_and_assets(apk, out):
    z = zipfile.ZipFile(apk)
    os.makedirs(os.path.join(out, "lib"), exist_ok=True); os.makedirs(os.path.join(out, "assets"), exist_ok=True)
    for lib in LIBS:
        with z.open("lib/armeabi-v7a/" + lib) as s, open(os.path.join(out, "lib", lib), "wb") as d: shutil.copyfileobj(s, d)
    for a in ("filelist.bin", "ct.inf"):
        with z.open("assets/" + a) as s, open(os.path.join(out, "assets", a), "wb") as d: shutil.copyfileobj(s, d)
    log("[apk] lib/%s, assets/filelist.bin, assets/ct.inf" % ", lib/".join(LIBS))

def copy_obb(obb, out):
    dst = os.path.join(out, "main.obb")
    if os.path.exists(dst) and os.path.getsize(dst) == os.path.getsize(obb) and md5_of(dst) == KNOWN["obb"][0]:
        log("[obb] main.obb already in place"); return
    log("[obb] copying main.obb (%d MB) ..." % (os.path.getsize(obb) >> 20))
    shutil.copyfile(obb, dst)

def stage_unity(apk, obb, work):
    """The converter's input: mainData + sharedassets0.assets (APK, split in 1 MiB parts), level0 + sharedassets1.assets + every <guid> file (OBB)."""
    root = os.path.join(work, "unity"); data = os.path.join(root, "obb_data")
    if os.path.isdir(root): shutil.rmtree(root)
    os.makedirs(data)
    za = zipfile.ZipFile(apk)
    with za.open("assets/bin/Data/mainData") as s, open(os.path.join(root, "mainData"), "wb") as d: shutil.copyfileobj(s, d)
    parts = sorted((n for n in za.namelist() if n.startswith("assets/bin/Data/sharedassets0.assets.split")), key=lambda n: int(n.rsplit("split", 1)[1]))
    if not parts: die("APK has no sharedassets0.assets.split* entries (wrong file?)")
    with open(os.path.join(root, "sharedassets0.assets"), "wb") as d:
        for n in parts:
            with za.open(n) as s: shutil.copyfileobj(s, d)
    zo = zipfile.ZipFile(obb)
    names = zo.namelist()
    for top in ("level0", "sharedassets1.assets"):
        with zo.open("assets/bin/Data/" + top) as s, open(os.path.join(root, top), "wb") as d: shutil.copyfileobj(s, d)
    n = 0
    log("[obb] extracting Unity serialized files ...")
    for name in names:
        m = HEX32.match(name)
        if m:
            with zo.open(name) as s, open(os.path.join(data, m.group(1)), "wb") as d: shutil.copyfileobj(s, d)
            n += 1
    # the converter expects these two also under obb_data (they are listed there in the reference run)
    for top in ("level0", "sharedassets1.assets"): shutil.copyfile(os.path.join(root, top), os.path.join(data, top))
    log("[obb] %d asset files staged in %s" % (n, root))
    if n < 21000: die("OBB holds only %d Unity asset files (expected ~21600): wrong OBB?" % n)
    return root

def run_converter(unity_root, out, work, jobs):
    env = dict(os.environ, DQIV_UNITY_ROOT=unity_root, PYTHONHASHSEED="0")
    pack = os.path.join(out, "pack")
    cmd = [sys.executable, "-I", os.path.join(HERE, "convert", "dqiv_convert.py"), "--out", pack, "--shaders-out", os.path.join(work, "pack_shaders"), "--jobs", str(jobs)]
    log("[pack] converting the Unity assets (several minutes, ~2 GB RAM) ...")
    t = time.time()
    r = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    open(os.path.join(work, "convert.log"), "w").write(r.stdout)
    if r.returncode: print(r.stdout[-3000:]); die("converter failed (log: %s)" % os.path.join(work, "convert.log"))
    need = ["index.bin", "meta.pak", "mesh.pak", "tex.pak", "blob.pak"]
    for f in need:
        if not os.path.isfile(os.path.join(pack, f)): die("converter did not produce pack/" + f)
    for f in os.listdir(pack):          # runtime does not need the reports
        if f not in need: os.remove(os.path.join(pack, f))
    bad = [f for f in need if hashlib.sha256(open(os.path.join(pack, f), "rb").read()).hexdigest() != PACK_SHA256[f]]
    if bad: log("WARNING: pack file(s) %s differ from the reference build (different UnityPy/numpy than tools/requirements.txt, or --skip-md5 input?). It may still work; if the game misbehaves, redo with the pinned venv." % ", ".join(bad))
    else: log("[pack] verified: identical to the reference conversion")
    log("[pack] done in %.0f s: %s" % (time.time() - t, ", ".join("%s %.1f MB" % (f, os.path.getsize(os.path.join(pack, f)) / 1e6) for f in need)))

def copy_cg(out):
    src = os.path.join(REPO, "extras", "cg")
    dst = os.path.join(out, "cg"); os.makedirs(dst, exist_ok=True)
    if os.path.isdir(src):
        for f in os.listdir(src):
            if f.endswith(".bin"): shutil.copyfile(os.path.join(src, f), os.path.join(dst, f))
    log("[cg] %d precompiled shader blobs (the game compiles them with libshacccg when missing)" % len(os.listdir(dst)))

def write_config(out, lang):
    p = os.path.join(out, "config.txt")
    if os.path.exists(p): log("[config] config.txt exists, kept"); return
    with open(p, "w") as f:
        f.write("# Dragon Quest IV (native port) -- ux0:data/dqiv/config.txt, one key=value per line\n")
        f.write("# lang: en (default) | ja | ko | zh-tw | zh-cn\nlang=%s\n" % lang)
        f.write("# confirm=circle   swap Cross/Circle (Japanese layout)\n# layout=landscape|split|portrait   screen layout (default landscape)\n")
    log("[config] config.txt (lang=%s)" % lang)

def main():
    ap = argparse.ArgumentParser(description="Create ux0:data/dqiv for the native Dragon Quest IV port from your own APK + OBB.")
    ap.add_argument("apk"); ap.add_argument("obb"); ap.add_argument("outdir")
    ap.add_argument("--lang", choices=LANGS, default="en")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--keep-work", action="store_true"); ap.add_argument("--skip-md5", action="store_true")
    ap.add_argument("--no-pack", action="store_true", help="only libs/assets/obb/config (skip the slow asset conversion)")
    ap.add_argument("--no-venv", action="store_true", help="use the current Python (needs the tools/requirements.txt packages)")
    a = ap.parse_args()
    if not a.no_venv: bootstrap_venv(sys.argv[1:])
    t0 = time.time()
    out = os.path.abspath(a.outdir); work = os.path.join(out, ".work")
    check_inputs(a.apk, a.obb, a.skip_md5)
    os.makedirs(out, exist_ok=True); os.makedirs(work, exist_ok=True)
    extract_libs_and_assets(a.apk, out)
    copy_obb(a.obb, out)
    copy_cg(out)
    write_config(out, a.lang)
    if not a.no_pack:
        run_converter(stage_unity(a.apk, a.obb, work), out, work, max(1, a.jobs))
    if not a.keep_work: shutil.rmtree(work, ignore_errors=True)
    log("\nDone in %.0f s. Copy the contents of %s to ux0:data/dqiv/ on the Vita (about %d MB)." % (time.time() - t0, out, sum(os.path.getsize(os.path.join(dp, f)) for dp, _, fs in os.walk(out) for f in fs) >> 20))

if __name__ == "__main__":
    main()

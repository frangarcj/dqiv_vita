#!/usr/bin/env python3
"""Validate a Release build and optionally stage its release bundle.

    python3 tools/check_release.py build [--require-clean-source] [--output release/ci]

Checks packaging and build provenance only; it does not validate gameplay.
--output stages dqivn.vpk, dqivn.elf, manifest.json and SHA256SUMS."""
import argparse, hashlib, json, re, shutil, struct, subprocess, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TITLE_ID = "DQIVN0001"
OFF_OPTIONS = ("DQIV_DEV_TOOLS", "DQIV_FAST_BOOT", "DQIV_TRACE_LINK", "DQIVN_MEMSTATS")
IMAGES = {   # the Vita refuses to promote a package with non-indexed PNGs (0x8010113D)
    "sce_sys/icon0.png": (128, 128),
    "sce_sys/pic0.png": (960, 544),
    "sce_sys/livearea/contents/bg0.png": (840, 500),
    "sce_sys/livearea/contents/startup.png": (280, 158),
}
OTHER_FILES = {"eboot.bin", "sce_sys/param.sfo", "sce_sys/livearea/contents/template.xml",
               "cg/dqiv_gray.bin", "cg/dqiv_sepia_fld.bin", "cg/dqiv_sleep.bin"}

def require(ok, message):
    if not ok: sys.exit("FAIL: " + message)

def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args], text=True).strip()

def read_cache(path):
    values = {}
    for line in path.read_text().splitlines():
        m = re.match(r"([A-Za-z0-9_]+):[A-Z]+=(.*)$", line)
        if m: values[m[1]] = m[2]
    return values

def read_sfo(data):
    magic, _, keys, values, count = struct.unpack_from("<4sIIII", data)
    require(magic == b"\0PSF", "param.sfo is not an SFO")
    out = {}
    for i in range(count):
        key, fmt, length, _, offset = struct.unpack_from("<HHIII", data, 20 + i * 16)
        name = data[keys + key:data.index(b"\0", keys + key)].decode()
        raw = data[values + offset:values + offset + length]
        out[name] = raw.rstrip(b"\0").decode() if fmt == 0x0204 else raw
    return out

def check_png(name, data, size):
    require(data[:8] == b"\x89PNG\r\n\x1a\n" and data[12:16] == b"IHDR", name + " is not a PNG")
    w, h, depth, color = struct.unpack(">IIBB", data[16:26])
    require((w, h) == size, "%s is %dx%d, expected %dx%d" % (name, w, h, *size))
    require(depth == 8 and color == 3, "%s must be an 8-bit palette PNG (bit depth %d, colour type %d)" % (name, depth, color))

def validate(build):
    cache = read_cache(build / "CMakeCache.txt")
    require(cache.get("CMAKE_BUILD_TYPE") == "Release", "not a Release build")
    require(cache.get("DQIVN_SLOT") == "1", "not install slot 1")
    for option in OFF_OPTIONS:
        require(cache.get(option, "OFF") == "OFF", option + " must be OFF")

    elf, vpk = build / "dqivn", build / "dqivn.vpk"
    require(elf.read_bytes()[:4] == b"\x7fELF", "dqivn is not an ELF")
    with zipfile.ZipFile(vpk) as z:
        names = {i.filename for i in z.infolist() if not i.is_dir()}
        require(names == OTHER_FILES | set(IMAGES), "unexpected VPK contents: %s" % sorted(names ^ (OTHER_FILES | set(IMAGES))))
        for name, size in IMAGES.items():
            check_png(name, z.read(name), size)
        sfo = read_sfo(z.read("sce_sys/param.sfo"))
        require(z.read("eboot.bin") == (build / "eboot.bin").read_bytes(), "the VPK's eboot.bin is not this build's")
    require(sfo.get("TITLE_ID") == TITLE_ID, "title id %r" % sfo.get("TITLE_ID"))

    return {
        "title_id": TITLE_ID,
        "app_version": sfo.get("APP_VER"),
        "build_type": cache["CMAKE_BUILD_TYPE"],
        "dev_tools": cache.get("DQIV_DEV_TOOLS", "OFF") != "OFF",
        "source_revision": git("rev-parse", "HEAD"),
        "source_dirty": bool(git("status", "--porcelain")),
        "artifacts": {"dqivn.vpk": sha256(vpk), "dqivn.elf": sha256(elf)},
    }

def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("build", type=Path)
    parser.add_argument("--require-clean-source", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    manifest = validate(args.build)
    if args.require_clean_source:
        require(not manifest["source_dirty"], "uncommitted changes in the source tree:\n" + git("status", "--porcelain"))
    if args.output:
        args.output.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(args.build / "dqivn.vpk", args.output / "dqivn.vpk")
        shutil.copyfile(args.build / "dqivn", args.output / "dqivn.elf")
        (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        names = ("dqivn.vpk", "dqivn.elf", "manifest.json")
        (args.output / "SHA256SUMS").write_text("".join("%s  %s\n" % (sha256(args.output / n), n) for n in names))
    print("PASS: %s %s %s%s" % (manifest["title_id"], manifest["app_version"], manifest["source_revision"][:8], " (dirty)" if manifest["source_dirty"] else ""))

main()

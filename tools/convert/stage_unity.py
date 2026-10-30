#!/usr/bin/env python3
"""Developer staging of the converter input (what make_data.py's stage_unity() does for end users, but the APK may be an
extracted directory):

    python3 -I tools/convert/stage_unity.py <apk dir | apk zip> <main.106...obb> <unity_root>

DQ4 (Unity 4.6.5f1) layout: APK  assets/bin/Data/mainData + sharedassets0.assets.split0..N (concatenated)
                            OBB  assets/bin/Data/{level0,sharedassets1.assets,<32 hex guid>...}
<unity_root>/{mainData,sharedassets0.assets,level0,sharedassets1.assets,obb_data/<guid>...}   (DQIV_UNITY_ROOT of dqivconv/unityenv.py)
"""
import os, re, shutil, sys, zipfile

HEX32 = re.compile(r"^assets/bin/Data/([0-9a-f]{32})$")
apk, obb, root = sys.argv[1:4]
data = os.path.join(root, "obb_data")
if os.path.isdir(root): shutil.rmtree(root)
os.makedirs(data)
if os.path.isdir(apk):
    base = os.path.join(apk, "assets", "bin", "Data")
    op = lambda n: open(os.path.join(base, n), "rb")
    parts = sorted((n for n in os.listdir(base) if n.startswith("sharedassets0.assets.split")), key=lambda n: int(n.rsplit("split", 1)[1]))
else:
    za = zipfile.ZipFile(apk)
    op = lambda n: za.open("assets/bin/Data/" + n)
    parts = sorted((n.rsplit("/", 1)[1] for n in za.namelist() if n.startswith("assets/bin/Data/sharedassets0.assets.split")), key=lambda n: int(n.rsplit("split", 1)[1]))
if not parts: sys.exit("no sharedassets0.assets.split* in the APK")
with op("mainData") as s, open(os.path.join(root, "mainData"), "wb") as d: shutil.copyfileobj(s, d)
with open(os.path.join(root, "sharedassets0.assets"), "wb") as d:
    for n in parts:
        with op(n) as s: shutil.copyfileobj(s, d)
zo = zipfile.ZipFile(obb)
for top in ("level0", "sharedassets1.assets"):
    with zo.open("assets/bin/Data/" + top) as s, open(os.path.join(root, top), "wb") as d: shutil.copyfileobj(s, d)
    shutil.copyfile(os.path.join(root, top), os.path.join(data, top))
n = 0
for name in zo.namelist():
    m = HEX32.match(name)
    if m:
        with zo.open(name) as s, open(os.path.join(data, m.group(1)), "wb") as d: shutil.copyfileobj(s, d)
        n += 1
print("staged %d guid files + %d sharedassets0 parts in %s" % (n, len(parts), root))

#!/usr/bin/env python3
"""Pure-python reader / dumper for the DQIV pack (debug aid, independent of the C loader).

  python3 dqiv_inspect.py [--pack DIR] ls [prefix]          list resource paths
  python3 dqiv_inspect.py [--pack DIR] show <resources/path> dump the objects behind a path
  python3 dqiv_inspect.py [--pack DIR] gid <id>             dump one object
"""
import argparse, os, struct, sys

KN = {1: "Texture", 2: "Mesh", 3: "Material", 4: "Shader", 5: "Prefab", 6: "AnimClip", 7: "TextAsset", 8: "Font"}
TF = {1: "A8", 2: "RGB565", 3: "RGBA4444", 4: "RGBA8888"}

class Pack:
    def __init__(self, d):
        self.d = d
        self.idx = open(os.path.join(d, "index.bin"), "rb").read()
        h = struct.unpack_from("<4s7I2I", self.idx, 0)
        assert h[0] == b"DQIV"
        (_, ver, self.nobj, self.nres, self.objs_off, self.res_off, self.str_off, self.str_size) = h[:8]
        self.dfn = []
        for i in range(4):
            no, sz = struct.unpack_from("<II", self.idx, 64 + 8 * i)
            self.dfn.append(self.s(no))
        self.files = [open(os.path.join(d, n), "rb") for n in self.dfn]

    def s(self, off):
        b = self.idx
        o = self.str_off + off
        return b[o:b.index(b"\0", o)].decode("utf-8", "replace")

    def obj(self, g):
        return struct.unpack_from("<6I", self.idx, self.objs_off + 24 * g)

    def res(self, i):
        po, g, k, _ = struct.unpack_from("<4I", self.idx, self.res_off + 16 * i)
        return self.s(po), g, k

    def blob(self, g):
        k, df, off, size, no, aux = self.obj(g)
        f = self.files[df]; f.seek(off)
        return f.read(size)

def bs(b, off):
    if not off: return ""
    return b[off:b.index(b"\0", off)].decode("utf-8", "replace")

def dump(p, g, full=False):
    k, df, off, size, no, aux = p.obj(g)
    b = p.blob(g)
    print("[%d] %s '%s' %s:%d size=%d" % (g, KN.get(k, k), p.s(no), p.dfn[df], off, size))
    if k == 1:
        (_, _, fmt, w, h, ow, oh, fl, wr, fi, an, of, doff, dsz, mips, bias) = struct.unpack_from("<IIIHHHHIBBBBIIIf", b, 0)
        print("   %s %dx%d flags=%d wrap=%d filter=%d aniso=%d unity_fmt=%d data=%d" % (TF[fmt], w, h, fl, wr, fi, an, of, dsz))
    elif k == 2:
        v = struct.unpack_from("<8I6hI5I6fI", b, 0)
        print("   verts=%d stride=%d idx=%d subs=%d bones=%d flags=%d attr_off(pos,nrm,col,uv0,uv1,tan)=%s" % (v[2], v[3], v[4], v[5], v[6], v[7], v[8:14]))
        vo, io, so = v[15], v[16], v[17]
        for i in range(min(v[2], 3 if not full else v[2])):
            print("   v%d pos=%s" % (i, struct.unpack_from("<3f", b, vo + i * v[3])))
        for i in range(v[5]):
            print("   sub%d %s" % (i, struct.unpack_from("<5I6f", b, so + 48 * i)))
    elif k == 3:
        v = struct.unpack_from("<IIIiIIIIIIIIII", b, 0)
        print("   shader=%d (%s) queue=%d main_tex=%d" % (v[2], bs(b, v[4]), v[3], v[13]))
        for i in range(v[5]):
            n, t, sx, sy, ox, oy = struct.unpack_from("<IIffff", b, v[6] + 24 * i); print("   tex %s -> %d scale=(%g,%g) off=(%g,%g)" % (bs(b, n), t, sx, sy, ox, oy))
        for i in range(v[7]):
            n, f = struct.unpack_from("<If", b, v[8] + 8 * i); print("   float %s = %g" % (bs(b, n), f))
        for i in range(v[9]):
            t = struct.unpack_from("<I4f", b, v[10] + 20 * i); print("   color %s = %s" % (bs(b, t[0]), t[1:]))
    elif k == 4:
        v = struct.unpack_from("<IIIiIIIIIII", b, 0)
        print("   name=%s queue=%d dump=%s passes=%d props=%d gles=%d" % (bs(b, v[2]), v[3], bs(b, v[4]), v[6], v[7], v[8]))
        for i in range(v[6]):
            print("   pass%d %s" % (i, struct.unpack_from("<6ifi2f", b, v[9] + 40 * i)))
    elif k == 5:
        v = struct.unpack_from("<8I", b, 0)
        print("   nodes=%d comps=%d roots=%d scene=%d" % (v[2], v[3], v[6], v[7]))
        for i in range(v[2]):
            n = struct.unpack_from("<I3i10f2BH5I", b, v[4] + 80 * i)
            print("   node%d '%s' parent=%d child=%d next=%d pos=%s rot=%s scl=%s layer=%d active=%d tag=%d comps=%d..+%d" % (
                i, bs(b, n[0]), n[1], n[2], n[3], n[4:7], n[7:11], n[11:14], n[14], n[15], n[16], n[17], n[18]))
            for c in range(n[18]):
                t, en, do, ds = struct.unpack_from("<4I", b, v[5] + 16 * (n[17] + c))
                print("      comp class=%d enabled=%d off=%d size=%d %s" % (t, en, do, ds, comp_str(p, b, t, do, ds)))
    elif k == 6:
        v = struct.unpack_from("<IIffiIIIIf", b, 0)
        print("   rate=%g duration=%g wrap=%d curves=%d events=%d" % (v[2], v[3], v[4], v[5], v[7]))
        for i in range(min(v[5], 4 if not full else v[5])):
            c = struct.unpack_from("<5I1I2i", b, v[6] + 32 * i)
            print("   curve %s/%s type=%d keys=%d" % (bs(b, c[0]), bs(b, c[1]), c[2], c[4]))
    elif k == 7:
        n = struct.unpack_from("<I", b, 8)[0]
        print("   %d bytes: %r" % (n, b[16:16 + 80]))
    elif k == 8:
        v = struct.unpack_from("<2I5f6i2I", b, 0)
        print("   size=%g line=%g ascent=%g rects=%d ttf=%d tex=%d mat=%d" % (v[2], v[3], v[4], struct.unpack_from("<I", b, 60)[0], struct.unpack_from("<I", b, 76)[0], v[13], v[14]))

def comp_str(p, b, t, off, size):
    if t == 33: return "mesh=%d" % struct.unpack_from("<I", b, off)
    if t == 23:
        f, lm, so, nm = struct.unpack_from("<IiiI", b, off); return "mats=%s" % list(struct.unpack_from("<%dI" % nm, b, off + 16))
    if t == 137:
        f, mesh, rb, nm, nb = struct.unpack_from("<IIiII", b, off); return "mesh=%d root=%d mats=%d bones=%d" % (mesh, rb, nm, nb)
    if t == 111:
        w, pa, cu, dc, nc = struct.unpack_from("<iIiII", b, off); return "default_clip=%d clips=%d wrap=%d auto=%d" % (dc, nc, w, pa)
    if t == 20: return "camera %s" % (struct.unpack_from("<I4f4f4fI f I i I I I".replace(" ", ""), b, off),)
    cn, n = struct.unpack_from("<II", b, off)
    out = []
    for i in range(min(n, 12)):
        no, ty = struct.unpack_from("<II", b, off + 8 + 24 * i)
        raw = b[off + 16 + 24 * i: off + 32 + 24 * i]
        val = struct.unpack("<4I", raw) if ty in (0, 2, 3, 7, 8) else struct.unpack("<4f", raw)
        if ty == 3: val = bs(b, val[0])
        out.append("%s=%s" % (bs(b, no), val))
    return "props[%s] class=%s %s" % (n, bs(b, cn), "; ".join(out))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pack", default="/Users/frangar/Fun/dqiv/work/extract/pack")
    ap.add_argument("cmd", choices=["ls", "show", "gid"])
    ap.add_argument("arg", nargs="?", default="")
    ap.add_argument("--full", action="store_true")
    a = ap.parse_args()
    p = Pack(a.pack)
    if a.cmd == "ls":
        for i in range(p.nres):
            path, g, k = p.res(i)
            if path.startswith(a.arg.lower()): print(path, KN[k], g)
    elif a.cmd == "show":
        for i in range(p.nres):
            path, g, k = p.res(i)
            if path == a.arg.lower(): dump(p, g, a.full)
    else:
        dump(p, int(a.arg), a.full)

if __name__ == "__main__":
    main()

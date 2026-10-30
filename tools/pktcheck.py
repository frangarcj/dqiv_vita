#!/usr/bin/env python3
"""Offline checker for packet traces written by native/packet_recorder (ux0:data/dqiv/trace/NNNN.pkt).

usage: tools/pktcheck.py [-v] [-q] TRACE_DIR_OR_FILES...

Walks every native->host command stream with the EXACT operand consumption of NativeManager.cs (ints AND floats, 02_PACKET_PROTOCOL.md §C; the same table
as NativeManager.cpp) and verifies, per frame:
  * the int stream ends with -1 exactly at the recorded length,  * the float stream is consumed exactly,  * opcode range / slot ranges (chara 40, rise 16, mesh 24, fbx 16),
  * op0 (fld_load) has a string,  * mesh (op23) vertex counts are multiples of 4 and index values stay inside the vertex range,
  * the MenuPort native->host buffer is a valid linked list (heads, offsets, lengths),  * the host->native input stream is well formed (ops 0/1, -1),
  * the frame numbers are consecutive.
Also prints per-run totals (opcode histogram, first frame of every opcode, flag changes, MenuPort port opens, BGM/SE ids).
Exit status 1 when any frame is bad.  This is the "--replay" offline equivalent: the same data can be fed back into the app with ux0:data/dqiv/replay.on.
"""
import struct, sys, os, glob

# DQ4 command table (NativeManager.cs m_cmd_tbl, 38 entries): DQ5's ops 8 fld_etc, 28 mesh_ortho and 40 particle do not exist, so every later op number is lower.
NAMES = ["fld_load","fld_color","fld_alpha","fld_pos","fld_rot","fld_common_anim","fld_event_anim","fld_on_off","back_color","fade","camera","camera_sub",
         "chara_load","chara","chara_on_off","chara_shadow","chara_sleep_pose","chara_sleep_near","chara_pose","rise_icon_load","rise_icon","mesh_load","mesh","mesh_on_off",
         "mesh_shader","mesh_color","mesh_tex_offset","field_load","field","field_symbol","field_scale","fbx_load","fbx","map_color_rate","screen_effect",
         "se_play","se_stop","bgm"]
# the operand walker below is written with the DQ5 op numbers (shared operand layouts); translate DQ4 -> DQ5 numbering
_DQ5 = ["fld_load","fld_color","fld_alpha","fld_pos","fld_rot","fld_common_anim","fld_event_anim","fld_on_off","fld_etc","back_color","fade","camera","camera_sub",
        "chara_load","chara","chara_on_off","chara_shadow","chara_sleep_pose","chara_sleep_near","chara_pose","rise_icon_load","rise_icon","mesh_load","mesh","mesh_on_off",
        "mesh_shader","mesh_color","mesh_tex_offset","mesh_ortho","field_load","field","field_symbol","field_scale","fbx_load","fbx","map_color_rate","screen_effect",
        "se_play","se_stop","bgm","particle"]
SENT_I = 0x7EADBEEF


class Bad(Exception):
    pass


def bt(a, b):
    return (a & b) != 0


class Stream:
    def __init__(self, ints, floats):
        self.i = ints; self.f = floats; self.io = 1; self.fo = 0     # [0] = flag

    def gi(self):
        if self.io >= len(self.i): raise Bad("int stream overrun")
        v = self.i[self.io]; self.io += 1; return v

    def gf(self, n=1):
        if self.fo + n > len(self.f): raise Bad("float stream overrun")
        self.fo += n

    def skip_i(self, n):
        if n < 0 or self.io + n > len(self.i): raise Bad("int skip overrun (%d)" % n)
        self.io += n


def run_command(op, s, strings, info):
    if not (0 <= op < len(NAMES)): raise Bad("opcode %d out of range (DQ4 has %d)" % (op, len(NAMES)))
    name = NAMES[op]
    op = _DQ5.index(name)
    def slot(v, n, what):
        if not (0 <= v < n): raise Bad("%s slot %d out of range (op %s)" % (what, v, name))
    if op == 0:
        v = s.gi()
        if not strings: raise Bad("fld_load without a string")
        info["map"] = strings
    elif op == 1: pass
    elif op in (2, 5, 6, 7, 8):
        s.gi()
    elif op == 3: s.gi(); s.gf(3)
    elif op == 4: s.gi(); s.gf(2)
    elif op == 9: s.gf(6)
    elif op == 10:
        v = s.gi()
        if bt(v, 4096) and bt(v, 2048): s.gf(3)
    elif op == 11:
        a = s.gi()
        for b in (1, 2, 4):
            if bt(a, b): s.gf(3)
    elif op == 12:
        a = s.gi()
        for b in (1, 2, 4, 16, 32):
            if bt(a, b): s.gf(1)
    elif op == 13:
        sl = s.gi(); s.gi(); slot(sl, 40, "chara")
    elif op == 14:
        v = s.gi(); slot(v & 0xFF, 40, "chara")
        if bt(v, 2048): s.gf(1)
        if bt(v, 4096): s.gf(4)
        if bt(v, 8192): s.gf(3)
        if bt(v, 16384): s.gi()
        if bt(v, 32768): s.gf(3)
    elif op == 15:
        sl = s.gi(); s.gi(); slot(sl, 40, "chara")
    elif op == 16:
        v = s.gi(); slot(v & 0xFF, 40, "chara")
        if bt(v, 4096): s.gf(3)
        if bt(v, 8192): s.gf(1)
        if bt(v, 16384): s.gf(3)
    elif op == 17: v = s.gi(); slot(v & 0xFF, 40, "chara")
    elif op == 18: s.gi(); s.gi()
    elif op == 19: v = s.gi(); slot(v & 0xFF, 40, "chara")
    elif op == 20: s.gi(); s.gi()
    elif op == 21: sl = s.gi(); slot(sl, 16, "rise"); s.gf(4)
    elif op == 22: sl = s.gi(); s.gi(); slot(sl, 24, "mesh")
    elif op == 23:
        sl = s.gi(); slot(sl, 24, "mesh"); s.gf(3)
        n = s.gi()
        if n < 0 or n % 4 or n > 4096: raise Bad("mesh vertex count %d" % n)
        for i in range(n):
            s.gf(5)
            if i & 3 == 0: s.gi()
        ni = n * 6 // 4
        base = s.io; s.skip_i(ni)
        for k in range(ni):
            if not (0 <= s.i[base + k] < n): raise Bad("mesh index %d out of range (n=%d)" % (s.i[base + k], n))
        info["mesh"] = info.get("mesh", 0) + 1
    elif op == 24: sl = s.gi(); s.gi(); slot(sl, 24, "mesh")
    elif op == 25: v = s.gi(); slot(v & 0xFF, 24, "mesh")
    elif op == 26: sl = s.gi(); slot(sl, 24, "mesh"); s.gf(3)
    elif op == 27: s.gf(2); sl = s.gi(); slot(sl, 24, "mesh")
    elif op == 28: sl = s.gi(); slot(sl, 24, "mesh")
    elif op == 29: s.gi()
    elif op == 30:
        s.gi(); s.gi(); s.gi(); n = s.gi(); s.skip_i(n); s.gf(2)
    elif op == 31:
        v = s.gi()
        if v < 32768: s.gf(3)
        elif bt(v, 4096): s.gf(2)
    elif op == 32:
        v = s.gi()
        if v < 0: s.gf(1)
    elif op == 33: sl = s.gi(); s.gi(); slot(sl, 16, "fbx")
    elif op == 34:
        v = s.gi(); slot(v & 0xFF, 16, "fbx")
        if bt(v, 2048): s.gf(3)
        if bt(v, 4096): s.gf(4)
        if bt(v, 8192): s.gf(3)
        if bt(v, 16384): s.gf(3)
        if bt(v, 32768): s.gi()
    elif op == 35: s.gf(3)
    elif op == 36:
        v = s.gi()
        if (v & 0xFF) == 7 and not bt(v, 0x8000):
            if bt(v, 0x100): s.gf(3)
            if bt(v, 0x200): s.gf(4)
            if bt(v, 0x400): s.gf(4)
    elif op == 37: info.setdefault("se", set()).add(s.gi())
    elif op == 38: s.gi()
    elif op == 39:
        v = s.gi()
        if v < 16384: info.setdefault("bgm", set()).add(v)
    elif op == 40: s.gi()
    else: raise Bad("opcode %d out of range" % op)


def check_menu(buf):
    """native->host MenuPort buffer: heads[32], tails[32], cursor, messages"""
    if len(buf) < 65: return "menu buffer shorter than the header"
    heads = buf[:32]; cursor = buf[64]
    if not (65 <= cursor <= 1089): return "menu cursor %d out of range" % cursor
    msgs = 0
    for p in range(32):
        o = heads[p]; guard = 0
        while o != 0:
            guard += 1
            if guard > 512 or o < 65 or o >= len(buf): return "port %d: bad offset %d" % (p, o)
            ln = buf[o]
            if ln < 0 or o + 1 + ln >= max(len(buf), cursor + 1) + 1: return "port %d: bad length %d at %d" % (p, ln, o)
            nxt = buf[o + 1 + ln] if o + 1 + ln < len(buf) else 0
            o = nxt; msgs += 1
    return None


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    verbose = "-v" in sys.argv; quiet = "-q" in sys.argv
    files = []
    for a in args or ["."]:
        files += sorted(glob.glob(os.path.join(a, "*.pkt"))) if os.path.isdir(a) else [a]
    bad = 0; hist = {}; first = {}; prev_frame = None; prev_flag = None; flags = []; maps = []; bgm = set(); se = set()
    for f in files:
        d = open(f, "rb").read(); o = 0
        def u32():
            nonlocal o; v = struct.unpack_from("<I", d, o)[0]; o += 4; return v
        magic, ver, frame, press, toggle, flag_in = [u32() for _ in range(6)]
        if magic != 0x4B505144: print("%s: bad magic" % f); bad += 1; continue
        n_in = u32(); ins = struct.unpack_from("<%di" % n_in, d, o); o += 4 * n_in
        n_out = u32(); outs = struct.unpack_from("<%di" % n_out, d, o); o += 4 * n_out
        n_f = u32(); fl = struct.unpack_from("<%df" % n_f, d, o); o += 4 * n_f
        sl = u32(); s0 = d[o:o + sl].rstrip(b"\0").decode("latin1"); o += sl
        nmu = u32(); o += 2 * nmu
        nmn = u32(); mn = struct.unpack_from("<%dh" % nmn, d, o); o += 2 * nmn
        err = None; cmds = []; info = {}
        try:
            if prev_frame is not None and frame != prev_frame + 1: raise Bad("frame %d does not follow %d" % (frame, prev_frame))
            # host -> native
            i = 0
            while True:
                if i >= len(ins): raise Bad("input stream without terminator")
                v = ins[i]
                if v < 0: break
                if v == 0: i += 4
                elif v == 1: i += 2
                else: raise Bad("input op %d" % v)
            if i != len(ins) - 1: raise Bad("input stream length")
            # native -> host
            s = Stream(outs, fl)
            while True:
                op = s.gi()
                if op < 0: break
                cmds.append(op)
                run_command(op, s, s0, info)
            if s.io != len(outs): raise Bad("int stream: consumed %d of %d" % (s.io, len(outs)))
            if s.fo != len(fl): raise Bad("float stream: consumed %d of %d" % (s.fo, len(fl)))
            m = check_menu(mn)
            if m: raise Bad(m)
        except (Bad, IndexError, struct.error) as e:
            err = str(e)
        prev_frame = frame
        for c in cmds:
            hist[c] = hist.get(c, 0) + 1
            if c not in first: first[c] = frame
        if outs and outs[0] != prev_flag: flags.append((frame, outs[0])); prev_flag = outs[0]
        if "map" in info: maps.append((frame, info["map"]))
        bgm |= info.get("bgm", set()); se |= info.get("se", set())
        if err: bad += 1
        if (verbose or err) and not quiet or (not quiet and (cmds or frame % 60 == 0 or frame < 5)):
            hs = {}
            for c in cmds: hs[NAMES[c]] = hs.get(NAMES[c], 0) + 1
            print("F%04d pad=%03x/%03x flag %05x->%05x ints=%d floats=%d %s%s" % (frame, press, toggle, flag_in, outs[0] if outs else 0, len(outs), len(fl),
                  " ".join("%s*%d" % kv for kv in hs.items()), ("  ** %s **" % err) if err else ""))
    print("---- %d frames, %d bad" % (len(files), bad))
    print("opcodes seen: " + ", ".join("%s*%d(first F%d)" % (NAMES[c], hist[c], first[c]) for c in sorted(hist)))
    if flags: print("flag changes: " + ", ".join("F%d:%05x" % x for x in flags[:40]))
    if maps: print("fld_load: " + ", ".join("F%d:%s" % x for x in maps[:10]))
    if bgm or se: print("bgm ids: %s  se ids: %s" % (sorted(bgm), sorted(se)))
    sys.exit(1 if bad else 0)


main()

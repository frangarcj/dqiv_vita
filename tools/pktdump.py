#!/usr/bin/env python3
"""Dump / sanity-check packet traces written by native/packet_recorder (ux0:data/dqiv/trace/NNNN.pkt).

usage: tools/pktdump.py [-v] TRACE_DIR_OR_FILES...
Per frame: pad, flags, number of ints/floats and the opcode histogram (operand arities follow NativeManager.cpp / 02_PACKET_PROTOCOL.md).
Exit status 1 if a frame does not parse to exactly its terminator (stream desync).
"""
import struct, sys, os, glob

NAMES = ["fld_load","fld_color","fld_alpha","fld_pos","fld_rot","fld_common_anim","fld_event_anim","fld_on_off","fld_etc","back_color","fade","camera","camera_sub",
         "chara_load","chara","chara_on_off","chara_shadow","chara_sleep_pose","chara_sleep_near","chara_pose","rise_icon_load","rise_icon","mesh_load","mesh","mesh_on_off",
         "mesh_shader","mesh_color","mesh_tex_offset","mesh_ortho","field_load","field","field_symbol","field_scale","fbx_load","fbx","map_color_rate","screen_effect",
         "se_play","se_stop","bgm","particle"]

def bt(a, b): return (a & b) != 0

def parse(ints):
    """returns (flag, [(op, start_index)], ok) following the int-operand arities of every command"""
    i = 1; cmds = []
    n = len(ints)
    while i < n:
        op = ints[i]
        if op < 0: return ints[0], cmds, i == n - 1
        if op > 40: return ints[0], cmds, False
        cmds.append((op, i)); i += 1
        a = ints[i] if i < n else 0
        if op in (0,2,5,6,7,8,15 - 15+8,36,37,38,39,40,25,28,29,35 - 35+2,12,11,10,14,16,17,19,31,32,34):
            pass
        # arity table (ints after the op)
        if op in (1, 9, 35): k = 0
        elif op in (13,15,18,20,22,24): k = 2
        elif op == 21 or op == 26 or op == 3 or op == 4: k = 1
        elif op == 27: k = 1
        elif op == 23:
            cnt = ints[i + 1]; k = 2 + (cnt + 3) // 4 + cnt * 6 // 4
        elif op == 30: k = 4 + ints[i + 3]
        elif op == 14: k = 1 + (1 if bt(a, 16384) else 0)
        elif op == 34: k = 1 + (1 if bt(a, 32768) else 0)
        else: k = 1
        i += k
    return ints[0] if ints else 0, cmds, False

def main():
    args = [a for a in sys.argv[1:] if a != "-v"]; verbose = "-v" in sys.argv
    files = []
    for a in args or ["."]:
        files += sorted(glob.glob(os.path.join(a, "*.pkt"))) if os.path.isdir(a) else [a]
    bad = 0
    for f in files:
        d = open(f, "rb").read(); o = 0
        def u32():
            nonlocal o; v = struct.unpack_from("<I", d, o)[0]; o += 4; return v
        magic, ver, frame, press, toggle, flag_in = [u32() for _ in range(6)]
        assert magic == 0x4B505144, "bad magic in %s" % f
        n_in = u32(); ins = struct.unpack_from("<%di" % n_in, d, o); o += 4 * n_in
        n_out = u32(); outs = struct.unpack_from("<%di" % n_out, d, o); o += 4 * n_out
        n_f = u32(); fl = struct.unpack_from("<%df" % n_f, d, o); o += 4 * n_f
        sl = u32(); s = d[o:o + sl].rstrip(b"\0").decode("latin1"); o += sl
        flag, cmds, ok = parse(list(outs))
        hist = {}
        for op, _ in cmds: hist[NAMES[op]] = hist.get(NAMES[op], 0) + 1
        print("F%04d pad=%03x/%03x flag %05x->%05x in=%d out=%d floats=%d %s%s%s" % (frame, press, toggle, flag_in, flag, n_in, n_out, n_f, " ".join("%s*%d" % kv for kv in hist.items()),
              (" str=%r" % s) if s else "", "" if ok else "  ** DESYNC **"))
        if not ok: bad += 1
        if verbose: print("   ints:", outs[:64], "...")
    sys.exit(1 if bad else 0)

main()

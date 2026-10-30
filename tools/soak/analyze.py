#!/usr/bin/env python3
"""Turns the `SOAK ...` lines of a dqivn log into a table and a flat-curve verdict (linear regression of the 2nd half of the run).
   tools/soak/analyze.py dqivn1.log [--every N] [--md]"""
import re, sys
FIELDS = ["ms_avg", "ms_max", "heap", "peak", "arena", "go", "comp", "mat", "mesh", "meshMB", "tex", "res", "texMB", "live", "fds", "ports"]
RX = re.compile(r"SOAK f=(\d+) t=(\d+) ms=([\d.]+)/(\d+) heap=([\d.]+)/([\d.]+)/([\d.]+)\(arena\).*?go=(\d+) comp=(\d+) mat=(\d+) mesh=(\d+)\(([\d.]+)MB\) tex=(\d+)\(res (\d+), ([\d.]+)MB\) live=(\d+) fds=(\d+) ports=(\d+)")
def rows(path):
    out = []
    for l in open(path, errors="replace"):
        m = RX.search(l)
        if m: out.append([int(m.group(1))] + [float(x) for x in m.groups()[2:]])
    return out
def slope(xs, ys):
    n = len(xs); mx = sum(xs) / n; my = sum(ys) / n
    d = sum((x - mx) ** 2 for x in xs)
    return sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / d if d else 0.0
def windows(r, size, offset):
    """per-iteration floor/ceiling: a leak shows as a rising floor"""
    print("\nper %d-frame iteration (offset %d): floor..ceiling of the sampled values" % (size, offset))
    cols = ["heap", "arena", "go", "mat", "mesh", "tex", "live", "ms_avg"]
    print("| iter | " + " | ".join(cols) + " |\n|" + "---|" * (len(cols) + 1))
    k = 0
    while True:
        lo, hi = offset + k * size, offset + (k + 1) * size
        w = [row for row in r if lo <= row[0] < hi]
        if not w: break
        cells = []
        for c in cols:
            j = FIELDS.index(c) + 1
            ys = [row[j] for row in w]; cells.append("%g..%g" % (min(ys), max(ys)))
        print("| %d | " % k + " | ".join(cells) + " |")
        k += 1

def main():
    path = sys.argv[1]; every = int(sys.argv[sys.argv.index("--every") + 1]) if "--every" in sys.argv else 1
    r = rows(path)
    if len(r) < 4: print("not enough SOAK lines"); return
    hdr = ["frame"] + FIELDS
    print("| " + " | ".join(hdr) + " |\n|" + "---|" * len(hdr))
    for i, row in enumerate(r):
        if i % every == 0 or i == len(r) - 1: print("| " + " | ".join(("%g" % v) for v in row) + " |")
    if '--window' in sys.argv: windows(r, int(sys.argv[sys.argv.index('--window') + 1]), int(sys.argv[sys.argv.index('--offset') + 1]) if '--offset' in sys.argv else 0)
    h = r[len(r) // 2:]
    print("\nslope over the second half (%d samples, frames %d..%d), per 10000 frames:" % (len(h), h[0][0], h[-1][0]))
    for j, name in enumerate(FIELDS):
        ys = [row[j + 1] for row in h]; s = slope([row[0] for row in h], ys) * 10000
        print("  %-7s min %-8g max %-8g slope %+.3f" % (name, min(ys), max(ys), s))
main()

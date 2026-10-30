#!/usr/bin/env python3
"""Verify that source/loader/dynlib_table.inc covers every import of libdq4core.so + libsdlib_android.so (refs/imports_*.txt).
Also lists table entries nobody imports. Exit status 1 if an import is missing."""
import re, sys, os
here = os.path.dirname(os.path.abspath(__file__))
tbl = open(os.path.join(here, "..", "source", "loader", "dynlib_table.inc")).read()
names = set(re.findall(r'\{ "([^"]+)"', tbl))
refs = os.environ.get("DQIV_REFS", os.path.join(here, "..", "work", "refs"))
need = set()
for f in ("imports_libdq4core.txt", "imports_libsdlib_android.txt"):
    need |= set(open(os.path.join(refs, f)).read().split())
# symbols that the two libs resolve against each other (libdq4core exports them) are still listed in the table as a fallback
print("table: %d entries, imports: %d" % (len(names), len(need)))
print("MISSING:", sorted(need - names))
print("unused :", sorted(names - need))
sys.exit(1 if need - names else 0)

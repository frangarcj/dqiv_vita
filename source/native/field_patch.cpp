// native/field_patch.cpp -- widen the world-map tile block of libdq4core for the landscape layout (docs/arch-dq5/07_LANDSCAPE.md §8, DQ4 re-derivation).
//
// FieldData::drawBlock (_ZN9FieldData9drawBlockEv, .so offset 0x14178c; Ghidra 0x15178c) emits a fixed block of 25 columns (tile offsets -4..+20 from the view's base tile) x 38 rows
// (-5..+32) = 400 x 608 units into the "field" packet (op 28: startX, startY, width = tiles of the first row, count, then the tile ids), which the host draws as a 25 x 40 quad grid.
// Unlike DQ5 there is no FieldSet in the native: Field.cs (host) fills the UVs itself with a hard-coded row stride, which the DQ4 Field port takes from the mesh width.
// A 960 px wide landscape view (602 units) does not fit 400 units, so three ARM immediates make the block kFieldCols wide (59: back 18, forward 40, zoom 1.25 + panel shift fit),
// around the same tile (original centre = base + 8):
//   +0x60  (0x1417ec)  sub r4, r4, #4     -> #kFieldBack   first column (also the packet's startX)
//   +0x138 (0x1418c4)  cmp r2, #20        -> #kFieldFwd    last column (loop while col - base <= 20)
//   +0x154 (0x1418e0)  sub r4, r3, #4     -> #kFieldBack   column reset at the start of each row
// Every word is checked before it is written; a different libdq4core build is left alone.
#include "native/field_patch.hpp"

#include <kubridge.h>
#include <cstdint>
#include "loader/loader.h"
#include "utils/logger.h"

namespace NativeCall {

static const uint32_t kDrawBlock = 0x14178c;
struct Site { uint32_t vaddr, expect, repl; };

bool PatchFieldBlock() {
  uintptr_t db = loader_symbol("dq4core", "_ZN9FieldData9drawBlockEv");
  if (!db) { l_error("PatchFieldBlock: FieldData::drawBlock not found"); return false; }
  uintptr_t base = db - kDrawBlock;
  const Site sites[] = {
    { 0x1417ec, 0xe2444004u, 0xe2444000u | kFieldBack },
    { 0x1418c4, 0xe3520014u, 0xe3520000u | kFieldFwd },
    { 0x1418e0, 0xe2434004u, 0xe2434000u | kFieldBack },
  };
  for (const Site& s : sites) if (*(volatile uint32_t*)(base + s.vaddr) != s.expect) { l_error("PatchFieldBlock: unexpected word %08x at +%x, not patching", *(volatile uint32_t*)(base + s.vaddr), s.vaddr); return false; }
  for (const Site& s : sites) {
    uint32_t w = s.repl;
    kuKernelCpuUnrestrictedMemcpy((void*)(base + s.vaddr), &w, sizeof(w));
    kuKernelFlushCaches((void*)(base + s.vaddr), 4);
  }
  l_success("world map tile block widened to %d columns (back %d, forward %d)", kFieldCols, kFieldBack, kFieldFwd);
  return true;
}

}  // namespace NativeCall

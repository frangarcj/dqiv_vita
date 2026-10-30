// native/field_patch.cpp -- widen the world-map tile block of libdq4core for the landscape layout (re/arch/07_LANDSCAPE.md §8).
//
// fld::FieldData::drawBlock (vaddr 0x162e04 in the .so, Ghidra 0x172e04) emits a fixed block of 25 columns (tile offsets -4..+20 from the view's base tile) x 38 rows
// (-5..+32) = 400 x 608 units, which the host draws as a 25 x 40 quad grid (Field.cs); FieldSet (0xe57b4) lays the tile UVs out with a row stride of 25 quads.
// The portrait view (256 x 455 units) fits; a 960 px wide landscape view (602 units) does not.  Four ARM immediates make the block kFieldCols wide (59: back 18, forward 40, zoom 1.25 + panel shift fit), around the
// same tile (original centre = base + 8):
//   0x162e44  sub r5, r5, #4      -> #kFieldBack        first column
//   0x162fb8  cmp r2, #20         -> #kFieldFwd         last column (loop while col - base <= 20)
//   0x162fd0  suble r5, r3, #4    -> #kFieldBack        column reset at the start of each row
//   0x0e5924  add r5, r5, #25     -> #kFieldCols        FieldSet row stride (quads)
// The host mesh (Field::Init) uses the same width.  Every word is checked before it is written; a different libdq4core build is left alone.
#include "native/field_patch.hpp"

#include <kubridge.h>
#include <cstdint>
#include "loader/loader.h"
#include "utils/logger.h"

namespace NativeCall {

static const uint32_t kDrawBlock = 0x162e04, kFieldSet = 0xe57b4;
struct Site { uint32_t vaddr, expect, repl; };

bool PatchFieldBlock() {
  uintptr_t fs = loader_symbol("dq4core", "FieldSet");
  if (!fs) { l_error("PatchFieldBlock: FieldSet not found"); return false; }
  uintptr_t base = fs - kFieldSet;
  const Site sites[] = {
    { 0x162e44, 0xe2455004u, 0xe2455000u | kFieldBack },
    { 0x162fb8, 0xe3520014u, 0xe3520000u | kFieldFwd },
    { 0x162fd0, 0xd2435004u, 0xd2435000u | kFieldBack },
    { 0x0e5924, 0xe2855019u, 0xe2855000u | kFieldCols },
  };
  (void)kDrawBlock;
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

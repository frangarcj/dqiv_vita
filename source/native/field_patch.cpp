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

#include "utils/logger.h"

namespace NativeCall {

// DISABLED for DQ4 (TODO(dq4) field_patch): the sites below are libdq4core of DQ5 (fld::FieldData::drawBlock, FieldSet). libdq4core is the older codebase and exports
// neither symbol, so the four words cannot be located by name. Re-derive the world-map tile block of DQ4 with Ghidra (tools/re/decomp.sh) before enabling anything;
// until then nothing is patched, the host uses the original 25-column block (Field.cpp: FieldCols(false)).
bool PatchFieldBlock() {
  l_warn("field_patch: DISABLED for DQ4 (DQ5 offsets, FieldSet/drawBlock not present in libdq4core); world map keeps the original 25-column tile block. TODO(dq4)");
  return false;
}

}  // namespace NativeCall

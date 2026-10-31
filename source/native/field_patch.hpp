// native/field_patch.hpp -- see field_patch.cpp.  kFieldCols = 25 is the original block; the patched block is centred on the same tile.
#pragma once
namespace NativeCall {
constexpr int kFieldColsOrig = 25;
constexpr int kFieldBack = 18, kFieldFwd = 40, kFieldCols = kFieldBack + kFieldFwd + 1;   // 59 columns = 944 units
void RestoreGoldLimit();              // call once after the modules are loaded and bound, in every layout: gold limit comparison, see field_patch.cpp
bool PatchFieldBlock();               // call once after the modules are loaded and bound, only for the landscape layouts
extern bool g_fieldPatched;           // true when the block is 59 columns wide (host mesh must match)
inline int FieldCols(bool patched) { return patched ? kFieldCols : kFieldColsOrig; }
}  // namespace NativeCall

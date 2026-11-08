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

// status::BasePartyStatus::setGold / addGold of this libdq4core build compare the wrong way round (`cmp r2, r1` / `cmp r1, r3` with the 999998 limit on the left), so any amount below the limit
// becomes 999999 and shops never charge. libdq5core and libdq4core's own setBankMoney / addBankMoney show the normal form; a scan of the whole core finds no other limit compared like this.
// Restore the two words; a build that already has the normal form is left alone.
void RestoreGoldLimit() {
  static const uint32_t kSetGold = 0x108af8;
  uintptr_t sg = loader_symbol("dq4core", "_ZN6status15BasePartyStatus7setGoldEj");
  if (!sg) { l_warn("RestoreGoldLimit: BasePartyStatus::setGold not found"); return; }
  uintptr_t base = sg - kSetGold;
  const Site sites[] = {
    { 0x108b10, 0xe1520001u, 0xe1510002u },   // setGold: cmp r2, r1 -> cmp r1, r2
    { 0x108b3c, 0xe1510003u, 0xe1530001u },   // addGold: cmp r1, r3 -> cmp r3, r1
  };
  for (const Site& s : sites) if (*(volatile uint32_t*)(base + s.vaddr) != s.expect) { l_info("RestoreGoldLimit: word %08x at +%x is not the expected one, not patching", *(volatile uint32_t*)(base + s.vaddr), s.vaddr); return; }
  for (const Site& s : sites) {
    uint32_t w = s.repl;
    kuKernelCpuUnrestrictedMemcpy((void*)(base + s.vaddr), &w, sizeof(w));
    kuKernelFlushCaches((void*)(base + s.vaddr), 4);
  }
  l_success("gold limit comparison restored (setGold / addGold)");
}

// MenuItem::term (_ZN8MenuItem4termEv, .so 0x813fc) closes the item's MenuPort and clears the port pointer (+0x3c), but leaves the item marked live (+0x48, set by SetMenuItem2) with its last
// input state (+0x30 / +0x34 / +0x38). MenuItem::ExecInput2 trusts +0x48 and sends to the port. TownImageMap::execute calls ExecInput2(gMI_CityMap) whenever the window is open, but
// TownImageMap::open only sets gMI_CityMap up when the place has a map icon; a place with a shop list and no map (Chapter 2 Colosseum) opens the window with gMI_CityMap still as the last
// term() left it (the world map, FieldImageMap, and the town maps use the same item), so ExecInput2 calls MenuPort::Send(NULL) -> data abort at address 4 (rc3 crash dump).
// term() is replaced by the same code plus a reset of the closed item to the state of an item never set up (+0x48 = 0, input state 0): ExecInput2 then skips it and returns 0.
// The deferred close (+0x52: the item asks the port to close itself) is unchanged. A build whose term() differs is left alone.
static void (*s_portSend)(void* port, int count, ...);
static void (*s_portClose)(void* port);

static void MenuItemTerm(uint8_t* item) {
  void* port = *(void**)(item + 0x3c);
  if (!port) return;
  if (item[0x52]) { s_portSend(port, 2, 7, 0); return; }
  s_portClose(port);
  *(void**)(item + 0x3c) = nullptr;
  *(int16_t*)(item + 0x48) = 0;
  *(int32_t*)(item + 0x30) = 0;
  *(int32_t*)(item + 0x34) = 0;
  *(int32_t*)(item + 0x38) = 0;
}

void FixMenuItemTerm() {
  uintptr_t term = loader_symbol("dq4core", "_ZN8MenuItem4termEv");
  s_portSend = reinterpret_cast<decltype(s_portSend)>(loader_symbol("dq4core", "_ZN8MenuPort4SendEsz"));
  s_portClose = reinterpret_cast<decltype(s_portClose)>(loader_symbol("dq4core", "_ZN8MenuPort5CloseEv"));
  if (!term || !s_portSend || !s_portClose) { l_warn("FixMenuItemTerm: MenuItem::term / MenuPort::Send / MenuPort::Close not found"); return; }
  // the whole original body (0x813fc..0x81438): checked so that a different build is not replaced by code that does not match it
  static const uint32_t kTerm[16] = {
    0xe92d4070u, 0xe1a04000u, 0xe590003cu, 0xe3500000u, 0x0a000004u, 0xe5d45052u, 0xe3550000u, 0x1a000002u,
    0xeb000b23u, 0xe584503cu, 0xe8bd8070u, 0xe3a01002u, 0xe3a02007u, 0xe3a03000u, 0xe8bd4070u, 0xea0009bau,
  };
  for (int i = 0; i < 16; i++) if (((volatile uint32_t*)term)[i] != kTerm[i]) { l_info("FixMenuItemTerm: word %08x at term+%x is not the expected one, not patching", ((volatile uint32_t*)term)[i], i * 4); return; }
  const uint32_t jump[2] = { 0xe51ff004u, (uint32_t)(uintptr_t)&MenuItemTerm };   // ldr pc, [pc, #-4]; .word MenuItemTerm (bit 0 selects Thumb)
  kuKernelCpuUnrestrictedMemcpy((void*)term, jump, sizeof(jump));
  kuKernelFlushCaches((void*)term, sizeof(jump));
  l_success("MenuItem::term resets the closed item (town map with a shop list and no map)");
}

}  // namespace NativeCall

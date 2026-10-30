// game/FieldTest.hpp -- OWNER: WP5c (dev aid).  World-map viewer: if ux0:data/dqiv/fieldtest<slot>.on exists, the game boots normally (libdq4core is initialised, so the native
// FieldSet() runs for real) but NativeManager.Update is held paused and a driver calls Field::Init / SetPosition / SetIndexList / SetSymbol / SetScale itself.
// File lines: "<col0> <row0> [scale] [frames]": the 25x40 grid shows the block of the Field/field atlas starting at tile (col0,row0) (tile id = row*64 + col); symbols 0..41 are laid out
// in a grid next to the map. Scale > 1 zooms out (the native FieldSet then gets the 0x10000000 flag).
#pragma once
// DQIV_DEV_TOOLS (CMake option, ON for Debug / OFF for Release): without it this dev aid is compiled out and the calls are no-ops.
#ifdef DQIV_DEV_TOOLS
namespace FieldTest { bool Requested(); void Create(); }
#else
namespace FieldTest { inline bool Requested() { return false; } inline void Create() {} }
#endif

// game/FldTest.hpp -- OWNER: WP5b (dev aid).  Field-map viewer: if ux0:data/dqiv/fldtest<slot>.on exists, the game boots straight into FldObject instead of the title:
// each line of the file is "<map path without extension> [dispScale] [frames] [mapCamera 0=free orbit] [elevation]" (e.g. "map/c/ca/in/ca2f1a 6 150"); maps are shown in order for `frames` frames each
// (default 120) and the list repeats. A free camera frames the model bounds (or plays the map's own EMV camera 1 when it has one). Used with shot_frames in dqivn<slot>.cfg.
#pragma once
// DQIV_DEV_TOOLS (CMake option, ON for Debug / OFF for Release): without it this dev aid is compiled out and the calls are no-ops.
#ifdef DQIV_DEV_TOOLS
namespace FldTest { bool Requested(); void Create(); }
#else
namespace FldTest { inline bool Requested() { return false; } inline void Create() {} }
#endif

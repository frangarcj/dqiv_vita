// game/CharaTest.hpp -- OWNER: WP5a (dev aid).  Billboard sprite viewer: if ux0:data/dqiv/charatest<slot>.on exists, the game boots straight into a test scene
// (SceneCamera town camera + a grid of Chara sprites with shadows, one lying (sleep pose) and one Riseup-less) instead of the title. No native calls.
// File content (optional): "<first CharaTex index> <count>" (default "1 12").  Use with shot_frames in dqivn<slot>.cfg.
#pragma once
// DQIV_DEV_TOOLS (CMake option, ON for Debug / OFF for Release): without it this dev aid is compiled out and the calls are no-ops.
#ifdef DQIV_DEV_TOOLS
namespace CharaTest { bool Requested(); void Create(); }
#else
namespace CharaTest { inline bool Requested() { return false; } inline void Create() {} }
#endif

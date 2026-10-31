// game/FxTest.hpp -- OWNER: WP6 (dev aid).  Screen-effect test scene: if ux0:data/dqiv/fxtest<slot>.on exists the game boots into a checkerboard scene (MainCamera + MenuCamera +
// Fade + Capture, no native calls) and triggers one effect.  File content: "<effect> [start frame]" with effect one of
//   battle | darklamp | morning | riremito | tabi | tabirev | buttons | all (the six effects in turn; the DQ4 Capture has exactly these)
// Use with shot_frames in dqivn<slot>.cfg (tools/run_vita3k.sh: FXTEST="battle 40" SHOT_FRAMES=...).
#pragma once
// DQIV_DEV_TOOLS (CMake option, ON for Debug / OFF for Release): without it this dev aid is compiled out and the calls are no-ops.
#ifdef DQIV_DEV_TOOLS
namespace FxTest { bool Requested(); void Create(); }
#else
namespace FxTest { inline bool Requested() { return false; } inline void Create() {} }
#endif

// game/DemoOverlay.hpp -- OWNER: WP2 (dev aid).  Phase-A proof of the pack -> GL path: a camera that clears the logical surface to a colour
// and a quad textured with boot/retro5_copyright, rendered at the bottom of the camera stack (depth -100). Remove when the real UI draws.
#pragma once
// DQIV_DEV_TOOLS (CMake option, ON for Debug / OFF for Release): without it this dev aid is compiled out and the calls are no-ops.
#ifdef DQIV_DEV_TOOLS
namespace DemoOverlay { void Create(); }
#else
namespace DemoOverlay { inline void Create() {} }
#endif

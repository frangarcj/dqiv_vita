// game/ui_weak_stubs.cpp -- OWNER: WP4a.  WEAK default definitions of the port factories MenuPort::CreateMenuPort calls for categories owned by WP6
// (3 EndrollPort, 4 NetConnection, 5 SlimeTouchPort). WP6 provides strong definitions in its own files, e.g.
//   MenuPort* dqiv_CreateEndrollPort(short port, short type) { return new EndrollPort(port, type); }
// (TouchPad::GetPressRay, ApplicationQuit::Create and Fade::SetFadePriority, which used to be weak here, are now real in WP3/WP5a files.)
#include "game/MenuPort.hpp"
#include "utils/logger.h"

#define WEAK __attribute__((weak))

WEAK MenuPort* dqiv_CreateEndrollPort(short port, short type) { l_warn("STUB EndrollPort(port %d, type %d) (WP6)", (int)port, (int)type); return nullptr; }
WEAK MenuPort* dqiv_CreateSlimeTouchPort(short port, short type) { l_warn("STUB SlimeTouchPort(port %d, type %d) (WP6)", (int)port, (int)type); return nullptr; }

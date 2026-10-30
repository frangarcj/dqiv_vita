// game/ui_weak_stubs.cpp -- OWNER: WP4a.  WEAK default definitions of the port factories MenuPort::CreateMenuPort calls for categories owned by WP6
// (3 StaffrollPort, 4 NetConnection). WP6 provides strong definitions in its own files, e.g.
//   MenuPort* dqiv_CreateEndrollPort(short port, short type) { return new EndrollPort(port, type); }
// (TouchPad::GetPressRay, ApplicationQuit::Create and Fade::SetFadePriority, which used to be weak here, are now real in WP3/WP5a files.)
#include "game/MenuPort.hpp"
#include "utils/logger.h"

#define WEAK __attribute__((weak))

WEAK MenuPort* dqiv_CreateStaffrollPort(short port) { l_warn("UNPORTED StaffrollPort(port %d): category 3 requested by the native, not delta-ported yet (DQ4 Staffroll/StaffrollPort)", (int)port); return nullptr; }

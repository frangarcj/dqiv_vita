// game/MenuPortItemTable_unported.cpp -- OWNER: WP4b.  Placeholders for the screens that are not ported yet (see the recipe in MenuPortItemTable.hpp).
// Each stub logs once and returns an empty MenuPortItem, so the port exists and the native messages are consumed (SetItemParam/Code report the
// missing tables with LogError). Port a screen => move it to its own file and delete its line here.
#include "game/MenuPortItemTable.hpp"
#include "utils/logger.h"

#define UNPORTED(NAME)                                                                                         \
  MenuPortItem* MenuPortItemTable::NAME(short port, short type) {                                              \
    static bool once_;                                                                                         \
    if (!once_) { once_ = true; l_warn("UNPORTED screen " #NAME " (port %d, subtype %d)", (int)port, (int)type); } \
    return new MenuPortItem(port, type);                                                                       \
  }


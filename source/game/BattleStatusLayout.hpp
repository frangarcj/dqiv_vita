// game/BattleStatusLayout.hpp -- landscape battles: the party status row (MenuPortItemTable::Make_BattleUpStatus) is drawn LARGER by its own full-surface UI camera and centred
// over the visible world area instead of being clipped into the 408 px panel (re/arch/07_LANDSCAPE.md §10, DQIV-Recompiled battle layout).
// cfg: battle_face_scale = percent for the face icons only (default 100 = off; > 100 is clipped by the screen top, see 07 §11).
#pragma once
#include "miniunity.hpp"
using namespace miniunity;

namespace BattleStatusLayout {
bool Enabled();                                  // Layout::Wide() && battle_layout != band
void Attach(Transform* root, int memberFrames);  // root = the dummy parent of the 4 status frames (children 0..3 are the frames)
}

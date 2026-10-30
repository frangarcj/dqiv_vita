// game/BattleStatusLayout.cpp -- see BattleStatusLayout.hpp.
#include "game/BattleStatusLayout.hpp"

#include <algorithm>
#include <cmath>
#include "game/Layout.hpp"
#include "game/MenuParts.hpp"
#include "game/UiCameras.hpp"
#include "platform/cfg.hpp"
#include "utils/logger.h"

namespace {
constexpr float kPpu = 544.f / 600.f;            // surface pixels per UI unit (the 408x544 tablet canvas: 450 x 600 units)
class StatusPlacer : public Component {
public:
  // DQIV-style status row (right side, panel): the whole row (faces, boxes, action labels) is moved down by battle_status_y px (default 45) and the face icons are
  // enlarged (battle_face_scale, default 250 %): each is centred over its own box with its bottom edge on the box top, so it grows upward into the room the shift makes.
  // The action labels under the boxes ("Follow Orders", "Attack / Healslime") were drawn over black in portrait; over the bright landscape scene they get a dark
  // window plate (the game's own variable window, alpha 0.78) sized to the text.
  Transform* root = nullptr; int frames = 4; float faceScale = 1.f, downPx = 0.f; bool init = false; bool faceDone[4] = {};
  Transform* cursor = nullptr; Vector3 cursorLast, cursorBase; bool cursorHave = false;      // the active-member arrow (BattleUpCursor), drawn by the Status camera so it can sit beside the enlarged face
  TextMesh* label[4] = {}; MenuParts* plate[4] = {}; int plateLines[4] = {-1, -1, -1, -1}, plateW[4] = {};
  void Update() override {
    if (!root) return;
    if (!init) {
      init = true;
      root->localPosition = Vector3(root->localPosition.x, root->localPosition.y, root->localPosition.z - downPx / kPpu);
      for (int i = 0; i < frames && i < root->childCount(); i++) {
        Transform* f = root->GetChild(i);
        for (Transform* c : f->children()) {
          TextMesh* tm = c->gameObject->GetComponent<TextMesh>();
          if (!faceDone[i] && !tm && std::fabs(c->localPosition.z + 15.5f) < 0.2f && faceScale > 1.001f) {   // the face icon (Sub_BattleUpStatus: menuParts2->Build(4, 0, 57 + num, 0, -15.5))
            faceDone[i] = true;                  // about its own centre in x, keeping its bottom edge (28.8 units high icon)
            c->localScale = Vector3(c->localScale.x * faceScale, c->localScale.y, c->localScale.z * faceScale);
            c->localPosition = Vector3(c->localPosition.x, c->localPosition.y, c->localPosition.z + 14.4f * (faceScale - 1.f));
          }
          if (tm && !label[i] && std::fabs(c->localPosition.z + 141.f) < 2.5f) label[i] = tm;       // the action label (Sub_BattleUpStatus: AddTextMesh(57.5, 4, -145))
        }
        if (label[i]) {
          plate[i] = MenuParts::CreateBuildVariableWindow(0, 100.f, 30.f, 4, false);
          plate[i]->transform()->SetParent(f, false);
          plate[i]->gameObject->setActive(false);
        }
      }
      if (root->childCount() > frames) {         // Make_BattleUpStatus builds the cursor right after the frames
        cursor = root->GetChild(frames);
        UiCameras::Status();
        UiCameras::Retag(cursor, UiCameras::kStatusLayer);
      }
    }
    if (cursor) {
      // the native code writes localPosition (-200 + 111 code, 0, -15) when the active member changes; add our offset on top each frame (detect a fresh write)
      Vector3 cur = cursor->localPosition;
      if (!cursorHave || cur.x != cursorLast.x || cur.z != cursorLast.z) { cursorBase = cur; cursorHave = true; }
      float panelCentrePx = Layout::UiPixelX0() + Layout::UiW() * 0.5f, comp = (panelCentrePx - Layout::SurfaceW() * 0.5f) / kPpu;       // Status camera is centred on the surface, the panel is not
      Vector3 mine(cursorBase.x + comp - 14.4f * (faceScale - 1.f) * 1.35f, cursorBase.y, cursorBase.z + 14.4f * (faceScale - 1.f));       // left of the enlarged face, level with its middle
      cursor->localPosition = mine; cursorLast = mine;
    }
    for (int i = 0; i < frames; i++) {
      if (!label[i] || !plate[i]) continue;
      const std::string& t = label[i]->text;
      bool show = !t.empty() && label[i]->gameObject->activeInHierarchy();
      if (!show) { plate[i]->gameObject->setActive(false); continue; }
      int lines = 1, cur = 0, mx = 0;
      for (char ch : t) { if (ch == '\n') { lines++; cur = 0; } else if (((unsigned char)ch & 0xC0) != 0x80) { cur++; mx = std::max(mx, cur); } }
      int w = std::min(104, std::max(56, mx * 6 + 16));
      if (lines != plateLines[i] || w != plateW[i]) {
        plateLines[i] = lines; plateW[i] = w;
        float h = 17.5f * lines + 8.f;
        plate[i]->VariableWindow(0, (float)w, h, 4);
        plate[i]->transform()->localPosition = Vector3(57.5f, 3.f, -144.f - h * 0.5f);
      }
      plate[i]->gameObject->setActive(true);
    }
  }
};
}  // namespace

namespace BattleStatusLayout {
bool Enabled() { return Layout::Wide() && Layout::GetBattleLayout() != Layout::BattleLayout::Band; }
void Attach(Transform* root, int memberFrames) {
  float fs = std::min(300, std::max(100, Cfg::GetInt("battle_face_scale", 250))) / 100.f;
  float down = (float)std::min(120, std::max(0, Cfg::GetInt("battle_status_y", 45)));
  StatusPlacer* p = root->gameObject->AddComponent<StatusPlacer>();
  p->root = root; p->frames = memberFrames; p->faceScale = fs; p->downPx = down;
}
}  // namespace BattleStatusLayout

#include "platform/vita_input.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <psp2/ctrl.h>
#include <psp2/kernel/clib.h>
#include <psp2/touch.h>

#include "config.h"
#include "miniunity.hpp"
#include "pack/dqiv_pack.h"
#include "platform/cfg.hpp"
#include "game/Layout.hpp"
#include "game/UiCameras.hpp"
#include "utils/logger.h"

using namespace miniunity;
const char* DqvCurrentScene();                 // game/NativeManager.cpp: last map loaded by the native side (script `sync`)
bool DqvInBattle();                           // game/NativeManager.cpp: the battle stage is up (script `roam`)
bool DqvHeroPos(float* x, float* y, float* z); // game/NativeManager.cpp: last position the native sent for character 0 (script `walk`)

/*
 * Input script (headless emulator runs: Vita3K key injection does not reach the guest).  File: ux0:data/dqiv/input_script<slot>.txt (read when it exists).
 * One event per line, `#` comments.  <frame> = number of rendered frames so far (frame 0 = first Poll).  Buttons: up down left right l r cross circle triangle
 * square start select r3.  Touch coordinates are UI-PANEL pixels (408x544, origin bottom-left; in landscape the panel offset is added), so portrait scripts keep working.
 *     <F> down  <btn> [<btn>...]        hold buttons from frame F      <F> up <btn>...          release them
 *     <F> tap   <btn>[,<btn>] [n=2]     press for n frames              <F>-<T>/<P> tap <btn>    repeat every P frames from F to T
 *     <F> touch <x> <y> [n=3]           finger down at (x,y) for n frames and lift (a tap)      <F>-<T>/<P> touch <x> <y> [n]
 *     <F> drag  <x0> <y0> <x1> <y1> [n=10]   finger swipe             <F> stick <lx> <ly> [<rx> <ry>]   analog sticks 0..255 (128 = centre)
 *     <F> sync  <text> [tap <btn> <period>]                   hold the script until NativeManager's last loaded map path contains <text>; later events are then relative to that moment
 *     <F> walk  <x> <z> [tol=0.5] [timeout=600] [map]   closed loop with the d-pad only: hold the directions that bring the hero (position as sent by the native, `heropos` units)
 *                                     towards (x,z) until within tol (or `timeout` frames, or the scene no longer contains [map]); later events are relative to that moment.  Screen up = +z, right = +x (calibrated: d-pad Up raises z)
 *     <F> roam <timeout> [period=40]    walk about with the d-pad (down, right, up, left bursts of <period> frames) until a random battle starts; during the battle only Cross is tapped (every 20 frames);
 *                                     ends 120 frames after the battle stage went away again (victory) or at the timeout; later events are relative to that moment
 *     <F> autotap <btn> <period>        press <btn> for 2 frames every <period> frames from now on (0 = off), also while `walk`/`sync` hold the script
 *     <F> shot  <label>                 ask Vita3K for a screenshot now (file dqivn<slot>_s<frame>_<label>.jpg; the run scripts collect dqivn<slot>_*)
 *     <F> log   <text>                  write a marker line to the log   <F> quit                  Application.Quit() (clean end of a headless run)
 */
namespace vita_input {

static State g_state;
static unsigned g_prevButtons;
static bool g_swapConfirm;
static bool g_preGame = true;
static float g_injX, g_injY; static int g_injFrames = 0;
static int g_synthTap = 0;              // frames left of a synthesised centre tap (pre-game face button)

// touch state machine (06 §2)
static bool g_active;            // a finger is "down" from the game's point of view
static bool g_pendingEnd;        // tap shorter than a frame: Ended is emitted next frame
static Vector2 g_pos, g_prevPos, g_beganPos;
static float g_moved;
struct Sample { bool down; float x, y; };

// ------------------------------------------------------------------------------------------------ script
enum ActType { A_BTN_ON, A_BTN_OFF, A_TOUCH, A_TOUCH_UP, A_STICK, A_LOG, A_QUIT, A_SYNC, A_WALK, A_AUTOTAP, A_ROAM, A_SHOT };
struct Act { int frame; ActType type; unsigned mask; float x, y; int a, b; std::string text; };
static std::vector<Act> g_script;
static size_t g_scriptNext;
static int g_scriptBase;                       // `sync`: events after it are relative to the frame the condition became true
static bool g_scriptWait;
static unsigned g_autoMask; static int g_autoPer;               // `autotap`: press a button every N frames whatever else the script is doing (dismiss messages while walking)
static bool g_roamOn, g_roamSeen; static int g_roamStart, g_roamOut;
static bool g_walkOn; static int g_walkStart, g_walkLast;       // `walk` in progress
static unsigned g_sButtons;                    // buttons held by the script
static bool g_sTouchDown; static float g_sX, g_sY;
static int g_sLx = 128, g_sLy = 128, g_sRx = 128, g_sRy = 128;

static unsigned ButtonByName(const std::string& n) {
  static const struct { const char* n; unsigned b; } t[] = {
    {"up", SCE_CTRL_UP}, {"down", SCE_CTRL_DOWN}, {"left", SCE_CTRL_LEFT}, {"right", SCE_CTRL_RIGHT}, {"l", SCE_CTRL_LTRIGGER}, {"r", SCE_CTRL_RTRIGGER},
    {"cross", SCE_CTRL_CROSS}, {"circle", SCE_CTRL_CIRCLE}, {"triangle", SCE_CTRL_TRIANGLE}, {"square", SCE_CTRL_SQUARE}, {"start", SCE_CTRL_START},
    {"select", SCE_CTRL_SELECT}, {"r3", SCE_CTRL_R3}, {"l3", SCE_CTRL_L3},
  };
  for (auto& e : t) if (n == e.n) return e.b;
  return 0;
}
static unsigned ParseButtons(const std::string& s) {      // "cross,circle"
  unsigned m = 0; size_t p = 0;
  while (p < s.size()) { size_t q = s.find(',', p); if (q == std::string::npos) q = s.size(); m |= ButtonByName(s.substr(p, q - p)); p = q + 1; }
  return m;
}
static void Add(int f, ActType t, unsigned mask = 0, float x = 0, float y = 0, int a = 0, int b = 0, const char* text = "") { g_script.push_back({f, t, mask, x, y, a, b, text}); }

static float ScriptPanelX0() { return UiCameras::BootPhase() ? (Layout::SurfaceW() - Layout::UiW()) * 0.5f : Layout::UiPixelX0(); }

static void LoadScript() {
  char path[256];
  snprintf(path, sizeof(path), "%sinput_script%s.txt", DATA_PATH, DQIVN_SLOT_STR);
  FILE* f = fopen(path, "r");
  if (!f) return;
  char line[512]; int lineNo = 0;
  while (fgets(line, sizeof(line), f)) {
    lineNo++;
    char* h = strchr(line, '#'); if (h) *h = 0;
    std::vector<std::string> w; char* sv = nullptr;
    for (char* tk = strtok_r(line, " \t\r\n", &sv); tk; tk = strtok_r(nullptr, " \t\r\n", &sv)) w.push_back(tk);
    if (w.size() < 2) continue;
    int f0 = 0, f1 = 0, per = 0;
    const std::string& fs = w[0];
    if (sscanf(fs.c_str(), "%d-%d/%d", &f0, &f1, &per) == 3) {}
    else { f0 = f1 = atoi(fs.c_str()); per = 1; }
    if (per < 1) per = 1;
    const std::string& cmd = w[1];
    for (int fr = f0; fr <= f1; fr += per) {
      if (cmd == "down" || cmd == "up") {
        for (size_t i = 2; i < w.size(); i++) Add(fr, cmd == "down" ? A_BTN_ON : A_BTN_OFF, ParseButtons(w[i]));
      } else if (cmd == "tap" && w.size() >= 3) {
        unsigned m = ParseButtons(w[2]); int n = w.size() > 3 ? atoi(w[3].c_str()) : 2;
        Add(fr, A_BTN_ON, m); Add(fr + std::max(n, 1), A_BTN_OFF, m);
      } else if (cmd == "touch" && w.size() >= 4) {
        float x = (float)atof(w[2].c_str()), y = (float)atof(w[3].c_str()); int n = w.size() > 4 ? atoi(w[4].c_str()) : 3;
        Add(fr, A_TOUCH, 0, x, y); Add(fr + std::max(n, 1), A_TOUCH_UP);
      } else if (cmd == "drag" && w.size() >= 6) {
        float x0 = (float)atof(w[2].c_str()), y0 = (float)atof(w[3].c_str()), x1 = (float)atof(w[4].c_str()), y1 = (float)atof(w[5].c_str());
        int n = w.size() > 6 ? atoi(w[6].c_str()) : 10; n = std::max(n, 2);
        for (int i = 0; i < n; i++) Add(fr + i, A_TOUCH, 0, x0 + (x1 - x0) * i / (n - 1), y0 + (y1 - y0) * i / (n - 1));
        Add(fr + n, A_TOUCH_UP);
      } else if (cmd == "stick" && w.size() >= 4) {
        Add(fr, A_STICK, 0, 0, 0, atoi(w[2].c_str()) | (atoi(w[3].c_str()) << 8), w.size() >= 6 ? (atoi(w[4].c_str()) | (atoi(w[5].c_str()) << 8)) : (128 | (128 << 8)));
      } else if (cmd == "log") {
        std::string t; for (size_t i = 2; i < w.size(); i++) { if (i > 2) t += ' '; t += w[i]; }
        Add(fr, A_LOG, 0, 0, 0, 0, 0, t.c_str());
      } else if (cmd == "sync" && w.size() >= 3) {
        unsigned m = 0; int per = 0;                                      // optional: `sync <text> tap <btn> <period>` taps while waiting
        if (w.size() >= 6 && w[3] == "tap") { m = ParseButtons(w[4]); per = atoi(w[5].c_str()); }
        Add(fr, A_SYNC, m, 0, 0, per, 0, w[2].c_str());
      } else if (cmd == "walk" && w.size() >= 4) {
        float tol = w.size() > 4 ? (float)atof(w[4].c_str()) : 0.5f; int to = w.size() > 5 ? atoi(w[5].c_str()) : 600;
        Add(fr, A_WALK, 0, (float)atof(w[2].c_str()), (float)atof(w[3].c_str()), to, (int)(tol * 1000.f), w.size() > 6 ? w[6].c_str() : "");
      } else if (cmd == "roam" && w.size() >= 3) {
        Add(fr, A_ROAM, 0, 0, 0, atoi(w[2].c_str()), w.size() > 3 ? atoi(w[3].c_str()) : 40);
      } else if (cmd == "autotap" && w.size() >= 4) {
        Add(fr, A_AUTOTAP, ParseButtons(w[2]), 0, 0, atoi(w[3].c_str()));
      } else if (cmd == "shot" && w.size() >= 3) {
        Add(fr, A_SHOT, 0, 0, 0, 0, 0, w[2].c_str());
      } else if (cmd == "quit") {
        Add(fr, A_QUIT);
      } else { l_warn("input script %s:%d: cannot parse '%s'", path, lineNo, cmd.c_str()); break; }
    }
  }
  fclose(f);
  std::stable_sort(g_script.begin(), g_script.end(), [](const Act& a, const Act& b) { return a.frame < b.frame; });
  l_success("input script %s: %u events, last at frame %d", path, (unsigned)g_script.size(), g_script.empty() ? 0 : g_script.back().frame);
}

static void RunScript(int frame) {
  if (g_autoPer > 0 && g_autoMask) { static unsigned held; if (frame % g_autoPer == 0) held = g_autoMask, g_sButtons |= held; else if (frame % g_autoPer == 2) g_sButtons &= ~g_autoMask; }
  while (g_scriptNext < g_script.size() && g_script[g_scriptNext].frame + g_scriptBase <= frame) {
    const Act& a = g_script[g_scriptNext];
    if (a.type == A_SYNC) {                    // `<F> sync <map substring>`: hold the script until the native side loaded a map whose path contains the text
      if (strstr(::DqvCurrentScene(), a.text.c_str())) { g_scriptBase = frame - a.frame; g_scriptWait = false; l_info("SCRIPT f%d: sync '%s' reached", frame, a.text.c_str()); g_scriptNext++; continue; }
      if (!g_scriptWait) { g_scriptWait = true; l_info("SCRIPT f%d: waiting for scene '%s'", frame, a.text.c_str()); }
      if (a.a > 0 && a.mask) { if (frame % a.a == 0) g_sButtons |= a.mask; else if (frame % a.a == 2) g_sButtons &= ~a.mask; }
      return;
    }
    if (a.type == A_WALK) {                    // closed loop on the hero position: only ever sets d-pad bits
      const unsigned dirs = SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT;
      if (!g_walkOn) { g_walkOn = true; g_walkStart = frame; g_walkLast = frame; l_info("SCRIPT f%d: walk to (%d, %d) /100", frame, (int)(a.x * 100.f), (int)(a.y * 100.f)); }
      float px, py, pz; bool have = ::DqvHeroPos(&px, &py, &pz);
      float tol = a.b / 1000.f, dx = a.x - px, dz = a.y - pz;
      bool wrongMap = !a.text.empty() && !strstr(::DqvCurrentScene(), a.text.c_str());
      bool done = have && std::fabs(dx) <= tol && std::fabs(dz) <= tol;
      if (done || wrongMap || frame - g_walkStart > a.a) {
        l_info("SCRIPT f%d: walk %s at (%d, %d) /100 target (%d, %d) scene '%s'", frame, done ? "reached" : wrongMap ? "left the map" : "timeout", (int)(px * 100.f), (int)(pz * 100.f), (int)(a.x * 100.f), (int)(a.y * 100.f), ::DqvCurrentScene());     // ints: Vita3K's sceClibVsnprintf crashes on several double varargs
        g_sButtons &= ~dirs; g_walkOn = false; g_scriptBase = frame - a.frame; g_scriptNext++; continue;
      }
      g_sButtons &= ~dirs;
      if (have) {
        if (dx > tol * 0.5f) g_sButtons |= SCE_CTRL_RIGHT; else if (dx < -tol * 0.5f) g_sButtons |= SCE_CTRL_LEFT;
        if (dz > tol * 0.5f) g_sButtons |= SCE_CTRL_UP; else if (dz < -tol * 0.5f) g_sButtons |= SCE_CTRL_DOWN;      // calibrated in mlb1a: Up raises z, Right raises x
      }
      if (frame - g_walkLast >= 120) { g_walkLast = frame; l_info("SCRIPT f%d: walking, hero (%d, %d) /100, scene '%s'", frame, (int)(px * 100.f), (int)(pz * 100.f), ::DqvCurrentScene()); }
      return;
    }
    if (a.type == A_ROAM) {
      const unsigned dirs = SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT;
      if (!g_roamOn) { g_roamOn = true; g_roamSeen = false; g_roamStart = frame; g_roamOut = 0; l_info("SCRIPT f%d: roam until a battle (timeout %d)", frame, a.a); }
      bool inB = ::DqvInBattle();
      if (inB) { if (!g_roamSeen) l_info("SCRIPT f%d: battle started", frame); g_roamSeen = true; g_roamOut = 0; } else if (g_roamSeen) g_roamOut++;
      if ((g_roamSeen && g_roamOut > 120) || frame - g_roamStart > a.a) {
        l_info("SCRIPT f%d: roam %s (scene '%s')", frame, g_roamSeen ? "battle fought, back on the map" : "timeout, no battle", ::DqvCurrentScene());
        g_sButtons &= ~(dirs | SCE_CTRL_CROSS); g_roamOn = false; g_scriptBase = frame - a.frame; g_scriptNext++; continue;
      }
      g_sButtons &= ~(dirs | SCE_CTRL_CROSS);
      if (inB) { if (frame % 20 < 2) g_sButtons |= SCE_CTRL_CROSS; }
      else if (!g_roamSeen) { static const unsigned seq[4] = { SCE_CTRL_DOWN, SCE_CTRL_RIGHT, SCE_CTRL_UP, SCE_CTRL_LEFT }; g_sButtons |= seq[((frame - g_roamStart) / std::max(a.b, 1)) & 3]; }
      return;
    }
    g_scriptNext++;
    switch (a.type) {
      case A_BTN_ON: g_sButtons |= a.mask; break;
      case A_BTN_OFF: g_sButtons &= ~a.mask; break;
      case A_TOUCH: g_sTouchDown = true; g_sX = a.x + ScriptPanelX0(); g_sY = a.y; break;   // script x = UI-panel pixels; the panel is centred during the start flow, else at the right
      case A_TOUCH_UP: g_sTouchDown = false; break;
      case A_STICK: g_sLx = a.a & 0xFF; g_sLy = (a.a >> 8) & 0xFF; g_sRx = a.b & 0xFF; g_sRy = (a.b >> 8) & 0xFF; break;
      case A_AUTOTAP: g_autoMask = a.mask; g_autoPer = a.a; if (!a.a) g_sButtons &= ~a.mask; break;
      case A_LOG: l_info("SCRIPT f%d: %s", frame, a.text.c_str()); break;
      case A_SHOT: { char msg[96]; snprintf(msg, sizeof(msg), "VITA3K_SCREENSHOT dqivn%d_s%05d_%s", (int)DQIVN_SLOT, frame, a.text.c_str()); sceClibPrintf("%s\n", msg); l_info("SCRIPT f%d: screenshot %s", frame, a.text.c_str()); break; }
      case A_QUIT: l_info("SCRIPT f%d: quit", frame); Application::Quit(); break;
      default: break;
    }
  }
}
bool ScriptActive() { return g_scriptNext < g_script.size() || g_sButtons || g_sTouchDown; }

// ------------------------------------------------------------------------------------------------ pointer cursor (Select)
static GameObject* g_cursor[2];
static void ShowCursor(bool on, float x, float y) {
  if (on && !g_cursor[0]) {
    GameObject* camGo = new GameObject("PointerCamera");
    Camera* cam = camGo->AddComponent<Camera>();
    cam->orthographic = true; cam->orthographicSize = Screen::height * 0.5f; cam->nearClipPlane = 0.3f; cam->farClipPlane = 1000.f;
    cam->clearFlags = CameraClearFlags::Depth; cam->depth = 1000.f; cam->cullingMask = 1 << 13;
    Object::DontDestroyOnLoad(camGo);
    static const uint8_t px[16] = { 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255 };
    Texture2D* white = Texture2D::FromPack("pointer_white", 2, 2, DQIV_TF_RGBA8888, 1, 0, 4, px);   // 2x2 white: Custom/Picture = texture * tint (FrontFade is a clip-space shader)
    for (int i = 0; i < 2; i++) {
      GameObject* q = GameObject::CreatePrimitive(PrimitiveType::Quad);
      q->name = i ? "PointerDot" : "PointerRing";
      q->layer = 13;
      float s = i ? 9.f : 15.f;
      q->transform->localScale = Vector3(s, s, 1.f);
      Material* m = new Material(Shader::Find("Custom/Picture"));
      m->mainTexture = white;
      m->color = i ? Color(1.f, 0.9f, 0.1f, 0.95f) : Color(0.f, 0.f, 0.f, 0.9f);
      q->renderer()->setMaterial(m);
      Object::DontDestroyOnLoad(q);
      g_cursor[i] = q;
    }
  }
  for (int i = 0; i < 2; i++) {
    if (!g_cursor[i]) continue;
    g_cursor[i]->renderer()->enabled = on;
    g_cursor[i]->transform->localPosition = Vector3(x - Screen::width * 0.5f, y - Screen::height * 0.5f, 10.f + (i ? -1.f : 0.f));
  }
}

// ------------------------------------------------------------------------------------------------ init / physical input
void Init() {
  sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
  sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
  sceTouchEnableTouchForce(SCE_TOUCH_PORT_FRONT);
  g_swapConfirm = Cfg::Get("confirm") == "circle";
  g_state.pointerX = Layout::SurfaceW() * 0.5f; g_state.pointerY = Layout::SurfaceH() * 0.5f;
  LoadScript();
}
void SetPreGame(bool pre) { g_preGame = pre; }
void InjectTap(float x, float y, int holdFrames) { g_injX = x; g_injY = y; g_injFrames = holdFrames; }

bool TouchToLogical(int rawX, int rawY, float* lx, float* ly) {
  float sx = (float)(rawX / 2), sy = (float)(rawY / 2);           // display pixels (integer division: raw 1087 -> 543), origin top-left
  float x = (sx - Layout::ViewX0());
  float y = (float)(Layout::SurfaceH() - 1) - (sy - Layout::ViewY0());      // Unity origin bottom-left
  if (lx) *lx = x;
  if (ly) *ly = y;
  return x >= 0 && x < Layout::SurfaceW() && y >= 0 && y < Layout::SurfaceH();
}

// 8-way direction from a stick vector (deadzone 56/128, 22.5 degree sectors like c_padDirTable)
static int StickDirs(int dx, int dy /* dy: +down as on the hardware */) {
  if (dx * dx + dy * dy < 56 * 56) return 0;
  float a = std::atan2((float)-dy, (float)dx) * 57.29578f;      // 0 = right, 90 = up
  if (a < 0) a += 360.f;
  int sector = (int)((a + 22.5f) / 45.f) & 7;                    // 0 R, 1 UR, 2 U, 3 UL, 4 L, 5 DL, 6 D, 7 DR
  static const int table[8] = { PAD_R, PAD_U | PAD_R, PAD_U, PAD_U | PAD_L, PAD_L, PAD_D | PAD_L, PAD_D, PAD_D | PAD_R };
  return table[sector];
}

static int ButtonsToPad(unsigned b, int lx, int ly, int rx) {
  int held = 0;
  if (b & SCE_CTRL_UP) held |= PAD_U;
  if (b & SCE_CTRL_DOWN) held |= PAD_D;
  if (b & SCE_CTRL_LEFT) held |= PAD_L;
  if (b & SCE_CTRL_RIGHT) held |= PAD_R;
  held |= StickDirs(lx - 128, ly - 128);
  if (b & SCE_CTRL_LTRIGGER) held |= PAD_ROT_L;
  if (b & SCE_CTRL_RTRIGGER) held |= PAD_ROT_R;
  if ((b & SCE_CTRL_LTRIGGER) && (b & SCE_CTRL_RTRIGGER)) held |= PAD_ROT_RESET;
  if (b & SCE_CTRL_R3) held |= PAD_ROT_RESET;                     // reset camera to north
  int rxd = rx - 128;
  if (rxd > 56) held |= PAD_ROT_R; else if (rxd < -56) held |= PAD_ROT_L;
  if (b & SCE_CTRL_CROSS) held |= g_swapConfirm ? PAD_B : PAD_A;  // confirm (config confirm=circle swaps with Circle)
  if (b & SCE_CTRL_CIRCLE) held |= g_swapConfirm ? PAD_A : PAD_B; // cancel / talk
  if (b & SCE_CTRL_TRIANGLE) held |= PAD_X;                       // command menu
  if (b & SCE_CTRL_SQUARE) held |= PAD_Y;                         // area map
  return held;
}

// Unity touch synthesis from this frame's samples (06 §2). Poll once per frame and latch: a down+up between two polls still yields Began / Ended.
static void FeedTouch(const Sample* smp, int n) {
  bool sawDown = false, downNow = false;
  Vector2 firstDown, last = g_pos;
  for (int s = 0; s < n; s++) {
    bool d = smp[s].down;
    if (d) {
      if (!sawDown && !g_active) firstDown = Vector2(smp[s].x, smp[s].y);
      sawDown = true; last = Vector2(smp[s].x, smp[s].y);
    }
    downNow = d;
  }
  g_state.tapToggle = 0;
  Input::touchCount = 0;
  Touch& t = Input::touches[0];
  float dt = Time::deltaTime;
  const float tapSlop = 8.f * (float)Layout::UiW() / 450.f;       // TouchPad.s_AnyThreshold = 8 UI units (450 units across the surface)
  if (g_pendingEnd) {                      // second half of a sub-frame tap
    g_pendingEnd = false; g_active = false;
    t.phase = TouchPhase::Ended; t.position = g_pos; t.deltaPosition = Vector2(); t.deltaTime = dt; t.tapCount = 1; Input::touchCount = 1;
    if (g_moved < tapSlop) g_state.tapToggle = PAD_ANY;
    return;
  }
  if (!g_active) {
    if (!sawDown) return;
    g_active = true; g_pos = firstDown; g_prevPos = firstDown; g_beganPos = firstDown; g_moved = 0;
    t.phase = TouchPhase::Began; t.position = g_pos; t.deltaPosition = Vector2(); t.deltaTime = dt; t.tapCount = 1; Input::touchCount = 1;
    if (!downNow) { g_pendingEnd = true; if (last.x != g_pos.x || last.y != g_pos.y) g_pos = last; }
    return;
  }
  // finger was down at the previous frame
  g_prevPos = g_pos;
  if (downNow) g_pos = last;
  Vector2 delta = g_pos - g_prevPos;
  g_moved = std::fmax(g_moved, (g_pos - g_beganPos).magnitude());
  t.position = g_pos; t.deltaPosition = delta; t.deltaTime = dt; t.tapCount = 1; Input::touchCount = 1;
  if (downNow) {
    t.phase = (delta.x != 0.f || delta.y != 0.f) ? TouchPhase::Moved : TouchPhase::Stationary;
  } else {
    t.phase = TouchPhase::Ended; g_active = false;
    if (g_moved < tapSlop) g_state.tapToggle = PAD_ANY;      // released without moving => native maps toggle 0x100 to A
  }
}

void Poll() {
  const int frame = g_state.frame++;
  RunScript(frame);

  // ---- buttons: hardware | script
  SceCtrlData pad; memset(&pad, 0, sizeof(pad)); pad.lx = pad.ly = pad.rx = pad.ry = 128;
  if (sceCtrlPeekBufferPositive(0, &pad, 1) <= 0) { pad.buttons = 0; pad.lx = pad.ly = pad.rx = pad.ry = 128; }
  unsigned b = pad.buttons | g_sButtons;
  int lx = pad.lx, ly = pad.ly, rx = pad.rx;
  if (g_sLx != 128 || g_sLy != 128) { lx = g_sLx; ly = g_sLy; }
  if (g_sRx != 128) rx = g_sRx;
  unsigned edge = b & ~g_prevButtons;
  g_prevButtons = b;
  g_state.escapeDown = (edge & SCE_CTRL_START) != 0;
  g_state.pointerToggle = (edge & SCE_CTRL_SELECT) != 0;
  if (g_state.pointerToggle) {
    g_state.pointerMode = !g_state.pointerMode;
    l_info("pointer cursor %s", g_state.pointerMode ? "ON" : "OFF");
    ShowCursor(g_state.pointerMode, g_state.pointerX, g_state.pointerY);
  }

  // ---- touch samples of this frame
  Sample smp[10]; int ns = 0;
  if (g_state.pointerMode) {
    float dx = (float)(lx - 128) / 128.f, dy = (float)(ly - 128) / 128.f;
    if (std::fabs(dx) < 0.15f) dx = 0.f;
    if (std::fabs(dy) < 0.15f) dy = 0.f;
    if (b & SCE_CTRL_LEFT) dx -= 0.6f;
    if (b & SCE_CTRL_RIGHT) dx += 0.6f;
    if (b & SCE_CTRL_UP) dy -= 0.6f;
    if (b & SCE_CTRL_DOWN) dy += 0.6f;
    g_state.pointerX = std::fmin(std::fmax(g_state.pointerX + dx * 9.f, 0.f), (float)Layout::SurfaceW() - 1.f);
    g_state.pointerY = std::fmin(std::fmax(g_state.pointerY - dy * 9.f, 0.f), (float)Layout::SurfaceH() - 1.f);   // stick down = screen down = smaller y (origin bottom-left)
    ShowCursor(true, g_state.pointerX, g_state.pointerY);
    smp[ns++] = { (b & (SCE_CTRL_CROSS | SCE_CTRL_CIRCLE)) != 0, g_state.pointerX, g_state.pointerY };
    g_state.held = 0;
  } else {
    g_state.held = ButtonsToPad(b, lx, ly, rx);
    // pre-game (logos, title): a face button taps the screen centre for two frames
    if (g_preGame && (edge & (SCE_CTRL_CROSS | SCE_CTRL_CIRCLE | SCE_CTRL_TRIANGLE | SCE_CTRL_SQUARE))) g_synthTap = 2;
    if (g_synthTap > 0) { g_synthTap--; smp[ns++] = { true, Layout::SurfaceW() * 0.5f, Layout::SurfaceH() * 0.5f }; g_state.held = 0; }
  }
  if (ns == 0 && g_injFrames > 0) { smp[ns++] = { true, g_injX, g_injY }; g_injFrames--; }
  if (ns == 0 && g_sTouchDown) smp[ns++] = { true, g_sX, g_sY };
  if (ns == 0) {
    // sceTouchRead returns every sample since the last call (up to N): a down+up between two frames is not lost
    SceTouchData td[8];
    int n = sceTouchRead(SCE_TOUCH_PORT_FRONT, td, 8);
    for (int s = 0; s < n && ns < 10; s++) {
      if (td[s].reportNum > 0) {
        float lxp, lyp;
        bool inside = TouchToLogical(td[s].report[0].x, td[s].report[0].y, &lxp, &lyp);
        if (inside || g_active) smp[ns++] = { true, lxp, lyp };         // a touch that Began in the pillars never starts a game touch
        else smp[ns++] = { false, 0, 0 };
      } else smp[ns++] = { false, 0, 0 };
    }
  } else {
    SceTouchData td[8]; sceTouchRead(SCE_TOUCH_PORT_FRONT, td, 8);       // drain
  }
  FeedTouch(smp, ns);
  Input::escapeDown = g_state.escapeDown;
  // debug trace (Debug builds only, via l_info): input changes, so a headless run documents what the script / pad produced
  static int s_lastHeld = -1, s_lastPhase = -1;
  if (g_state.held != s_lastHeld) { l_info("INPUT f%d: pad word %03x%s", frame, g_state.held, g_state.pointerMode ? " (pointer mode)" : ""); s_lastHeld = g_state.held; }
  int ph = Input::touchCount ? (int)Input::touches[0].phase : -1;
  if (ph != s_lastPhase && ph != (int)TouchPhase::Stationary) {
    if (ph >= 0) l_info("INPUT f%d: touch phase %d at (%.0f, %.0f)", frame, ph, Input::touches[0].position.x, Input::touches[0].position.y);
    s_lastPhase = ph;
  }
  if (g_state.escapeDown) l_info("INPUT f%d: Start (Escape)", frame);
}

const State& GetState() { return g_state; }

}  // namespace vita_input

/*
 * vita_input.hpp -- physical input -> the game's input model (WP3, docs 06).
 *
 *  - SceCtrl (buttons, sticks) -> the native pad word (bits of GameUpdate(press, toggle), 01 §2)
 *  - front touch (1920x1088 raw) -> logical-surface coordinates (bottom-left origin) -> Input.touches[0] with Unity phases
 *  - Start -> Input.GetKeyDown(Escape) (quit dialog flow); Select -> pointer-cursor toggle (left stick moves a cursor, Cross = finger down)
 *  - scripted input (headless emulator runs): ux0:data/dqiv/input_script<slot>.txt, see ScriptHelp() / README "Input script"
 *
 * Poll() runs once per rendered frame (30 Hz) before the Update() pass; TouchPad.Update() (game/TouchPad.cpp) turns the
 * result into TouchPad.m_press / m_toggle exactly where the C# factUpdate() did.
 */
#pragma once
#include <cstdint>

namespace vita_input {

// native pad bits (GameUpdate)
enum PadBits : int {
  PAD_U = 0x1, PAD_D = 0x2, PAD_L = 0x4, PAD_R = 0x8,
  PAD_ROT_L = 0x10, PAD_ROT_R = 0x20, PAD_ROT_RESET = 0x30,
  PAD_ANY = 0x100,            // tap anywhere (native converts a lone 0x100 toggle to A)
  PAD_A = 0x200, PAD_B = 0x400, PAD_X = 0x800, PAD_Y = 0x1000,
};

struct State {
  int held = 0;               // pad word of this frame (buttons + sticks); 0 while the pointer cursor is active
  int tapToggle = 0;          // legacy: PAD_ANY on the frame a finger is released without having moved (TouchPad derives it itself now)
  bool escapeDown = false;    // Start pressed this frame
  bool pointerToggle = false; // Select pressed this frame
  bool pointerMode = false;   // pointer cursor active
  float pointerX = 0, pointerY = 0;   // cursor, logical surface pixels (bottom-left origin)
  int frame = 0;              // number of Poll() calls so far (script clock)
};

void Init();
void Poll();                  // fills Input.touches / touchCount / escapeDown and State()
const State& GetState();
void SetPreGame(bool pre);    // true until the first GameUpdate: any face button = a tap at the screen centre (Boot logos / title)
bool ScriptActive();          // an input script was loaded and has events left

// 06 §2: raw front-panel coordinates (0..1919, 0..1087) -> logical surface pixels, origin bottom-left. False if outside the surface.
bool TouchToLogical(int rawX, int rawY, float* lx, float* ly);

}  // namespace vita_input

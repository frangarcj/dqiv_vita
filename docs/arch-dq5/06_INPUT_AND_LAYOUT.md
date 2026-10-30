# 06 - Input design and screen layout (Vita)

## 1. Screen layout (decision + options)

Facts: the game is portrait only; UI world width is **450 units** (MenuCamera ortho size = 225/aspect), height = 450/aspect; Field/DSSA cameras fix the world width at 256/257 units; `SceneCamera` keeps horizontal FOV fixed.
`Camera.aspect > 0.74` = "tablet" branch (sent to native in the MenuMessage handshake `SendInts(texId, fmt, 512, 512, tabletFlag)`; changes the message-window variants). Layout code (`eIndent`, `canvas_data`) anchors to screen edges, so any aspect >= 0.5625 works; > 0.75 untested.

| mode (config `layout=`) | logical surface | placement on 960x544 | px/UI unit | notes |
|---|---|---|---|---|
| **`tablet` (default)** | **408 x 544** (3:4, aspect 0.75) | centred, 276 px pillars (use for a border image/ button-hint panel) | 0.907 | +33 % bigger than phone layout; uses the game's own tablet layout; UI height 600 units |
| `phone` | 306 x 544 (9:16) | centred, 327 px pillars | 0.68 | smallest, exactly the Android phone layout (safest for pixel-compat of native layouts) |
| `tate` (stretch goal) | 544 x 960 -> 540x960 (9:16) rotated 90 deg onto the display | hold the console vertically (screen's long axis vertical), 1:1 pixels | 1.2 | needs rotated d-pad/touch/buttons mapping (see 3); present pass rotates the logical-surface FBO; best readability |
| `fill` (debug) | 960 x 544 | whole screen | - | UI vertical crop: unsupported |

Implementation: one logical-surface FBO (03 §4). `Screen.width/height` = logical size. Touch/pointer coordinates are converted to logical pixels with bottom-left origin.
Rejected: re-layout of the UI for landscape (native positions menus assuming portrait aspect, ~70 screens of C# layouts, `canvas_data`); rotating the UI inside landscape (unreadable).

## 2. Touch (front panel)

Vita front touch: 1920 x 1088 raw (`sceTouchPeek(SCE_TOUCH_PORT_FRONT)`, reports have x in [0,1919], y in [0,1087]) <-> display 960 x 544 (divide by 2; y raw 1087 ~ 543).
```
sx = raw.x / 2.0, sy = raw.y / 2.0                     // display pixels, origin top-left
lx = (sx - X0) * (viewW_logical / viewW_display)       // X0 = (960 - W)/2 for layouts above (scale 1.0 for tablet/phone)
ly = (H - 1) - (sy - Y0)                               // Unity origin bottom-left
inside = 0 <= lx < W && 0 <= ly < H                    // touches in the pillars are ignored (Began outside never starts a touch)
```
Unity `Touch` synthesis (`Input.touches[0]`, `touchCount` only counts our synthesized primary finger; game reads only index 0):
* primary finger = first report id seen; keep until it lifts; other fingers ignored. Phase: `Began` on first frame, `Moved` if the position changed, `Stationary` otherwise, `Ended` on the frame where it disappears (position = last position, `touchCount` still 1 for that frame, exactly like Unity: `RawPad.touch` setter uses `phase==Ended` to clear press). `deltaPosition` = pos - previous frame pos, `deltaTime` = frame dt, `tapCount` = 1.
* Poll once per rendered frame (30 Hz) and **latch**: if a finger goes down and up between two polls (<33 ms) emit Began on frame N and Ended on N+1 with the same position - otherwise short taps are lost (the C# needs `bPress` for >= 1 frame to produce `m_toggle`/click states).
* `TouchPad.m_pad` (`RawPads`) and the `Ray`/`ScreenPointToRay(Camera)` logic stay as in the C# - used by every MenuPort button hit test (`MenuPortSystem.s_ray` from `TouchPad.GetPressRay`), SlimeTouch and InputName.
* Tap anywhere => `PAD_ANY (0x100)` toggle (state ANY, finger released without moving) => **native converts to A (0x200)**: tapping the screen confirms messages, advances pages etc.

## 3. Physical buttons -> what `TouchPad` hands to `GameUpdate(press, toggle)`

The original code never reads gamepads (only `Input.touches` + Escape). The native pad word is `TouchPad.m_press/m_toggle` (01 §2). The port replaces the virtual pad part of `TouchPad.factUpdate` with:

| Vita | bits (press / rising-edge toggle) | notes |
|---|---|---|
| D-pad, left stick (deadzone 56/128, 8-way by 22.5 deg sectors like `c_padDirTable`) | `U=1 D=2 L=4 R=8` (diagonals = two bits) | movement on field/town; also native menu cursor when the menu item has `m_bPad` |
| L / R | `0x10` / `0x20` (camera rotate -/+, hold) | in `tate` swap per rotation |
| L + R together, or R3 | `0x30` toggle = **reset camera to north** (`ROT_OR_FLICK` tap emits `m_toggle = m_press = 0x30`) | |
| right stick X | `0x10` / `0x20` while |x| > deadzone | analog camera rotation |
| Cross | `A 0x200` | confirm. Configurable swap (`confirm=circle`) as in JP releases |
| Circle | `B 0x400` | cancel / **talk** (Talk button = key 0x400) |
| Triangle | `X 0x800` | command menu (the "Command" button = key 0x800) |
| Square | `Y 0x1000` | area map (the "Map" button = key 0x1000; only when `Global.Flag & 0x20` (`IsTouchMap`)) |
| Start | `Escape` (`Input.GetKeyDown`) => `ApplicationQuit.Create()` quit dialog (`m_bToggleESC`) | |
| Select | toggle "touch-pointer cursor" (below) | |
Gating (copy `factUpdate` exactly): `if ((Global.Flag & 0x30003) != 0) { press = toggle = 0; }` (fade / screen effect / pause / cleaning ignore input). `toggle = (prevPress ^ press) & press | externalToggle`. Pad bits and touch bits are OR-ed (touch tap => 0x100 while buttons => 0x200).
Do **not** draw the virtual pad (ring, rotate bar, 4 buttons, `MPad` meshes) when a pad is used: `TouchPad` keeps only its C# state machine for `SetInputMode/SetPosPadSize/SetPadBlink` (accept and ignore) plus the on-screen **menu-related buttons that are real MenuParts** are untouched. Optional `touchpad=on` setting re-enables the full C# virtual pad (useful to validate against Android behaviour; needs `MPad`/colliders from `scene/level0`).

### 3.1 Menus and touch-only screens
* MenuPortItem screens (shop, battle commands, status...) are *touch* UIs but also pad-aware: native tells the host `m_bPad` (cmd 1 param) and the active highlighted index (cmd 4); the native menu code reads the same pad word (D-pad moves, A selects, B cancels). **Expectation (to verify in first run): physical buttons drive menus with no extra host work.** Buttons the host draws then show the highlight via `IUC` messages.
* Touch-only: `InputName` (name entry soft keyboard = MenuParts buttons), `LicenseNobu`, `SlimeTouch*`, `EndrollPort` skip, `Boot.Logo` skip, `TapToStart`, `Capture` none, the world-map zoom? For these provide a **pointer cursor**: Select toggles; left stick moves a 16x16 sprite cursor (drawn on the Menu layer, queue Overlay), Cross = touch down/up at the cursor (synthesised `Input.touches[0]` Began/Ended), so every screen is reachable without touching. Also: while on the Boot logos/`TapToStart`, **any button = tap** (synthesised touch at screen centre, 2 frames).
* `FLAG_TOUCH_DIR (8)` (slime race/scope/sugoroku dice): native expects screen taps/direction by touch; provide pad -> synthetic touches only if needed (post-first-field work).

## 4. Rotation mode notes (`tate`)
Hold the Vita rotated 90 deg clockwise (left side down? choose one: "D-pad at the top"): logical (lx,ly) from display (sx,sy): `lx = sy_rot`, ... implement as a 2x2 matrix in the touch converter and in the present quad. Buttons: logical Up = physical Right (for clockwise rotation) etc.: the d-pad/stick direction vector is rotated, face buttons keep their meaning. The UI is much larger (1.2 px/unit); pillars disappear. Keep behind a setting; do not block other work on it.

## 5. Other host input
* `Input.GetKeyDown(KeyCode.Escape)`: Start (above). The Android "back" button flow in `Boot.Quiting` (state machine 0..3 with `ApplicationQuit` dialog) is kept; dialog = sceMsgDialog YES/NO (strings from `boot/quitmsg` per language index `lang*4..+3`).
* Sensors, keyboard, gyro, rear touch: unused. PS button / suspend => `Sound.SystemPause(true/false)` + freeze the loop; live-area resume => call nothing else (arrays are static).
* Saves: libdq5core writes `data1..9.dat` into the save dir by itself; the port only provides the directory (`ux0:data/dqv/files/`).

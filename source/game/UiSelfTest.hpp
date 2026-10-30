// game/UiSelfTest.hpp -- OWNER: WP4a (dev aid).  Scripted UI testing for headless Vita3K runs, driven by ux0:data/dqiv/dqivn<slot>.cfg (RUN_CFG_PATH) lines:
//   ui_press=FRAME:PADBITS[,FRAME:PADBITS...]   ORs PADBITS into TouchPad::m_externalToggle at that Time.frameCount (A = 16, B = 32, X = 64, Y = 128, U/D/L/R = 1/2/4/8),
//                                               i.e. a scripted "button press" that reaches GameUpdate like a real one (works while Global.Flag has no fade/pause bits).
//   ui_tap=FRAME:PORT*1000+BUTTONID[,...]       simulates a touch release on that MenuPortItem button (sends [1, buttonId, gamepad] to native exactly like MenuPortItem.Update).
//   ui_name=FRAME:PORT                          answers the name-entry screen (InputName, WP4b) with "Tester": Send(2,len) + SendBytes(utf8).
//   ui_cycle=FRAME:SCREEN_ID                    builds and closes that screen 10 times (UI-MEM lines in the log must stay flat: leak check)
//   ui_tour=FRAME:ID,FRAME:ID,...               screenshots every listed screen in turn (build, DebugFill, shot dqivn<slot>_ui<ID>.jpg, close); FRAME = start frame of the tour
//   ui_soak=FRAME:0                             endless loop over every screen (build, fill, 3 frames, close) from that frame on: memory soak (SoakStats / UI-MEM lines)
//   ui_magic=FRAME:0                            builds the battle spell list (screen 35) filled with 4 spells + "MP Cost:<tab>  N" (tab-stop layout check)
//   ui_open=FRAME:SCREEN_ID[,...]               builds MenuPortItemTable screen SCREEN_ID on a spare port (28..31) at that frame, shown permanently (m_bIgnoreDrawRequset).
// Nothing happens unless the cfg has these keys.  tools/run_vita3k.sh writes only shot_frames, so append the lines to the cfg by hand / with UI_CFG="..." (see script).
#pragma once
// DQIV_DEV_TOOLS (CMake option, ON for Debug / OFF for Release): without it this dev aid is compiled out and the calls are no-ops.
#ifdef DQIV_DEV_TOOLS
namespace UiSelfTest { void Tick(int frame); }
#else
namespace UiSelfTest { inline void Tick(int) {} }
#endif

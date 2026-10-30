// game/WarpTest.hpp -- OWNER: WP5 (dev aid).  "Warp" harness: ux0:data/dqiv/warp<slot>.on is a script executed line by line on the game thread once the native boot is done,
// through libdq4core's own debug / game-part entry points (decoded from DebugSetValue / DebugSetMapName / Global::start*), so the game can be dropped anywhere:
//   wait N                      frames to wait before the next line
//   chapter N                   DebugSetMapName("C-N")      -> StoryStatus::setChapter(N)  (N = 1..5, 0 also clears flags 0x71/0x32)
//   flag ID [0|1]               DebugSetValue(100, ID, v)   -> GameFlag::set / remove
//   gold N | bank K | coin N | medal N     DebugSetValue(3|4|5|7, 0, N)   (bank in thousands)
//   item ID N                   DebugSetValue(2, ID, N)     (ID 0: fill the bag, -1: empty it)
//   party ID...                 DebugSetValue(12, 0/ID/-1)  (player ids)         monster ID   DebugSetValue(0x20, 0, ID) (recruit from the old man)
//   raw K A B                   DebugSetValue(K, A, B)      any other case       map NAME  DebugSetMapName(NAME) (non-prefixed: reserve map)
//   town NAME | townid ID | firsttown | debugtown | field | casino | slimerace | slimetouch | surechigai | book | opening | staffroll | fin | title | load
//                               Global::startTown(NAME) etc. (g_Global is the object: `this` = &g_Global)
//   fieldgetpos | fieldpos TX TY   log / set the world-map player position (tiles, 16.16 fixed)
//   fieldscale S                host zoom of the world-map camera (Field::SetScale)
//   dumpmaps                    log every map id and its short name ("MAP <id> <name>")
//   battle MAP GROUPS...        same as BattleTest ("battle btlca1 1x2")
#pragma once
namespace WarpTest { bool Requested(); void Create(); void CreateFromCfg(); }      // CreateFromCfg: cfg `warp=<map>` [`warp_chapter=N`, `warp_delay=F`] on top of the normal boot (DevCheats)

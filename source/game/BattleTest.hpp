// game/BattleTest.hpp -- OWNER: harness (dev aid).  Forces a battle through libdq4core's own debug hooks (what the original debug menu did), see DevHooks::StartBattle (WarpTest.hpp):
//   DebugSetValue(1, event, female)            -> a loaded game state (the title has none): party, level, items of a story event; default event 1 (chapter 1 start)
//   DebugSetValue(8, g, (monster << 16) | count)  g = 0..3 monster groups
//   DebugSetValue(8, 4, 0)                     -> Encount type 0, StageStatus battle map "btl_pl_d", Global::startBattle(); the stage name is then overwritten with `map`
// File ux0:data/dqiv/battletest<slot>.on (one key per line): `map <stage>` (btl_pl_d, btl_mt_d, btlda1, ... the last letter d/e/n = time of day), `mon <monsterId> <count>` (up to 4), `event <id> [female]`
// (default `event 1`; `chapter N` is accepted too), `delay <frames>` (after the native boot, default 60), `repeat <frames>` (start another battle every N frames, 0 = once).
#pragma once
namespace BattleTest { bool Requested(); void Create(); }

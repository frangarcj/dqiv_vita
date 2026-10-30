// game/BattleTest.hpp -- OWNER: WP5 (dev aid).  Forces a battle through libdq4core's own debug hooks (what the original debug menu did):
//   DebugSetMapName("B-<battle map>")       -> the battle background FLD (short native name, e.g. "btlca1" or "btl_pl_d"; NOT the path)
//   DebugSetValue(12, id, 0) ... (12,-1,0)  -> party (player ids), optional
//   DebugSetValue(8, g, (monster << 16) | count)  g = 0..3 monster groups
//   DebugSetValue(8, 4, 0)                  -> setTileId / brew / Global::startBattle()
// File ux0:data/dqiv/battletest<slot>.on (one key per line): `map <short name>`, `mon <monsterId> <count>` (up to 4), `party <playerId>` (repeatable), `delay <frames>`, `forceparty` (honour `party` lines: they fault libdq4core before a game is loaded, so they are ignored with a warning otherwise)
// (frames after the native boot, default 60), `repeat <frames>` (start another battle every N frames, 0 = once).
#pragma once
namespace BattleTest { bool Requested(); void Create(); }

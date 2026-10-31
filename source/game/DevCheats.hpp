// game/DevCheats.hpp -- OWNER: harness (developer / tester aids, always compiled, inert unless a key is set in ux0:data/dqiv/config.txt or dqivn<slot>.cfg).
//   nodamage=1        party takes no damage (status::PartyStatus::setNoDamageEnable, a plain global flag; re-armed every 30 frames)
//   noencount=1       no random encounters (DebugSetValue(10, 0, 2) clears the Encount "enabled" byte; re-armed every 60 frames because Encount::initialize resets it)
//   gold=N            once, ~2.5 s after the first real map (not the title backdrop) has loaded: DebugSetValue(3, 0, N)
//   party_level=N     same moment: DebugSetValue(11, N, 0) = PlayerStatus::levelup(N) for every player
//   party_hp=N        same moment: max HP of every party-capable player (DebugSetValue 0x11, player, N);   party_mp=N  max MP (0x12); the party is then healed (DebugSetValue(9, 0, 0))
//   warp=<map name>   boot, then the event of `warp_chapter` (default 1) is loaded (DebugSetValue(1, ev, 0): party/level/items/flags) and Global::startTown(<map name>) like WarpTest
//                     (`warp_delay=F` frames after boot, default 150), e.g. warp=maout
//   heropos=1         log "CHARA0 pos" (the position of chara slot 0 = the hero, as sent by the native) whenever it changes
#pragma once
namespace DevCheats {
void Create();           // GameLoop: after the level builder; creates the driver only when a key is present
int HeroPosLog();        // `heropos=1` hero only, `heropos=2` every character slot
}

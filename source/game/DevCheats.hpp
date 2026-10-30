// game/DevCheats.hpp -- OWNER: WP3 (developer / tester aids, always compiled, inert unless a key is set in ux0:data/dqiv/config.txt or dqivn<slot>.cfg).
//   nodamage=1        party takes no damage (status::PartyStatusUtility::setNoDamageEnable, re-armed every 30 frames)
//   noencount=1       no random encounters (libdq4core btl::Encount::brew patched to "return 0"; execField / execDungeon start a battle only when it returns non-zero)
//   gold=N            once, ~2.5 s after the first real map (not the title backdrop) has loaded: DebugSetValue(3)
//   party_hp=N        same moment: max HP of the 8 party slots (DebugSetValue 0x11);   party_mp=N  max MP (0x12)
//   warp=<map name>   boot, then Global::startTown(<map name>) like WarpTest (`warp_chapter=N` first, default 1; `warp_delay=F` frames after boot, default 150), e.g. warp=caout1
//   heropos=1         log "CHARA0 pos" (the position of chara slot 0 = the hero, as sent by the native) whenever it changes
// There is no `party_level`: libdq4core has no level setter in its debug interface (HaveStatusInfo::levelup is an instance method on state we cannot reach safely); use party_hp / party_mp.
#pragma once
namespace DevCheats {
void Create();           // GameLoop: after the level builder; creates the driver only when a key is present
bool HeroPosLog();       // `heropos=1`
}

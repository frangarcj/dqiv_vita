// game/WarpTest.hpp -- OWNER: harness (dev aid).  "Warp" harness: ux0:data/dqiv/warp<slot>.on is a script executed line by line on the game thread once the native boot is done,
// through libdq4core's own debug / game-part entry points (decoded from DebugSetValue / DebugSetMapName / Global::start* / status::Status::setEventParty), so the game can be dropped anywhere.
// DQ4 has a built-in "event table" (param::Event, 347 rows = party, level, gold, items, chapter, flags and map of a story checkpoint); DebugSetValue(1, id, female) loads one into the
// game state at once.  That is the DQ4 way to "start a chapter": the title state has no game (empty party), so every warp script begins with `chapter N` or `event ID`.
//   delay N                     (first lines only) frames after the native boot before the script starts (default 450)
//   wait N                      frames to wait before the next line
//   chapter N                   event of chapter N's start (1:ev1 caf1, 2:ev24 cbf2, 3:ev59 mfm1f1, 4:ev93 mhg1n1, 5:ev109 mlm1f1, 6:ev310 mqout) = `event` below; `chapter 0` = prologue (ev0, mlb1a)
//   event ID [female]           DebugSetValue(1, ID, female): party/level/gold/items/flags/chapter of event ID, then Global::startTown(<its map>)
//   eventinit ID [female]       same without leaving the current map (the map is applied by a following `town`/`field`/...)
//   dumpevents                  log every event row ("EVENT id ch map gold lv party0 party1 party2 name")
//   gold N | bank K | coin N | medal N | pmedal N     DebugSetValue(3|4|5|7|6, 0, N)   (bank in thousands; coin = casino coins, medal = mini medals, pmedal = player medal coins)
//   item ID N                   DebugSetValue(2, ID, N)     (ID 0: fill the bag, -1: empty it)
//   level N [player]            DebugSetValue(11, N, player)  (player 0 = everyone, -1 = carriage-out party)
//   hpmax P N | mpmax P N       DebugSetValue(0x11|0x12, P, N)   (P = player index 1..25)
//   heal [P]                    DebugSetValue(9, 0, P) = setBestCondition (full HP/MP, no status); `status K P` = DebugSetValue(9, K, P) (K 1 death, 2 sleep, 3 poison, 4 near-death, 5 hp=1)
//   timezone N                  DebugSetValue(12, N, 0)  (StageStatus::setTimeZone)
//   nodamage [0|1] | noencount [0|1]     party takes no damage (PartyStatus::setNoDamageEnable) / no random encounters (DebugSetValue(10, 0, 2) -> Encount enable byte = 0)
//   raw K A B                   DebugSetValue(K, A, B)      any other case       map NAME  DebugSetMapName(NAME) (ignored by the native while the map is the title `ev01`)
//   town NAME | firsttown | debugtown | field | casino [poker|slot|N] | book | title
//                               Global::startTown(NAME) etc. (g_Global is the object: `this` = &g_Global); `casino` sets Global::setMinigame first (0 poker, 1 slot)
//   staffroll                   StoryStatus chapter 9 + the prologue town `mlb1a` (what the native `LunchStaffroll` does)
//   fieldgetpos | fieldpos TX TY   log / set the world-map player position (tiles, 16.16 fixed)
//   fieldscale S                host zoom of the world-map camera (Field::SetScale)
//   battle MAP GROUPS...        force a battle: DebugSetValue(8, g, monster<<16 | count) for groups 0..3, DebugSetValue(8,4,0) (startBattle with "btl_pl_d"), then the stage name MAP
//                               ("btl_pl_d", "btl_mt_d", "btlda1" ... = file names under map/btl/*/stage; the last letter d/e/n is the time of day) is written into StageStatus.
//                               ("battle btl_pl_d 1x3 2x1": monster id x count).  Needs a loaded game state: run `chapter N` first.
#pragma once
#include <cstdint>
namespace WarpTest { bool Requested(); void Create(); void CreateFromCfg(); }      // CreateFromCfg: cfg `warp=<map>` [`warp_chapter=N`, `warp_delay=F`] on top of the normal boot (DevCheats)

// Shared native hooks (also used by BattleTest and DevCheats); each logs and returns false when its symbol is missing.
namespace DevHooks {
const uint8_t* EventRec(int id);                    // param::Event::getFileData: first row of event `id` (map name at +0x93, chapter at +0x9b & 7, UTF-8 title at +0x13), null if absent
int  ChapterEvent(int chapter);                     // chapter start -> event id (-1 if unknown)
bool EventInit(int id, bool female);                // DebugSetValue(1, id, female)
bool StartTown(const char* map);                    // Global::startTown
void SetNoDamage(bool on);                          // status::PartyStatus::setNoDamageEnable
void SetNoEncount(bool on);                         // DebugSetValue(10, 0, 2|0); re-arm after every EventInit (Encount::initialize resets it)
bool NoEncountActive();                             // DebugGetValue(1): the Encount enable byte is 0
bool StartBattle(const char* stageMap, const int* mon, const int* count, int groups);   // see `battle`
void* Obj(uint32_t vaddr);                          // an internal global of libdq4core (so vaddr) -> runtime address (via g_Global)
}

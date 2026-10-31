# Dragon Quest IV (PS Vita)

<p align="center"><img src="extras/livearea/pic0.png" width="480"></p>

A native PS Vita port of the Android version of *Dragon Quest IV: Chapters of the Chosen* (Square Enix, v1.0.5). The Android game logic (`libdq4core.so`) and audio engine
(`libsdlib_android.so`) are loaded in memory and run unchanged; everything that Unity did (scenes, sprites, UI, effects) is a C++ re-implementation on top of vitaGL.
**No Unity runtime is involved and no game data is included**: you convert your own copy of the game once, on a PC.

* Four languages: English (default), Korean, Traditional Chinese and Simplified Chinese. **There is no Japanese**: the v1.0.5 OBB holds game text only for `en`, `ko`, `sl` (zh-CN) and `tl` (zh-TW), so `lang=ja` is refused with a log warning.
* Landscape display (the original is portrait-only; `layout=portrait` restores it). Physical controls plus touch, as on Android. Saves are the original format (`data1..5.dat`, `data9.dat`).
* Cloud Save / Google Drive and the online present-code features are not available (the Vita side answers as if the device were offline).

**Status: early.** What was actually checked, in Vita3K only (the logs and screenshots of the development builds; **no real PS Vita has ever run this port**):

* verified: both libraries load and link with 0 unresolved imports, `JNI_OnLoad` of both succeeds, the converted pack opens (28 446 objects), the world-map tile patch is applied (`libdq4core` `FieldData::drawBlock`, 59 columns);
  the title scene (logos, backdrop, "Create a new adventure log" menu with Licence / Cloud Save buttons) and the new-game name-entry screen render in landscape with sound running (no underruns in the log) and are reachable with the buttons only (`tools/padonly_dq4.txt`);
  libdq4core writes its save files (`data1..5.dat`, `data9.dat`) into `saves1/`; the DQ4 menu screens with ids 1..80 (except 55, the name entry, and 74, an empty port; the name entry was seen separately) were built with sample data and screenshotted (`docs/TODO_DQ4.md`, DEVELOPERS.md "UI visual checklist").
* **not verified in this port**: actually playing: walking in towns/dungeons, the world map, battles, shops and every other in-game screen driven by the real game, cutscenes, the staff roll, the Korean and Chinese languages, `layout=split|portrait`, the battle/dialogue layout options below, performance on a real console, memory use on a real console. The code for most of these (field, characters, world map, battle UI, effects) is ported but, for DQ4, only seen at title/menu level.
* the developer cheats and warp harness (`DevCheats`, `WarpTest`, `BattleTest`) are **disabled for DQ4** in the code this README was written against (their DQ5 hooks do not exist in `libdq4core.so`; see `docs/TODO_DQ4.md` for the current state). The Coverage section and the Debug options below were carried over from the DQ5 port and are rewritten by the harness work, not here: until then they do not describe DQ4.

See [DEVELOPERS.md](DEVELOPERS.md) and [docs/TODO_DQ4.md](docs/TODO_DQ4.md) for the details and the open items.

## Setup (end users)

### 1. Vita requirements
* A hacked PS Vita with [taiHEN](https://github.com/yifanlu/taiHEN). Install **kubridge**: copy `kubridge.skprx` ([releases](https://github.com/bythos14/kubridge/releases/)) to `ux0:tai/` and add it under `*KERNEL` in `ux0:tai/config.txt`:
  ```
  *KERNEL
  ux0:tai/kubridge.skprx
  ```
  (reboot afterwards).
* `libshacccg.suprx` is **not needed to start the game** as long as `ux0:data/dqiv/cg/*.bin` exists (prepared by step 3): the screen-effect shaders `dqiv_gray` and `dqiv_sepia_fld` are shipped precompiled. The third program (`dqiv_sleep`, the lying-down pose of characters) has no shipped blob: without `libshacccg.suprx` it falls back to a fixed-function approximation (unverified on hardware), with it the game compiles it once and caches it as `ux0:data/dqiv/cg/dqiv_sleep.bin`
  ([how to get libshacccg.suprx](https://samilops2.gitbook.io/vita-troubleshooting-guide/shader-compiler/extract-libshacccg.suprx)).

### 2. Get the game files (legally, from your own purchase)
You need exactly these two files (the script checks their md5):

| file | version | md5 |
|---|---|---|
| `DRAGON QUEST IV` APK | v1.0.5 (`com.square_enix.android_googleplay.dq4`) | `2f0bcb22598a16ce50a038488696bc98` |
| `main.106.com.square_enix.android_googleplay.dq4.obb` | main.106 | `3d27a44d87f30f119fa4a7d73805c4b7` |

These md5s are those of the files the port was developed with. **A different build of the APK, even if it is also called v1.0.5, is refused** by the script (and by the web page, which has no override); `tools/make_data.py ... --skip-md5` lets you try it at your own risk (the Unity assets must then still convert to the same layout; the script warns if the pack differs from the reference).

The OBB is in `/sdcard/Android/obb/com.square_enix.android_googleplay.dq4/` on your phone; the APK can be extracted with any APK extractor app or `adb pull $(adb shell pm path com.square_enix.android_googleplay.dq4 | cut -d: -f2) dq4.apk`.

### 3. Prepare the data on a PC (Windows, macOS or Linux)

**Recommended: the web page** (source in `web/`, no install; the planned address is <https://frangarcj.github.io/dqiv_vita/>, which **only becomes valid once this repository is published** and the Pages workflow has run). Open it in a modern browser (Chrome/Edge write straight into a folder; Firefox/Safari download one ZIP), select the APK and the OBB, pick language and screen layout, press Convert. Everything runs locally in the browser tab: your files are never uploaded and the site ships no game data. Its output is streamed, so it needs well under 1 GB of RAM on top of the browser; it checks that the result is byte-identical to the reference conversion (SHA-256 of the five pack files).
You can serve the page yourself: from a clone of this repository run `python3 -m http.server -d web 8000` and open <http://localhost:8000/> (or just open `web/index.html` from disk; `web/dist/app.js` is committed, no build needed). Details, build and test instructions in [web/README.md](web/README.md). The workflow `.github/workflows/pages.yml` publishes `web/index.html` + `web/dist/app.js` to GitHub Pages from the `main` branch.

**Alternative: the Python script** (same output, bit for bit). Needs Python 3.10+ (3.12 recommended), about 2 GB of free RAM and 1.5 GB of free disk, and internet access on first run (it creates `tools/.venv` and installs the pinned packages of `tools/requirements.txt`).

```sh
python3 tools/make_data.py DQ4.apk main.106.com.square_enix.android_googleplay.dq4.obb dqiv_data        # add --lang ko|zh-tw|zh-cn if you want another language
```

Either way you get (a few seconds to a few minutes, the asset conversion is the long part; about 380 MB in all):

```
lib/        libdq4core.so, libsdlib_android.so   (taken unchanged from the APK)
assets/     ct.inf                               (the APK's only root asset, 10 bytes; DQ4 has no filelist.bin: the game reads the OBB zip itself)
main.obb    your OBB, unchanged
pack/       index.bin + 4 .pak files             (converted Unity assets, ~190 MB)
cg/         2 precompiled shader blobs
config.txt  user settings
```

Copy **the contents** of `dqiv_data/` (or of the folder you chose in the web page; or the `dqiv` folder of `dqiv.zip`) to `ux0:data/dqiv/` on the Vita (FTP via VitaShell, or USB).

### 4. Install the app
Install `dqivn.vpk` (from the [Releases](../../releases) page once one exists, or build it yourself, see below) with VitaShell. The bubble is "Dragon Quest IV" (title id `DQIVN0001`). The first launch takes a little longer.

### config.txt
`ux0:data/dqiv/config.txt`, one `key=value` per line (`#` = comment). Every key below is read by the code of this repository; **only `lang`, `confirm`, `field_patch` and the default `layout=landscape` were seen working in DQ4 in Vita3K**, the other layout and battle keys come with the DQ5 port's code and have not been re-checked in DQ4 gameplay.

| key | values | meaning |
|---|---|---|
| `lang` | `en` (default), `ko`, `zh-tw`, `zh-cn` | game language (menus, native text, fonts). Changing it does not touch your saves. No Japanese in this OBB. Only `en` was seen in DQ4 runs. |
| `confirm` | `circle` | swap Cross/Circle (Japanese layout) |
| `layout` | `landscape` (default), `split`, `portrait` | screen layout: full 960x544 world with the UI panel on the right / world beside the panel / the original portrait pillar-box. `ui_side=left` puts the panel on the left. |
| `battle_layout` | `large` (default), `panorama`, `panel`, `band` | landscape battles: whole-surface 3D view with the zoom auto-fitted to the monsters / fixed `battle_zoom` / no zoom / the original 4:3 band |
| `battle_zoom` | `100`..`175` (default 150) | zoom of `battle_layout=panorama` |
| `battle_speed` | `1` (default)..`4` | run the battle logic and animations N times faster (battles only) |
| `dialogue` | `bottom` (default), `panel` | landscape: message window + Yes/No at the bottom-centre of the screen, or in the right panel like portrait |
| `dialogue_scale` | `100`..`160` (default 100) | scale of the bottom-centre dialogue window (percent) |
| `boot_centre` | `1` (default), `0` | landscape: title / start menus / save slots / name entry centred on the screen instead of in the right panel |
| `world_shift` | `0` (default), `1` | `1` = the world slides left by half a panel while a panel menu is open |
| `battle_shift` | `1` (default), `0` | landscape battles: fixed slide of the 3D view to the left of the panel for the whole battle |
| `battle_face_scale` | `100`..`300` (default 250) | landscape battles: scale of the party face icons only |
| `battle_status_y` | `0`..`120` (default 45) | landscape battles: the whole party status row is moved down by this many pixels |
| `field_patch` | `1` (default), `0` | landscape: patch libdq4core so the world map draws a 59-column tile block (needed for the full 960 px width); `0` = limit the camera instead. The patch log line `world map tile block widened to 59 columns` is seen at every start; the widened map itself was not looked at in play. |

The DQ5 keys that do not exist for this game are gone: the cheats (`nodamage`, `noencount`, `gold`, `party_hp`, `party_mp`, `warp`) are disabled in DQ4 (see the Debug options note below).

## Controls
Mapping of the Vita buttons to the original pad bits (`source/platform/vita_input.cpp`); the in-game effect of each button in DQ4 has not been checked beyond the menus and the name entry.

| Button | Field | Menus |
|---|---|---|
| D-pad / left stick | move | move the cursor |
| Cross | confirm / talk-check (A) | confirm |
| Circle | cancel / talk (B) | cancel / back |
| Triangle | command menu (X) | |
| Square | area map (Y) | |
| L / R (or right stick) | rotate the camera (towns and dungeons only, there is no field rotation on the world map); L+R or R3 = reset north | |
| Start | quit dialog | |
| touch screen | everything that was touch-only on Android (name entry, shops, battle commands ...) | |

## Saves
Written by the game itself to `ux0:data/dqiv/saves1/` (`data1..5.dat` = 15 360 bytes each, `data9.dat` = 512 bytes, as created in Vita3K), in the original Android format. Back that folder up before updating.
Logs: `ux0:data/dqiv/dqivn1.log` (errors only in the release build).

## Troubleshooting
| symptom | cause / fix |
|---|---|
| crash at launch / "kubridge" in the log | kubridge is not loaded: check `ux0:tai/config.txt` (`*KERNEL` section) and reboot |
| black screen, log says `cannot open the asset pack` | `ux0:data/dqiv/pack/` missing or incomplete: redo step 3 and copy **all** files |
| log: `DevInit returned false: libdq4core could not open its data (OBB)` or `asset_manager: cannot open ...main.obb` | `main.obb` missing/renamed, or it is not the main.106 OBB |
| the web page / `make_data.py` says "md5 mismatch" | other game build; this port supports the APK/OBB of the table above only (`--skip-md5` of `make_data.py` at your own risk) |
| `make_data.py` fails in `pip install` | use Python 3.12 (`py -3.12`/`python3.12`); behind a proxy set `HTTPS_PROXY`; or install `tools/requirements.txt` yourself and pass `--no-venv` |
| error about shaders / `libshacccg` | `ux0:data/dqiv/cg/` missing (re-run step 3) or install libshacccg.suprx (see above) |
| wrong language | edit `lang=` in `config.txt` (`en`, `ko`, `zh-tw`, `zh-cn`; `ja` is not available) |
| out of memory on load | close other apps; the app uses the extended memory mode (SFO ATTRIBUTE2) and a 96 MB newlib heap (`DQIV_HEAP_MB`), none of this was measured on a real Vita |

### Warp harness and Coverage

`ux0:data/dqiv/warp<slot>.on` (`game/WarpTest.cpp`, full syntax in `WarpTest.hpp`) is a script run on the game thread after the native boot. libdq4core has a built-in story-checkpoint table (`param::Event`, 347 rows: party, level, gold, items, flags, chapter and start map), and `DebugSetValue(1, id, female)` loads one into the game state at once: that is how every script starts (the title has no game, an empty party faults the native). `chapter N` (1..5, 6 = after the ending, 0 = prologue) = the first row of that chapter, `event ID [female]` = any row, `eventinit ID` loads the state without leaving the current map; `dumpevents` logs the table (`EVENT id ch map gold lv party name`). Then `town NAME` (`Global::startTown`, the 530 map names are the `map/<c|d|h|m|s|t|z|ev|f>/<xx>/<in|out>/<name>` files; short names in `tools/coverage/maps.txt`), `field`, `fieldpos TX TY`, `casino poker|slot` (needs a running town: from the bare title state `startCasino` faults), `book`, `staffroll`, `title`, and the debug cases `gold` / `bank` / `coin` / `medal` / `pmedal` / `item ID N` / `level N [P]` / `hpmax P N` / `mpmax P N` / `heal` / `status K P` / `timezone N` / `nodamage` / `noencount` / `raw K A B` (decoded from `DebugSetValue`: 1 event, 2 items, 3 gold, 4 bank x1000, 5 casino coins, 6 player medals, 7 medals, 8 battle setup, 9 status, 10 debug bit flags, 11 level, 12 time zone, 0x11/0x12 max HP/MP, 0x13-0x17 stats, 13/14 poker cards, 0x20/0x21/0x32/0x200 misc).
`battle MAP GROUPS...` forces a fight: `DebugSetValue(8, g, monster<<16 | count)` for groups 0..3, then `DebugSetValue(8, 4, 0)` (native: Encount type 0, battle map `btl_pl_d`, `Global::startBattle`) and the stage name is overwritten in `StageStatus` (`btl_pl_d`, `btl_mt_d`, `btlda1`, ... any `map/btl/*/stage/*.fld`; the last letter d/e/n is the time of day). Example `battle btl_pl_d 1x2`. The battle UI is then driven with an input script of Cross taps (`tools/coverage/battle_pad.txt`: PadFocus presses Fight, Attack, the first target and the messages) to victory and back to the map.
`tools/coverage_run.sh [name...]` runs `tools/coverage/<name>.warp` (headers `#shots`, `#dur`, `#input`) one muted emulator at a time (it waits for the machine-wide emulator lock) and keeps screenshots + issue extracts (STUB/UNPORTED/WRN/ERR after the warp, DABT) in `build/coverage/<name>/`.

| place | warp script | result | issues (owner) |
|---|---|---|---|
| chapter 1-5 starts (Burland castle `caf1`, `cbf2`, Lakanaba shop `mfm1f1`, `mhg1n1`, hamlet `mlm1f1`) | `ch1_start` `ch2_start` `ch3_start` `ch4_start` `ch5_start` `ch5_heroine` (`event 109 1`) | OK: chapter-1 title card fades in on the right panel, then the narration box on black (Alena / Lakanaba / the two sisters / the nameless hamlet); no STUB / UNPORTED / WRN / ERR / DABT | the heroine variant is indistinguishable here (hero name is not shown); not checked that the title card of chapters 2-5 appears (shots start after it) |
| town exterior, chapter 1 (Imuru `maout`) | `ch1_village` | OK: FLD map, buildings, NPCs; no log issues | first shot (f650) shows characters as thin vertical slivers, a second run (f700) shows them complete but each drawn twice side by side (Chara / FldObject owner) |
| cave / well (`dab1`, `dbb2b`; chapter 3 cave `dfb1a`) | `ch1_cave` `ch1_well` `ch3_cave` | OK (lit cave FLD, party sprites, torches); no log issues | - |
| towns, chapters 2, 4, 5 (`mdout` Frenor, `miout`, `msout` Gotside) | `ch2_town` `ch4_town` `ch5_town` | OK (full town FLD + NPCs + hero pair); no log issues | hero is drawn as a doubled sprite pair in every town (Chara owner) |
| world map (`field`, chapter 5 party, native start position tile 26.5/18.4 via `fieldgetpos`) | `field` | OK after merging main (`field_patch`): ocean, islands, mountains, a castle and the party of 3 on the coast; before the merge the map was white (`Field::SetIndexList: native FieldSet not bound`); no log issues | - |
| forced battle, plains (`btl_pl_d`, 2 x Walking stick, party of 4) | `battle_pl` + `battle_pad.txt` | OK to victory, re-run after merging main: both mantis monsters drawn, face icons in the status boxes, command menu with PadFocus on Fight, "Walking stick A is defeated", back on the Imuru map; no log issues, no DABT (before the merge: magenta checker monsters and `Menu/face/f00 f02 f14 f15` not found) | - |
| forced battle in a cave (`btlda1`, chapter 3 party of 3) | `battle_cave` | OK to victory and back to the cave (`dfb1a`) (measured before the merge of main) | `Menu/face/f06 f10 f11` not found and checker-textured monsters at that time (fixed on main, not re-run) |
| battle + Tactics page by pad (`btl_mt_d`, Restless armour x2 + Bullfinch) | `battle_tactics` + `battle_tactics_pad.txt` | OK: down/left reaches Tactics, page "Show No Mercy / Fight Wisely / Watch My Back / Don't Use Magic" (1/2), Circle backs out, Cross fights: commands for the 4 members, "Critical hit!", victory, back to the map; no DABT | `Sound: sound/SE0000.akb not found` (Sound owner); Fight -> Party with a 1-member selectable party ends on "not enough party members" with Cross taps unable to dismiss the message (observed once, 600 frames, `battle_tactics` first version) |
| casino slot / poker (from a running town `hcout`) | `casino_slot` `casino_poker` | OK: slot machine with the paytable and 5000 tokens, bet window; poker table, paytable, "How much do you want to put down?" | `startCasino` straight after `eventinit` from the title state (no running town) faults the native (DABT FAR=0 right after `main.obb` is opened): always `town X` + `wait` first. Monster arena not reachable (below) |
| monster book | `book` | OK: parchment + 12 `???` buttons + page bar (nothing seen yet) | sky backdrop and a floating island mesh are drawn over the left half of the parchment, not behind it (UI / Capture owner) |
| staff roll | `staffroll` | native starts it (StoryStatus chapter 9 in `mlb1a`) and requests the port | `[WRN] UNPORTED StaffrollPort(port 12): category 3` (UI owner): screen stays black |
| `Global::startTitle` from a map | `title_return` | OK: map -> sky scene with the credits text "Music Compo..." -> loops | the credits text is drawn upside down / mirrored (Capture or Staffroll owner) |
| monster arena (fight stadium), shops / inn / church UIs, bank, ship, balloon | - | not reachable by warp: the arena is entered from the casino town scripts (`Global::setFightingArenaMapName` + `MaterielMenu_FIGHT_STADIUM`), the rest needs NPC / event interaction; only `bCasino_FightStadium_debugWin` (`DebugSetValue(10, 1, 8)` side effect) is a debug hook | open |
| random encounters with `noencount` | cfg `warp=dab1 nodamage=1 noencount=1 heropos=1` + `cave_walk.txt` | OK: 2200 frames of walking in the Imuru cave (1277 position changes), no battle; the same run with `noencount` off loaded `btlda1` (one real random encounter) | - |
| cheats | cfg `gold=12345 party_level=20 party_hp=500 party_mp=100 nodamage=1 noencount=1 warp=maout` | OK: log read-back "player N hpmax 500 mpmax 100" for the 18 party-capable players, `noencount armed, encounters off` | gold is only logged (no native getter) |


### Debug options (testers)
Keys in `ux0:data/dqiv/config.txt` (all slots) or `dqivn<slot>.cfg`, inert unless present (`game/DevCheats.hpp`):
| key | effect |
|---|---|
| `nodamage=1` | the party takes no damage (`status::PartyStatus::setNoDamageEnable`, re-armed every 30 frames) |
| `noencount=1` | no random encounters (`DebugSetValue(10, 0, 2)`: the native `Encount` enabled byte is cleared, re-armed every 60 frames) |
| `gold=N` | set the gold once, ~2.5 s after the first real map loaded (the title backdrop does not count) |
| `party_level=N` | same moment: every player to level N (`DebugSetValue(11, N, 0)`) |
| `party_hp=N` / `party_mp=N` | same moment: max HP / MP of every player, then a full heal |
| `warp=<map>` | after boot, load the story event of `warp_chapter=N` (1..5, default 1: party, level, items, flags of that chapter's start) and start in map `<map>` (`warp_delay=F`, default 150 frames); map short names: `tools/coverage/maps.txt`, e.g. `maout` (Imuru village), `dab1` (Imuru cave), `caf1` (Burland castle), `mhg1n1` (chapter 4 start) |
| `heropos=1` | log `CHARA0 pos` (hero position as sent by the native) whenever it changes |

## Open problems / next steps
From [docs/TODO_DQ4.md](docs/TODO_DQ4.md) (the authoritative list):

* Play-through verification of everything beyond the title/menu: field, world map, battle, events, staff roll; real-hardware test (never done).
* Native debug hooks (`DevCheats`, `WarpTest`, `BattleTest`) have to be re-derived from `libdq4core.so` (kept disabled until then).
* `Sound` still uses the DQ5 state machine (the DQ4 `s_state/s_bgmNext` logic is not ported); the DQ4 clear-plane camera path (non-Adreno) is not ported (the DQ5 mesh plane is used); `MainLoopEx/FadeEx/MenuPortSystemEx` (non-NEON path) and the virtual-pad code of `TouchPad` are not ported (physical buttons replace the virtual pad).
* Dead DQ5-only classes still in the tree (`Spotlight`, `Sepia`, `Volcano`, ...) to be removed.

## Build (developers)
You need a standard (hardfp) [vitasdk](https://github.com/vitasdk) and these packages (`vdpm`):
```sh
vdpm opensles libsndfile lame mpg123 opus libvorbis libogg flac libmathneon vitaShaRK kubridge taihen SceShaccCgExt zlib
git clone --recurse-submodules <this repository>      # vitaGL (lib/vitaGL) is built from the submodule, the SDK one lacks GL_COMBINE
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target dqivn.vpk-vpk -j8         # -> build/dqivn.vpk
```
or use the container: see [.github/workflows/compilation.yml](.github/workflows/compilation.yml) (`vitasdk/vitasdk`). The Release configuration was built successfully (`dqivn.vpk`, about 1.8 MB) when this README was written.

| CMake option | default | meaning |
|---|---|---|
| `CMAKE_BUILD_TYPE` | - | `Release` = optimised, errors-only log, no developer tools. `Debug` = full log + developer tools |
| `DQIV_DEV_TOOLS` | ON for Debug, OFF for Release | test scenes (`*test<slot>.on`), engine self tests, scripted UI (`ui_*` keys) |
| `DQIVN_SLOT` | 1 | install slot 1-9 (title id `DQIVN000<slot>`, `saves<slot>`, `dqivn<slot>.log`): lets several builds coexist (development) |
| `DQIV_FAST_BOOT` | ON for Debug, OFF for Release | shortened boot logos (development) |
| `VITA_VERSION` (edit CMakeLists) | `01.00` | version shown in the LiveArea / SFO |

LiveArea images (`extras/livearea/*.png`) are generated from the game's own launcher icon by `extras/livearea/src/make_art.py <apk>` (needs Pillow); they remain Square Enix artwork, replace them if you redistribute a VPK.

## On-Vita data preparation instead of a PC step? (evaluated, not implemented)
The converter (`tools/convert`) is Python + UnityPy + texture decoders. A first-boot converter on the Vita would remove the PC step, but:

| | PC script (current) | converter inside the VPK |
|---|---|---|
| effort for the user | one command, a few minutes, needs Python (or just a browser tab with `web/`) | none |
| implementation | done (~1k lines Python, reuses UnityPy; the TypeScript port in `web/` is byte-identical) | needs a C++ port of: Unity 4.6 serialized-file + TypeTree reader, LZ4/deflate handling, ETC/PVRTC/DXT/ASTC texture decoders -> RGB565/RGBA4444, mesh/material/animation/shader conversion, prefab flattening (~5-8k lines, no UnityPy equivalent exists in C) |
| speed | about 20 s on 4 cores (Python, measured on a Mac) | 4-core 444 MHz ARM + memory-card IO: estimated hours for 17 773 serialized files / ~490 MB of serialized data, redone for every converter bug fix (an estimate, not measured) |
| memory | ~2 GB RAM | 512 MB total: needs a streaming converter, no global object graph |
| risk | low, testable in CI, reproducible | high; every format bug is a crash on device |

Recommendation: stay with `make_data.py` / the web page.

## Credits
TheFloW (so_util), gl33ntwine (soloader-boilerplate, FalsoJNI), Rinnegatamante (vitaGL, vitaShaRK), frangarcj (opensles), bythos14 (kubridge), the vitasdk team; UnityPy (K0lb3) for the converter.
Dragon Quest is a trademark of Square Enix. This project contains no Square Enix game data.

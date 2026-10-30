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

`ux0:data/dqiv/warp<slot>.on` (`game/WarpTest.cpp`, syntax in `WarpTest.hpp`) is a script run on the game thread after the native boot: `chapter N` / `flag` / `gold` / `item` / `party` / `raw K A B` go through libdq4core's `DebugSetMapName` / `DebugSetValue` cases (decoded from the decompile: kind 2 items, 3 gold, 4 bank, 5 casino coins, 7 medals, 12 party, 100 game flag, 99 wedding-bride choice, 8 battle setup, 0x20/0x22 monsters...), and `town NAME` / `townid` / `field` / `casino` / `slimerace` / `slimetouch` / `surechigai` / `book` / `opening` / `staffroll` / `fin` call `Global::start*` on `g_Global` (the object itself is `this`; `dumpmaps` logs the 621 map ids/short names). `fieldpos TX TY` (x then y tile, fixed in the second revision) / `fieldscale S` move/zoom the world-map player (`FieldPlayerManager::setPosition`, 16.16 tiles; the world is 256 x 256 tiles, 16 px each).
`tools/coverage_run.sh [name...]` runs `tools/coverage/<name>.warp` (headers `#shots`, `#dur`) one muted emulator at a time and keeps screenshots + issue extracts (STUB/UNPORTED/WRN/ERR after the warp, DABT) in `build/coverage/<name>/`. Big maps (`caout1`) run at ~10 fps on this Mac: give them a longer `#dur` and early `#shots`.

| place | warp script | result | issues (owner) |
|---|---|---|---|
| town exterior (adult era, `maout1`, `mbout`) | `town_out`, `town_mb` | OK: FLD map + hero billboard + camera, no log issues | NPCs not visible in the shots (their chara loads depend on event scripts) - expected |
| houses / inn / church interiors (`mawf1`, `mayf1`, `mak`, `ma2f1`) | `shop inn church castle` | OK | map-name guesses: `mawf1` and `ma2f1` are houses; no real shop UI reached (needs walking/talking) |
| castle throne room (`cdf3b` via `firsttown`) | `firsttown` (manual) | OK | - |
| dungeon (`dkb1`) | `dungeon` | OK (cave FLD, lighting, hero) | encounters not triggered (no walking) |
| child-era town (`caout1`, chapter 1) | `child_town` | OK but slow (~10 fps, 94 objects, big textures) | perf: WP2 (draw call count / CPU vertex tint), WP1 (texture residency) |
| world map outdoor (coasts, rivers, islands, bridge, shrine; landscape 59 columns and portrait 25) | `field`, `fieldland`, `fieldedge`, `fieldflight` (`fieldpos 124 117 / 52 76 / 210 110 / 104 185 / 8 125`) | OK: same coastline/river/bridge in both layouts, tiles continuous (orientation and stride correct), no void at the sides at zoom 1.0 and in the dragon flight (native zoom 1.25, chapter 5); host-only zoom >= 1.25 right after a position jump shows the unrendered edge (left void ~66 px at 1.25, 1.6 and 1.9 show all grid edges) - only reachable with the harness `fieldscale` | ship/carriage not reached (no vehicle state via warp) |
| battle | see Battle section | OK (menus, monsters, effects, damage numbers, victory -> title) | - |
| casino poker / slime race / slime touch | `casino` `slimerace` `slimetouch` `slime_ret` | OK: UIs render (token window, bet grid, whack-a-slime board) | the earlier native DABT after slime touch was a warp artefact: `startSlimeTouch` runs `setMapNameId(0)`, so with no return map the next `GameUpdate` dereferences null. `slime_ret.warp` (warp to `maout1`, wait, then `slimetouch`) plays the game, shows the score message and returns to the town without a fault; from a real new-game state it also runs without fault |
| sugoroku (T'n'T board, `swlv3a`) | `sugoroku` | OK: board tiles, icons, hero | dice/turn UI not driven |
| slime-race track (`ev01`) | `event_ev01` | OK | - |
| monster book | `book` | OK: page 1, `???` entries (no monsters seen) | - |
| wedding choice (`raw 99 1/2/3`) | `wedding` `wedding2` `wedding3` | OK: throne room with the three brides (`ev15`) | - |
| opening / ending (`opening`, `staffroll`, `fin`) | same names | OK: title backdrop, credits with petals, "The End" | `staffroll` returns to `caout1` after ~500 frames (credits list shorter than the real roll? WP6 to check against Endroll.cs timing) |
| surechigai | `surechigai` | title backdrop + DQ logo, no UI | needs a real session (WP6/WP3) |
| monster recruit (`raw 30 0 IDX` + `monster 0`) | `recruit_valid`, `recruit_real` | the earlier DABT was a bad debug argument: `addFriendMonster(i)` uses ctrl `i*3-0x20`, so friend indices < 11 (0 and 1 were tried) are out of range; indices 11..88 (20, 25) are accepted. From a real new-game state (castle) the recruit calls run without fault; from the title state the call itself works but `GameUpdate` faults a few frames later (no loaded game: same family as BattleTest `party` lines). Recruit in a real won battle not exercised | not a native bug |
| first cave (`dab1`, `daf1`) real walk, random encounters | cfg `warp=dab1 nodamage=1 gold=500 party_hp=300 heropos=1` + `tools/coverage/cave_walk.txt` (pad only) | OK: stairs dab1 -> daf1 and back, 2 real random encounters (`btlda1`, 3 slimes), the battle command UI is driven with the pad (PadFocus: Items window, focus marker), flee/result messages, no STUB / UNPORTED / ERR / DABT, steady 28 Hz (busy 3.4 ms/frame) | chests visible but not opened (blind walking, no path); hero name shows as `WWWWWWWW` after a warp (warp artefact: no name entered) |
| New Game intro, ship cabin / deck, castle (real play, pad only) | `tools/padonly_to_cabin.txt` | OK to the ship deck (`snb1a` -> `snf1a` -> `snout` -> `snf1b`), cutscenes advance with Cross taps | the game writes saves only at New Game / church; milestone files in `re/saves/` |
| bank, boat, carriage, monster sanctuary, chapter title cards | - | not reachable by warp: need event flags / `StoryStatus::setVehicleEnable` instance (not exported) / walking | open: add `FieldPlayerManager`/`StoryStatus` singletons by decompiling their `getSingleton` accessors |

### Debug options (testers)
Keys in `ux0:data/dqiv/config.txt` (all slots) or `dqivn<slot>.cfg`, inert unless present (`game/DevCheats.hpp`):
| key | effect |
|---|---|
| `nodamage=1` | the party takes no damage (`status::PartyStatusUtility::setNoDamageEnable`, re-armed every 30 frames) |
| `noencount=1` | no random encounters (`btl::Encount::brew` patched to return 0; execField / execDungeon start a battle only when it returns non-zero) |
| `gold=N` | set the gold once, ~2.5 s after the first real map loaded (the title backdrop does not count) |
| `party_hp=N` / `party_mp=N` | max HP / MP of the 8 party slots at the same moment (there is no level setter in the native debug interface) |
| `warp=<map>` | after boot, start in map `<map>` (`warp_chapter=N`, default 1; `warp_delay=F`, default 150 frames); map short names: `dumpmaps` in a `warp<slot>.on` script, e.g. `caout1` (town), `dab1` (cave), `snb1a` (ship cabin) |
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

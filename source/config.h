/*
 * config.h -- every path / address / size the native port depends on, in one place.
 *
 * DATA_PATH comes from CMake (-DDATA_PATH="ux0:data/dqiv/", trailing '/'); the rest is derived here.
 * Everything this port writes uses per-slot names (log, saves, cfg) so several worktrees can coexist.
 *
 *   lib/libdq4core.so lib/libsdlib_android.so   ARMv7 NEON libs of the APK (flat, original names)
 *   main.obb                                    the untouched OBB (libdq4core fopen()s + fseek()s it)
 *   pack/                                       index.bin + {meta,mesh,tex,blob}.pak   (tools/convert output)
 *   saves<slot>/                                DevInit savePath (data1..9.dat)
 *   dqivn<slot>.log, dqivn<slot>.cfg              this port's log (truncated every boot) and run cfg (slot = DQIVN_SLOT)
 */
#ifndef DQIVN_CONFIG_H
#define DQIVN_CONFIG_H

#ifndef DATA_PATH
#define DATA_PATH "ux0:data/dqiv/"
#endif

#define LIB_PATH         DATA_PATH "lib/"
#define OBB_PATH         DATA_PATH "main.obb"
#define ASSETS_PATH      DATA_PATH "assets/"
#define PACK_PATH        DATA_PATH "pack"
#ifndef DQIVN_SLOT
#define DQIVN_SLOT 1
#endif
#define DQIVN_STR2(x) #x
#define DQIVN_STR(x) DQIVN_STR2(x)
#define DQIVN_SLOT_STR    DQIVN_STR(DQIVN_SLOT)
#define SAVE_PATH        DATA_PATH "saves" DQIVN_SLOT_STR "/"
#define TRACE_PATH       DATA_PATH "trace" DQIVN_SLOT_STR "/"      /* per slot: several worktrees record/replay side by side */
#define TRACE_ON_PATH    DATA_PATH "trace" DQIVN_SLOT_STR ".on"     /* create it to record (content: optional frame count, default 600) */
#define REPLAY_ON_PATH   DATA_PATH "replay" DQIVN_SLOT_STR ".on"    /* create it to replay TRACE_PATH (see native/packet_replay.hpp) */
#define SHADER_CACHE     DATA_PATH "shader_cache"
#define LOG_FILE_PATH    DATA_PATH "dqivn" DQIVN_SLOT_STR ".log"
#define RUN_CFG_PATH     DATA_PATH "dqivn" DQIVN_SLOT_STR ".cfg"

/* Modules (fixed load addresses via kubridge). Two modules, ~3.4 MiB: libdq4core + libsdlib_android. */
#define MODULES_BASE_ADDR  0x98010000
#define MODULE_PATCH_SIZE  0x10000
#define KUSER_BLOCK_ADDR   0x9D000000

/* newlib heap (ATTRIBUTE2=12 in the SFO). Multiple of 16 MiB (Vita3K aligns blocks to the lowest set bit). */
#ifndef DQIV_HEAP_MB
#define DQIV_HEAP_MB 96     /* measured peak: ~8 MB title, ~6 MB field tests (docs: see README "Memory budget"); 96 keeps >10x headroom for battle/menus */
#endif
/* GL texture residency cap (Resources::EnforceTextureBudget, LRU of textures not drawn in the last 2 frames). */
#ifndef DQIV_TEX_BUDGET_MB
#define DQIV_TEX_BUDGET_MB 48
#endif
/* RAM left outside vitaGL's pool (thread stacks, audio, libdq4core mallocs outside the newlib heap): vitaGL takes all remaining USER_RW minus this. */
#ifndef DQIV_VGL_RAM_RESERVE_MB
#define DQIV_VGL_RAM_RESERVE_MB 24
#endif

/* Game thread: runs boot + the whole frame loop (GL context owner, libdq4core caller). */
#define GAME_THREAD_STACK  (4 * 1024 * 1024)

/* ---- screen (docs 06 §1) -------------------------------------------------------------------------
 * The game is portrait only. Logical surface = what Screen.width/height report and what every camera
 * renders into (one FBO); it is presented centred on the 960x544 display. "tablet" layout 408x544. */
#ifndef DQIV_LOGICAL_W
#define DQIV_LOGICAL_W 408
#endif
#ifndef DQIV_LOGICAL_H
#define DQIV_LOGICAL_H 544
#endif
#define DQIV_DISPLAY_W 960
#define DQIV_DISPLAY_H 544
#define DQIV_VIEW_X0   ((DQIV_DISPLAY_W - DQIV_LOGICAL_W) / 2)
#define DQIV_VIEW_Y0   ((DQIV_DISPLAY_H - DQIV_LOGICAL_H) / 2)

/* Boot parameters of libdq4core: language ENGLISH (DQ4's DevInit has no deviceInfo argument; the PowerVR/SGX bias is Boot.s_gpuName -> Field.SystemInit). */
#define DQIV_LANG         1

#endif

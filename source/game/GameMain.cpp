/*
 * GameMain.cpp -- boot orchestration (WP3). Runs on the game thread created by main.cpp.
 *
 *   phase 1  platform: log, clocks, directories, kubridge check
 *   phase 2  so-loader: libdq4core.so + libsdlib_android.so (load, relocate, link, patch, ctors, JNI_OnLoad)
 *   phase 3  engine + game: Engine::Init, level0 / Boot / MainLoop, then the 30 Hz frame loop
 */
#include <cstdio>
#include <cstring>

#include <psp2/appmgr.h>
#include <psp2/apputil.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/clib.h>
#include <psp2/power.h>

#include <falso_jni/FalsoJNI.h>

#include "config.h"
#include "loader/loader.h"
#include "native/native_bridge.hpp"
#include "utils/logger.h"
#include "utils/utils.h"

int dqiv_game_run();   // game/GameLoop.cpp: engine init + frame loop

static void platform_init() {
    sceIoMkdir(DATA_PATH, 0777);
    log_file_reset();
    l_success("Dragon Quest IV (native port) starting, data: %s", DATA_PATH);

    SceAppUtilInitParam init_param = {};
    SceAppUtilBootParam boot_param = {};
    sceAppUtilInit(&init_param, &boot_param);

    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    if (!module_loaded("kubridge")) {
        l_fatal("kubridge is not loaded.");
        fatal_error("kubridge.skprx is not installed (taiHEN *KERNEL entry missing).");
    }
    l_success("kubridge check passed.");

    sceIoMkdir(LIB_PATH, 0777);
    sceIoMkdir(SAVE_PATH, 0777);
    sceIoMkdir(TRACE_PATH, 0777);
}

static void soloader_boot() {
    if (loader_load_all() < 0) fatal_error("loading the game libraries failed (%s)", LIB_PATH);

    jni_init();
    l_success("FalsoJNI initialized.");

    loader_run_ctors();

    int n = loader_call_jni_onload(&jvm);
    l_success("JNI_OnLoad called on %d modules", n);

    if (!NativeCall::Bind()) fatal_error("libdq4core.so: missing exports");
    int loaded = NativeCall::IsLoadNativeCode();
    l_info("IsLoadNativeCode() = %d", loaded);   // Boot.cs spins on this; 1 after dq4core's JNI_OnLoad
}

int dqiv_game_main(void) {
    platform_init();
    soloader_boot();
    int r = dqiv_game_run();
    l_warn("dqiv_game_run returned %d", r);
    return r;
}

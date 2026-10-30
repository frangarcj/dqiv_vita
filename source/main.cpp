/*
 * main.cpp -- process entry: heap size, big-stack game thread, nothing else.
 * Everything (so-loader boot, GL, libdq4core calls, the frame loop) runs on ONE thread (game/GameMain.cpp);
 * libsdlib's own threads (update + OpenSL callback) are the only others.
 */
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>

#include "config.h"

/* newlib heap. ATTRIBUTE2=12 in the SFO gives the extended memory. Keep a multiple of 16 MiB and below the
 * module window (MODULES_BASE_ADDR) so Vita3K's alignment rule does not swallow the .so images. */
extern "C" {
int _newlib_heap_size_user = DQIV_HEAP_MB * 1024 * 1024;
}

int dqiv_game_main(void);   /* game/GameMain.cpp */

static int game_thread(SceSize, void *) {
    int r = dqiv_game_main();
    sceClibPrintf("DQIVN: dqiv_game_main returned %d\n", r);
    return r;
}

int main() {
    SceUID thid = sceKernelCreateThread("dqivn_game", game_thread, 0x40, GAME_THREAD_STACK, 0, 0, NULL);
    if (thid < 0) {
        sceClibPrintf("DQIVN: cannot create the game thread: 0x%08x\n", thid);
        return 1;
    }
    sceKernelStartThread(thid, 0, NULL);
    sceKernelWaitThreadEnd(thid, NULL, NULL);
    sceKernelExitProcess(0);
    return 0;
}

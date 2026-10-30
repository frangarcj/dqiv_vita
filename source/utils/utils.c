#include "utils/utils.h"
#include "utils/logger.h"
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool file_exists(const char *path) {
    SceIoStat st;
    return sceIoGetstat(path, &st) >= 0;
}

bool file_mkpath(const char *path, mode_t mode) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            sceIoMkdir(tmp, mode);
            *p = '/';
        }
    }
    return sceIoMkdir(tmp, mode) >= 0 || file_exists(tmp);
}

int _vshKernelSearchModuleByName(const char *name, int *unk);   /* SceVshBridge */
bool module_loaded(const char *name) {
    int unk[2];
    return _vshKernelSearchModuleByName(name, unk) >= 0;
}

void fatal_error(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    l_fatal("FATAL: %s", buf);
    sceClibPrintf("DQIVN FATAL: %s\n", buf);
    sceKernelDelayThread(500 * 1000);
    sceKernelExitProcess(1);
    for (;;) {}
}

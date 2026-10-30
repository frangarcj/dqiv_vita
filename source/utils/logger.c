/*
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/logger.h"

#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "config.h"

#include <stdbool.h>
#include <stdatomic.h>

#define COLOR_RED    "\x1B[38;5;196m"
#define COLOR_PINK   "\x1B[38;5;212m"
#define COLOR_ORANGE "\x1B[38;5;202m"
#define COLOR_BLUE   "\x1B[38;5;32m"
#define COLOR_GREEN  "\x1B[32m"
#define COLOR_CYAN   "\x1B[36m"

#define COLOR_END    "\033[0m"

static SceKernelLwMutexWork _log_mutex;
static atomic_bool _log_mutex_ready = ATOMIC_VAR_INIT(false);

// Buffer A is used to adjust the format string.
static char buffer_a[2048];
// Buffer B is used to compile the final log using the updated format string.
static char buffer_b[2048];
// Buffer C: plain (uncoloured) copy for the log file.
static char buffer_c[2048];
static SceUID log_fd = -1;

/* Debug builds: open/append/close for every line so nothing is lost when the process is killed or
 * crashes (Vita3K buffers host file writes). Release builds keep one descriptor open. */
#ifdef DEBUG_SOLOADER
#define LOG_REOPEN 1
#else
#define LOG_REOPEN 0
#endif

/* Truncates the log file. Call once at boot (before any thread is created). */
void log_file_reset(void) {
    if (log_fd >= 0) sceIoClose(log_fd);
    log_fd = sceIoOpen(LOG_FILE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (LOG_REOPEN && log_fd >= 0) { sceIoClose(log_fd); log_fd = -1; }
}

static void log_file_emit(const char *buf, int n) {
    if (LOG_REOPEN) {
        SceUID fd = sceIoOpen(LOG_FILE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0666);
        if (fd >= 0) { sceIoWrite(fd, buf, n); sceIoClose(fd); }
        return;
    }
    if (log_fd < 0) {
        log_fd = sceIoOpen(LOG_FILE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0666);
        if (log_fd < 0) return;
    }
    sceIoWrite(log_fd, buf, n);
}

static void log_file_write(int t, const char *msg) {
    static const char *tag[] = { "DBG", "INF", "WRN", "ERR", "FTL", "OK ", "..." };
    int n = snprintf(buffer_c, sizeof(buffer_c), "[%s] %s\n", tag[t], msg);
    if (n > (int)sizeof(buffer_c) - 1) n = sizeof(buffer_c) - 1;
    log_file_emit(buffer_c, n);
}

void _log_print(int t, const char* fmt, ...) {
    if (!atomic_load_explicit(&_log_mutex_ready, memory_order_relaxed)) {
        int ret = sceKernelCreateLwMutex(&_log_mutex, "log_lock", 0, 0, NULL);
        if (ret < 0) {
            sceClibPrintf("Error: failed to create log mutex: 0x%x\n", ret);
            return;
        }
        atomic_store_explicit(&_log_mutex_ready, true, memory_order_relaxed);
    }
    sceKernelLockLwMutex(&_log_mutex, 1, NULL);

    switch (t) {
        case LT_DEBUG:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s• debug%s    %s\n",
                            COLOR_PINK, COLOR_END, fmt); break;
        case LT_INFO:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %sℹ info%s     %s\n",
                            COLOR_BLUE, COLOR_END, fmt); break;
        case LT_WARN:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s⚠ warning%s  %s\n",
                            COLOR_ORANGE, COLOR_END, fmt); break;
        case LT_ERROR:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s⨯ error%s    %s\n",
                            COLOR_RED, COLOR_END, fmt); break;
        case LT_FATAL:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s! fatal%s    %s\n",
                            COLOR_RED, COLOR_END, fmt); break;
        case LT_SUCCESS:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s! success%s  %s\n",
                            COLOR_GREEN, COLOR_END, fmt); break;
        case LT_WAIT:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s… waiting%s  %s\n",
                            COLOR_CYAN, COLOR_END, fmt); break;
        default:
            if (atomic_load_explicit(&_log_mutex_ready, memory_order_relaxed)) {
                sceKernelUnlockLwMutex(&_log_mutex, 1);
            }
            return;
    }

    va_list list;
    va_start(list, fmt);
    sceClibVsnprintf(buffer_b, sizeof(buffer_b), buffer_a, list);
    va_end(list);
    sceClibPrintf("%s", buffer_b);
    {
        va_list l2;
        char plain[1536];
        va_start(l2, fmt);
        vsnprintf(plain, sizeof(plain), fmt, l2);
        va_end(l2);
        log_file_write(t, plain);
    }

    if (atomic_load_explicit(&_log_mutex_ready, memory_order_relaxed)) {
        sceKernelUnlockLwMutex(&_log_mutex, 1);
    }
}

/* Raw line to console + log file (used by FalsoJNI's logger). */
void log_raw(const char *s) {
    sceClibPrintf("%s", s);
    log_file_emit(s, (int)strlen(s));
}

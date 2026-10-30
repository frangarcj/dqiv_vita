/*
 * bionic.c -- the small slice of Android's libc / libstdc++ / liblog that libdq4core.so and
 * libsdlib_android.so import and that cannot be taken from newlib as is (see dynlib_table.inc).
 * Everything else in the table maps straight to newlib/libm/libstdc++.
 */
#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>

#include "compat/bionic.h"
#include "loader/loader.h"
#include "utils/logger.h"

/* ---- liblog ------------------------------------------------------------------------------------ */
int __android_log_write(int prio, const char *tag, const char *text) {
    /* prio: 2 verbose 3 debug 4 info 5 warn 6 error 7 fatal */
    if (prio >= 6) l_error("[ALOG][%s] %s", tag ? tag : "", text ? text : "");
    else           l_info("[ALOG][%s] %s", tag ? tag : "", text ? text : "");
    return 0;
}

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
    char text[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    return __android_log_write(prio, tag, text);
}

/* ---- C++ ABI ------------------------------------------------------------------------------------- */
static SceKernelLwMutexWork guard_lock;
static volatile int guard_lock_ready;

static void glock(void) {
    if (!guard_lock_ready) {
        static volatile int creating;
        if (__sync_lock_test_and_set(&creating, 1) == 0) {
            sceKernelCreateLwMutex(&guard_lock, "cxa_guard", SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0, NULL);
            guard_lock_ready = 1;
        } else {
            while (!guard_lock_ready) sceKernelDelayThread(100);
        }
    }
    sceKernelLockLwMutex(&guard_lock, 1, NULL);
}

int __cxa_guard_acquire_soloader(volatile uint32_t *g) {
    if (*(volatile uint8_t *)g) return 0;
    glock();
    if (*(volatile uint8_t *)g) { sceKernelUnlockLwMutex(&guard_lock, 1); return 0; }
    return 1;
}
void __cxa_guard_release_soloader(volatile uint32_t *g) {
    __sync_synchronize();
    *(volatile uint8_t *)g = 1;
    sceKernelUnlockLwMutex(&guard_lock, 1);
}
void __cxa_guard_abort_soloader(volatile uint32_t *g) { (void)g; sceKernelUnlockLwMutex(&guard_lock, 1); }

int  __cxa_atexit_soloader(void (*f)(void *), void *arg, void *dso) { (void)f; (void)arg; (void)dso; return 0; }
int  __aeabi_atexit_soloader(void *obj, void (*dtor)(void *), void *dso) { (void)obj; (void)dtor; (void)dso; return 0; }
void __cxa_finalize_soloader(void *dso) { (void)dso; }

void __cxa_pure_virtual_soloader(void) {
    l_fatal("pure virtual method called from %p", __builtin_return_address(0));
    abort();
}

/* ---- process ------------------------------------------------------------------------------------- */
void __stack_chk_fail_soloader(void) {
    l_fatal("stack smashing detected, caller %p", __builtin_return_address(0));
    abort();
}

void abort_soloader(void) {
    game_module *g = loader_by_addr((uintptr_t)__builtin_return_address(0));
    l_fatal("abort() called from %p (%s+0x%x)", __builtin_return_address(0), g ? g->file : "?",
            g ? (unsigned)((uintptr_t)__builtin_return_address(0) - g->mod.load_addr) : 0);
    abort();
}

int raise_soloader(int sig) {
    game_module *g = loader_by_addr((uintptr_t)__builtin_return_address(0));
    l_fatal("raise(%d) from %p (%s+0x%x)", sig, __builtin_return_address(0), g ? g->file : "?",
            g ? (unsigned)((uintptr_t)__builtin_return_address(0) - g->mod.load_addr) : 0);
    if (sig == 6 /* SIGABRT */ || sig == 11 /* SIGSEGV */) abort();
    return 0;
}

clock_t clock_soloader(void) { return (clock_t)sceKernelGetProcessTimeLow(); }

/* ---- misc ---------------------------------------------------------------------------------------- */
int __page_size_soloader = 4096;

/* ARM __aeabi_memset has (dst, n, c) order (not used by our two libs today; kept for safety) */
void __aeabi_memset_soloader(void *dst, size_t n, int c) { memset(dst, c, n); }
void __aeabi_memcpy_soloader(void *dst, const void *src, size_t n) { memcpy(dst, src, n); }
void __aeabi_memmove_soloader(void *dst, const void *src, size_t n) { memmove(dst, src, n); }

/* OpenSLES (vitasdk) mixes at a fixed rate; libsdlib's core mixer runs at 32 kHz (REPORT_RUNTIME option A) */
int _opensles_user_freq = 32000;

/*
 * dynlib.cpp -- our "libc/libm/libGLESv2/libOpenSLES/libandroid" for libdq4core.so and libsdlib_android.so.
 *
 * C++ on purpose: we build with the standard (hardfp) VitaSDK while the Android .so files use the softfp
 * calling convention (floats/doubles in r0-r3). Every import whose signature contains float/double is
 * registered through WRAP(), which emits a pcs("aapcs") trampoline (softfp_wrappers.hpp). Variadic
 * functions already use the base AAPCS in both ABIs and are registered directly.
 *
 * The table (dynlib_table.inc) is the union of refs/imports_libdq4core.txt and refs/imports_libsdlib_android.txt.
 * Check it with tools/check_imports.py. Lookup is by name (loader.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <new>

#include <psp2/kernel/clib.h>
#include <so_util/so_util.h>

#include <vitaGL.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "compat/bionic.h"
#include "compat/pthr.h"
#include "native/asset_manager.h"
#include "native/opensles_shim.hpp"
#include "loader/loader.h"
#include "loader/softfp_wrappers.hpp"

extern "C" {
extern void *__stack_chk_guard;
int *__errno(void);
/* libsupc++ ARM EHABI helpers libdq4core links against (never reached unless a C++ exception unwinds) */
void __cxa_begin_cleanup(void);
void __cxa_call_unexpected(void*);
void __cxa_type_match(void);
}

#define PW(f) (uintptr_t)&f

static so_default_dynlib default_dynlib[] = {
#include "dynlib_table.inc"
};

extern "C" uintptr_t dynlib_lookup(const char *symbol) {
    for (size_t i = 0; i < sizeof(default_dynlib) / sizeof(default_dynlib[0]); i++) {
        if (strcmp(symbol, default_dynlib[i].symbol) == 0)
            return default_dynlib[i].func;
    }
    return 0;
}

extern "C" size_t dynlib_count(void) {
    return sizeof(default_dynlib) / sizeof(default_dynlib[0]);
}

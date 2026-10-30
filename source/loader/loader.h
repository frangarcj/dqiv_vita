/*
 * loader.h -- so-loader for the two Android libraries the native port still uses:
 *   libdq4core.so (game logic, NEON build) and libsdlib_android.so (audio, OpenSL ES).
 * Loads/relocates/links them at fixed addresses (kubridge), runs constructors and JNI_OnLoad.
 */
#ifndef DQIVN_LOADER_H
#define DQIVN_LOADER_H

#include <stdint.h>
#include <so_util/so_util.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct game_module {
    const char *name;     /* short name: "dq4core", "sdlib_android" */
    const char *file;     /* file name under LIB_PATH */
    int         required; /* 1: failing to load is fatal */
    so_module   mod;
    int         loaded;
} game_module;

#define N_GAME_MODULES 2
extern game_module g_modules[N_GAME_MODULES];

int  loader_load_all(void);          /* load + relocate + link + patch + flush caches; 0 or <0 */
void loader_run_ctors(void);         /* DT_INIT_ARRAY of every module, in order */
int  loader_call_jni_onload(void *vm);  /* JNI_OnLoad(vm, NULL) of every module that has one; returns count */

game_module *loader_by_name(const char *name);       /* "dq4core", "libdq4core.so", ... */
game_module *loader_by_addr(uintptr_t addr);          /* module whose text/data contains addr */
uintptr_t    loader_symbol(const char *module_name, const char *sym);   /* 0 if absent */

void *dl_unwind_find_exidx_soloader(uintptr_t pc, int *pcount);   /* ARM EHABI: .ARM.exidx covering pc */

/* dynlib.cpp (C++ because of the softfp wrappers) */
uintptr_t dynlib_lookup(const char *symbol);   /* 0 if not in the table */
size_t    dynlib_count(void);

void so_patch(void);   /* patch.c */

#ifdef __cplusplus
}
#endif
#endif

/*
 * loader.c -- two-module so-loader (libdq4core + libsdlib_android), see loader.h.
 *
 * Import resolution order for every undefined symbol:
 *   1. our dynlib table, only for symbols in prefer_table[] (ABI glue that must see all modules)
 *   2. the other loaded game module, if it exports it (libdq4core exports __gnu_Unwind_Find_exidx etc.)
 *   3. our dynlib table (bionic -> newlib/vitaGL/OpenSLES/NDK reimplementations, softfp wrappers)
 * Unresolved JUMP_SLOTs are pointed at so_util's plt0_stub, which aborts with the symbol name.
 */
#include "loader/loader.h"
#include "config.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <psp2/kernel/clib.h>
#include <psp2/io/fcntl.h>

#include "utils/logger.h"
#include "utils/utils.h"

extern void plt0_stub(void);   /* so_util.c */

game_module g_modules[N_GAME_MODULES] = {
    /* name            file                    req */
    { "dq4core",       "libdq4core.so",         1 },
    { "sdlib_android", "libsdlib_android.so",   1 },
};

static const char *const prefer_table[] = {
    "dl_unwind_find_exidx",
    NULL
};

static int is_preferred(const char *s) {
    for (int i = 0; prefer_table[i]; i++)
        if (strcmp(prefer_table[i], s) == 0) return 1;
    return 0;
}

static uintptr_t align_up(uintptr_t v, uintptr_t a) { return (v + a - 1) & ~(a - 1); }

static uintptr_t module_end(const so_module *m) {
    uintptr_t end = m->text_base + m->text_size;
    for (int i = 0; i < m->n_data; i++) {
        uintptr_t e = m->data_base[i] + m->data_size[i];
        if (e > end) end = e;
    }
    return end;
}

static int load_one(game_module *g, uintptr_t *next_addr) {
    char path[256];
    snprintf(path, sizeof(path), LIB_PATH "%s", g->file);

    if (!file_exists(path)) {
        l_error("%s not found (%s)", g->file, path);
        return -1;
    }
    int r = so_file_load(&g->mod, path, *next_addr);
    if (r < 0) {
        l_error("so_file_load(%s) failed: 0x%08x (addr 0x%08x)", g->file, r, (unsigned)*next_addr);
        return r;
    }
    g->loaded = 1;
    l_success("loaded %-20s text=%08x+%07x data=%08x exidx=%08x init_array=%u", g->file,
              (unsigned)g->mod.text_base, (unsigned)g->mod.text_size,
              (unsigned)(g->mod.n_data ? g->mod.data_base[0] : 0),
              (unsigned)g->mod.exidx_base, (unsigned)g->mod.num_init_array);
    *next_addr = align_up(module_end(&g->mod), 0x10000) + 0x10000 + MODULE_PATCH_SIZE;
    return 0;
}

uintptr_t loader_symbol(const char *module_name, const char *sym) {
    game_module *g = loader_by_name(module_name);
    return (g && g->loaded) ? so_symbol(&g->mod, sym) : 0;
}

static uintptr_t lookup_in_modules(const game_module *skip, const char *name, const char **from) {
    for (int i = 0; i < N_GAME_MODULES; i++) {
        game_module *o = &g_modules[i];
        if (!o->loaded || o == skip) continue;
        uintptr_t a = so_symbol(&o->mod, name);
        if (a) { if (from) *from = o->file; return a; }
    }
    return 0;
}

static void link_module(game_module *g) {
    const so_module *mod = &g->mod;
    unsigned n_mod = 0, n_tab = 0, n_unres = 0, n_weak = 0, n_all = 0;

    for (uint32_t i = 0; i < mod->num_reldyn + mod->num_relplt; i++) {
        Elf32_Rel *rel = i < mod->num_reldyn ? &mod->reldyn[i] : &mod->relplt[i - mod->num_reldyn];
        Elf32_Sym *sym = &mod->dynsym[ELF32_R_SYM(rel->r_info)];
        uintptr_t *ptr = (uintptr_t *)(mod->load_addr + rel->r_offset);
        int type = ELF32_R_TYPE(rel->r_info);

        if (type != R_ARM_ABS32 && type != R_ARM_GLOB_DAT && type != R_ARM_JUMP_SLOT)
            continue;
        if (sym->st_shndx != SHN_UNDEF)
            continue;

        const char *name = mod->dynstr + sym->st_name;
        const char *from = NULL;
        uintptr_t addr = 0;
        n_all++;

        if (is_preferred(name)) addr = dynlib_lookup(name);
        if (!addr) {
            addr = lookup_in_modules(g, name, &from);
            if (addr) {
                n_mod++;
#ifdef DQIV_TRACE_LINK
                l_debug("link %s: %s <- %s", g->file, name, from);
#endif
            }
        }
        if (!addr) {
            addr = dynlib_lookup(name);
            if (addr) n_tab++;
        }
        if (!addr) {
            if (ELF32_ST_BIND(sym->st_info) == STB_WEAK) {
                n_weak++;
                l_debug("%s: weak import %s left NULL", g->file, name);
            } else {
                n_unres++;
                l_error("UNRESOLVED import in %s: %s", g->file, name);
                if (type == R_ARM_JUMP_SLOT) addr = (uintptr_t)&plt0_stub;
            }
        }
        if (type == R_ARM_ABS32) *ptr += addr;   /* addend lives in place (e.g. &__sF[1]) */
        else                     *ptr  = addr;
    }
    l_info("link %-20s imports=%u  from-modules=%u  from-table=%u  weak-null=%u  UNRESOLVED=%u",
           g->file, n_all, n_mod, n_tab, n_weak, n_unres);
}

int loader_load_all(void) {
    uintptr_t next = MODULES_BASE_ADDR;

    for (int i = 0; i < N_GAME_MODULES; i++) {
        game_module *g = &g_modules[i];
        if (load_one(g, &next) < 0 && g->required)
            fatal_error("Could not load %s.\nCopy the .so files from the APK's lib/armeabi-v7a to %s", g->file, LIB_PATH);
    }
    for (int i = 0; i < N_GAME_MODULES; i++)
        if (g_modules[i].loaded) so_relocate(&g_modules[i].mod);
    l_success("all modules relocated");

    for (int i = 0; i < N_GAME_MODULES; i++)
        if (g_modules[i].loaded) link_module(&g_modules[i]);
    l_success("all imports linked");

    so_patch();
    l_success("modules patched");

    for (int i = 0; i < N_GAME_MODULES; i++)
        if (g_modules[i].loaded) so_flush_caches(&g_modules[i].mod);
    l_success("caches flushed");
    return 0;
}

void loader_run_ctors(void) {
    for (int i = 0; i < N_GAME_MODULES; i++) {
        game_module *g = &g_modules[i];
        if (!g->loaded) continue;
        l_info("running %u constructors of %s ...", (unsigned)g->mod.num_init_array, g->file);
        so_initialize(&g->mod);
        l_success("constructors of %s done", g->file);
    }
}

int loader_call_jni_onload(void *vm) {
    int n = 0;
    for (int i = 0; i < N_GAME_MODULES; i++) {
        game_module *g = &g_modules[i];
        if (!g->loaded) continue;
        uintptr_t f = so_symbol(&g->mod, "JNI_OnLoad");
        if (!f) { l_warn("%s: no JNI_OnLoad", g->file); continue; }
        l_info("calling %s JNI_OnLoad (%p) ...", g->file, (void *)f);
        int ret = ((int (*)(void *, void *))f)(vm, NULL);
        l_success("%s JNI_OnLoad returned 0x%x (JNI version)", g->file, ret);
        n++;
    }
    return n;
}

/* "/data/app-lib/x/libfoo.so" | "libfoo.so" | "foo" -> "foo" (written into out) */
static void normalize_name(const char *in, char *out, size_t n) {
    const char *b = strrchr(in, '/');
    b = b ? b + 1 : in;
    if (strncmp(b, "lib", 3) == 0) b += 3;
    snprintf(out, n, "%s", b);
    size_t l = strlen(out);
    if (l > 3 && strcmp(out + l - 3, ".so") == 0) out[l - 3] = 0;
}

game_module *loader_by_name(const char *name) {
    char want[128], cand[128];
    normalize_name(name, want, sizeof(want));
    for (int i = 0; i < N_GAME_MODULES; i++) {
        normalize_name(g_modules[i].file, cand, sizeof(cand));
        if (strcmp(want, cand) == 0) return &g_modules[i];
    }
    return NULL;
}

game_module *loader_by_addr(uintptr_t a) {
    for (int i = 0; i < N_GAME_MODULES; i++) {
        game_module *g = &g_modules[i];
        if (!g->loaded) continue;
        if (a >= g->mod.text_base && a < g->mod.text_base + g->mod.text_size) return g;
        for (int d = 0; d < g->mod.n_data; d++)
            if (a >= g->mod.data_base[d] && a < g->mod.data_base[d] + g->mod.data_size[d]) return g;
    }
    return NULL;
}

void *dl_unwind_find_exidx_soloader(uintptr_t pc, int *pcount) {
    for (int i = 0; i < N_GAME_MODULES; i++) {
        game_module *g = &g_modules[i];
        if (!g->loaded || !g->mod.exidx_base) continue;
        if (pc >= g->mod.text_base && pc < g->mod.text_base + g->mod.text_size) {
            if (pcount) *pcount = (int)(g->mod.exidx_size / 8);
            return (void *)g->mod.exidx_base;
        }
    }
    if (pcount) *pcount = 0;
    return NULL;
}

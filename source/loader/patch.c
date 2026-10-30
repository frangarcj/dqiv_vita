/*
 * patch.c -- code patches applied to the loaded modules.
 * Old NDK toolchains emit calls to the Linux "kuser helpers" page (0xFFFF0FC0 cmpxchg, 0xFFFF0FA0
 * memory barrier). Redirect every such literal in every module to a small RWX block of ours.
 * Per-function hooks (hook_addr(so_symbol(...), fn)) go into so_patch() too.
 */
#include <kubridge.h>
#include <so_util/so_util.h>
#include <string.h>
#include <vitasdk.h>

#include "config.h"
#include "loader/loader.h"
#include "utils/logger.h"
#include "utils/utils.h"

#define SCE_KERNEL_MEMBLOCK_TYPE_USER_RX (0x0C20D050)

void __kuser_memory_barrier(void) { __sync_synchronize(); }

int __atomic_cmpxchg(int old_value, int new_value, volatile int *ptr) {
    return __sync_val_compare_and_swap(ptr, old_value, new_value) != old_value;   /* 0 on success */
}

static void kuser_patch(void) {
    SceKernelAllocMemBlockKernelOpt opt;
    memset(&opt, 0, sizeof(opt));
    opt.size = sizeof(opt);
    opt.attr = 0x1;
    opt.field_C = (SceUInt32)KUSER_BLOCK_ADDR;
    if (kuKernelAllocMemBlock("atomic", SCE_KERNEL_MEMBLOCK_TYPE_USER_RX, 0x1000, &opt) < 0)
        fatal_error("could not allocate the kuser atomic block");
    kuKernelMemProtect((void *)KUSER_BLOCK_ADDR, 0x1000, KU_KERNEL_PROT_EXEC | KU_KERNEL_PROT_READ | KU_KERNEL_PROT_WRITE);

    hook_addr(KUSER_BLOCK_ADDR + 0xFA0, (uintptr_t)__kuser_memory_barrier);
    hook_addr(KUSER_BLOCK_ADDR + 0xFC0, (uintptr_t)__atomic_cmpxchg);

    for (int i = 0; i < N_GAME_MODULES; i++) {
        game_module *g = &g_modules[i];
        if (!g->loaded) continue;
        int n_cmpxchg = 0, n_barrier = 0;
        for (uint32_t addr = g->mod.text_base; addr < g->mod.text_base + g->mod.text_size; addr += 4) {
            uint32_t *a = (uint32_t *)addr;
            uint32_t patched_addr;
            if (*a == 0xFFFF0FC0) {
                patched_addr = KUSER_BLOCK_ADDR + 0xFC0;
                kuKernelCpuUnrestrictedMemcpy((void *)addr, &patched_addr, sizeof(uint32_t));
                n_cmpxchg++;
            } else if (*a == 0xFFFF0FA0) {
                patched_addr = KUSER_BLOCK_ADDR + 0xFA0;
                kuKernelCpuUnrestrictedMemcpy((void *)addr, &patched_addr, sizeof(uint32_t));
                n_barrier++;
            }
        }
        l_info("kuser patch %-20s cmpxchg=%d barrier=%d", g->file, n_cmpxchg, n_barrier);
    }
}

void so_patch(void) {
    kuser_patch();
    /* module-specific patches (e.g. sdlib audio, see REPORT_RUNTIME option B) go here */
}

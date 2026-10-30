/* test_pack.c - loads every object of a DQIV pack, validates structure / references / index ordering.
 *   cc -O2 -std=c99 -o test_pack test_pack.c dqiv_pack.c && ./test_pack /path/to/pack
 * Exit status 0 == all good.  Objects with size 0 (conversion failures recorded in failures.txt) are only counted. */
#include "dqiv_pack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : "/Users/frangar/Fun/dqiv/work/extract/pack";
    DqvPack* p = dqiv_open(dir);
    uint32_t i, n, errors = 0, missing = 0, kcount[DQIV_K_COUNT] = {0};
    uint64_t kbytes[DQIV_K_COUNT] = {0};
    char err[256];
    clock_t t0 = clock();
    static const char* kn[] = {"?", "Texture", "Mesh", "Material", "Shader", "Prefab", "AnimClip", "TextAsset", "Font"};
    if (!p) { fprintf(stderr, "cannot open pack %s\n", dir); return 2; }
    n = dqiv_object_count(p);
    printf("pack %s: %u objects, %u resource entries\n", dir, n - 1, dqiv_res_count(p));
    /* 1. index sanity */
    for (i = 1; i < dqiv_res_count(p); i++) {
        const DqvResEntry *a = dqiv_res_entry(p, i - 1), *b = dqiv_res_entry(p, i);
        int c = strcmp(dqiv_str(p, a->path_off), dqiv_str(p, b->path_off));
        if (c > 0 || (c == 0 && a->kind > b->kind)) { if (errors++ < 20) printf("ERR resource table not sorted at %u\n", i); }
    }
    for (i = 0; i < dqiv_res_count(p); i++) {
        const DqvResEntry* e = dqiv_res_entry(p, i);
        const DqvObject* o = dqiv_object(p, e->obj_id);
        const char* path = dqiv_str(p, e->path_off);
        uint32_t cnt, first;
        if (!o || o->kind != e->kind || !e->obj_id) { if (errors++ < 20) printf("ERR res %u (%s) -> bad object\n", i, path); continue; }
        first = dqiv_find_range(p, path, &cnt);
        if (first > i || i >= first + cnt) { if (errors++ < 20) printf("ERR find_range(%s) misses entry %u\n", path, i); }
        if (!dqiv_find(p, path, DQIV_KMASK(e->kind))) { if (errors++ < 20) printf("ERR find(%s) failed\n", path); }
    }
    /* 2. load + validate every object */
    for (i = 1; i < n; i++) {
        const DqvObject* o = dqiv_object(p, i);
        uint32_t size = 0;
        void* b;
        if (o->kind < 1 || o->kind > 8) { if (errors++ < 20) printf("ERR object %u bad kind %u\n", i, o->kind); continue; }
        if (!o->size) { missing++; continue; }
        b = dqiv_load(p, i, &size);
        if (!b) { if (errors++ < 20) printf("ERR object %u (%s) load failed\n", i, dqiv_str(p, o->name_off)); continue; }
        if (((uintptr_t)b & 15) || (o->offset & 15)) { if (errors++ < 20) printf("ERR object %u misaligned\n", i); }
        if (dqiv_validate(p, i, b, size, err, sizeof err)) {
            if (errors++ < 40) printf("ERR %s #%u '%s': %s\n", kn[o->kind], i, dqiv_str(p, o->name_off), err);
        }
        kcount[o->kind]++; kbytes[o->kind] += size;
        dqiv_free(b);
    }
    /* 3. spot-check the API the engine will use */
    {
        uint32_t id = dqiv_find(p, "scene/level0", DQIV_KMASK(DQIV_K_PREFAB)), sz;
        void* b = id ? dqiv_load(p, id, &sz) : NULL;
        const DqvPrefab* pf = b ? dqiv_as_prefab(b, sz) : NULL;
        printf("scene/level0 -> id %u, %u nodes, %u components\n", id, pf ? pf->node_count : 0, pf ? pf->comp_count : 0);
        if (!pf) { printf("ERR scene/level0 missing\n"); errors++; }
        dqiv_free(b);
        id = dqiv_find(p, "Menu/mplus-2c-medium_16", DQIV_KMASK(DQIV_K_FONT));
        printf("Menu/mplus-2c-medium_16 (font) -> id %u\n", id);
        if (!id) { printf("ERR font missing\n"); errors++; }
        id = dqiv_find(p, "CharaTex/h001", DQIV_KMASK(DQIV_K_TEXTURE));
        printf("CharaTex/h001 -> id %u\n", id);
    }
    printf("\n%-10s %8s %14s\n", "kind", "objects", "bytes");
    for (i = 1; i < DQIV_K_COUNT; i++) printf("%-10s %8u %14llu\n", kn[i], kcount[i], (unsigned long long)kbytes[i]);
    printf("\nabsent (size 0, conversion failed): %u, errors: %u, %.1fs\n", missing, errors, (double)(clock() - t0) / CLOCKS_PER_SEC);
    dqiv_close(p);
    puts(errors ? "FAIL" : "OK");
    return errors ? 1 : 0;
}

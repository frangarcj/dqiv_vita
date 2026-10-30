/*
 * asset_manager.c -- NDK AAssetManager shim (see re/REPORT_JNI_DATA.md 3.2); shared with the Unity experiment, behaviour identical.
 *
 * Two kinds of assets are served:
 *   1. OBB entries. libdq4core's file loader and libsdlib's SQEX::File (BGM streaming, path
 *      "<Application.dataPath>/sound/<name>.akb" == "<obb>/sound/...") call AAssetManager_open with a
 *      path that contains ".obb/" and possibly a NULL manager. Everything up to and including ".obb/"
 *      is stripped, the rest is looked up (lowercase) in a table built here from the OBB zip directory (the same table the game
 *      itself builds: all game content is STORED in the OBB, so an asset is simply a window
 *      {data offset, size} onto main.obb).
 *   2. plain files below ASSETS_PATH (none needed by DQ4).
 *
 * table layout (was assets/filelist.bin in DQ5): u32 count, then count * 20-byte entries
 *   { u32 namePtr (overwritten by the game), u32 localHeaderOffset, u32 size, u16 nameLen | u16 dataDelta << 16,
 *     u32 nameOffset }
 * names (lowercase, no "assets/" prefix) at (4 + nameOffset); the data of an entry starts at
 * localHeaderOffset + dataDelta in main.obb.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/threadmgr.h>

#include "config.h"
#include "native/asset_manager.h"
#include "utils/logger.h"

typedef struct {
    SceUID fd;
    int64_t base;      /* absolute offset of the window in the file */
    int64_t size;      /* window size */
    int64_t pos;       /* position inside the window */
} fake_asset;

/* 64-bit variants (API 13+); not imported by the game libs today but cheap to offer */
int64_t AAsset_seek64(AAsset *asset, int64_t offset, int whence);
int64_t AAsset_getLength64(AAsset *asset);
int64_t AAsset_getRemainingLength64(AAsset *asset);
off_t AAsset_getRemainingLength(AAsset *asset);

static char g_fake_mgr;

AAssetManager *AAssetManager_fromJava(void *env, void *assetManager) {
    (void)env; (void)assetManager;
    return (AAssetManager *)&g_fake_mgr;
}

/* ------------------------------------------------------------------------------------------ */
/* OBB asset table                                                                             */
/* ------------------------------------------------------------------------------------------ */

typedef struct {
    uint32_t ptr, hdr_off, size, namelen_delta, name_off;
} fl_entry;

static uint8_t *g_fl;          /* whole file */
static uint32_t g_fl_count;
static int g_fl_tried;

/* DQ4 has no assets/filelist.bin (DQ5's Java side built it from the OBB): libdq4core parses the OBB zip itself, so we build the same
 * table here from the zip central directory of main.obb: g_fl layout as documented above (names at 4 + name_off, lowercase, "assets/" stripped,
 * hdr_off = local header offset, size = uncompressed size, namelen; the data delta (30 + local name + extra length) is read on open). */
static int fl_load(void) {
    if (g_fl) return 1;
    if (g_fl_tried) return 0;
    g_fl_tried = 1;
    SceUID fd = sceIoOpen(OBB_PATH, SCE_O_RDONLY, 0);
    if (fd < 0) { l_warn("asset_manager: cannot open %s (0x%08x)", OBB_PATH, fd); return 0; }
    int64_t fsz = sceIoLseek(fd, 0, SCE_SEEK_END);
    uint8_t eocd[22];
    sceIoLseek(fd, fsz - 22, SCE_SEEK_SET);                       /* no archive comment in the OBB */
    if (sceIoRead(fd, eocd, 22) != 22 || *(uint32_t *)eocd != 0x06054b50) { l_error("asset_manager: %s has no zip end record", OBB_PATH); sceIoClose(fd); return 0; }
    uint32_t n = *(uint16_t *)(eocd + 10), cd_size = *(uint32_t *)(eocd + 12), cd_off = *(uint32_t *)(eocd + 16);
    uint8_t *cd = malloc(cd_size);
    if (!cd) { sceIoClose(fd); return 0; }
    sceIoLseek(fd, cd_off, SCE_SEEK_SET);
    if (sceIoRead(fd, cd, cd_size) != (int)cd_size) { l_error("asset_manager: cannot read zip directory"); free(cd); sceIoClose(fd); return 0; }
    uint8_t *tbl = malloc(4 + n * sizeof(fl_entry) + cd_size);   /* names are never longer than their central records */
    if (!tbl) { free(cd); sceIoClose(fd); return 0; }
    fl_entry *e = (fl_entry *)(tbl + 4);
    uint32_t names = n * sizeof(fl_entry), cnt = 0;
    uint8_t *p = cd;
    for (uint32_t i = 0; i < n && p + 46 <= cd + cd_size; i++) {
        uint32_t usize = *(uint32_t *)(p + 24), hoff = *(uint32_t *)(p + 42);
        uint16_t nl = *(uint16_t *)(p + 28), xl = *(uint16_t *)(p + 30), cl = *(uint16_t *)(p + 32);
        const char *nm = (const char *)(p + 46);
        p += 46u + nl + xl + cl;
        if (nl < 8 || strncmp(nm, "assets/", 7) != 0 || nm[nl - 1] == '/') continue;
        uint16_t len = nl - 7;
        for (uint16_t k = 0; k < len; k++) tbl[4 + names + k] = (uint8_t)tolower((unsigned char)nm[7 + k]);
        tbl[4 + names + len] = 0;
        e[cnt].ptr = 0; e[cnt].hdr_off = hoff; e[cnt].size = usize;
        e[cnt].namelen_delta = len;           /* data delta is read from the local header when the entry is opened */ e[cnt].name_off = names;
        names += len + 1u; cnt++;
    }
    free(cd); sceIoClose(fd);
    *(uint32_t *)tbl = cnt;
    g_fl = tbl; g_fl_count = cnt;
    l_info("asset_manager: built the asset table from the OBB zip directory: %u entries", cnt);
    return 1;
}

static const fl_entry *fl_find(const char *lname) {
    if (!fl_load()) return NULL;
    size_t l = strlen(lname);
    const fl_entry *e = (const fl_entry *)(g_fl + 4);
    /* linear scan is fine: ~3600 entries, lookups are rare (BGM start) */
    for (uint32_t i = 0; i < g_fl_count; i++) {
        if ((e[i].namelen_delta & 0xffff) == l && memcmp(g_fl + 4 + e[i].name_off, lname, l) == 0)
            return &e[i];
    }
    return NULL;
}

/* ------------------------------------------------------------------------------------------ */

/* absolute offset of the (stored) data of an OBB entry: local header offset + 30 + local name len + local extra len */
static int64_t fl_data_off(SceUID fd, const fl_entry *e) {
    uint8_t lh[30];
    sceIoLseek(fd, e->hdr_off, SCE_SEEK_SET);
    if (sceIoRead(fd, lh, 30) != 30 || *(uint32_t *)lh != 0x04034b50) return -1;
    return (int64_t)e->hdr_off + 30 + *(uint16_t *)(lh + 26) + *(uint16_t *)(lh + 28);
}

static AAsset *asset_new(SceUID fd, int64_t base, int64_t size) {
    fake_asset *a = calloc(1, sizeof(*a));
    a->fd = fd; a->base = base; a->size = size; a->pos = 0;
    sceIoLseek(fd, base, SCE_SEEK_SET);
    return (AAsset *)a;
}

AAsset *AAssetManager_open(AAssetManager *mgr, const char *filename, int mode) {
    (void)mgr; (void)mode;
    if (!filename) return NULL;

    /* 1. OBB entry: "<...>.obb/<name>" */
    const char *obb = strstr(filename, ".obb/");
    const char *rel = obb ? obb + 5 : NULL;
    char lname[256];
    if (rel) {
        size_t i = 0;
        for (; rel[i] && i < sizeof(lname) - 1; i++) lname[i] = tolower((unsigned char)rel[i]);
        lname[i] = 0;
        const fl_entry *e = fl_find(lname);
        if (!e) { l_warn("AAssetManager_open(%s): not in filelist", lname); return NULL; }
        SceUID fd = sceIoOpen(OBB_PATH, SCE_O_RDONLY, 0);
        if (fd < 0) { l_error("AAssetManager_open(%s): cannot open %s (0x%08x)", lname, OBB_PATH, fd); return NULL; }
        int64_t off = fl_data_off(fd, e);
        if (off < 0) { sceIoClose(fd); return NULL; }
        l_debug("AAssetManager_open(obb:%s): off=%lld size=%u", lname, (long long)off, e->size);
        return asset_new(fd, off, e->size);
    }

    /* 2. plain file below ASSETS_PATH */
    char path[512];
    snprintf(path, sizeof(path), ASSETS_PATH "%s", filename);
    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd >= 0) {
        int64_t size = sceIoLseek(fd, 0, SCE_SEEK_END);
        l_debug("AAssetManager_open(%s): size=%lld", path, (long long)size);
        return asset_new(fd, 0, size);
    }

    /* 3. last resort: a lowercase name of the filelist (a path relative to the OBB's assets/) */
    size_t i = 0;
    for (; filename[i] && i < sizeof(lname) - 1; i++) lname[i] = tolower((unsigned char)filename[i]);
    lname[i] = 0;
    const fl_entry *e = fl_find(lname);
    if (e && (fd = sceIoOpen(OBB_PATH, SCE_O_RDONLY, 0)) >= 0) {
        int64_t off = fl_data_off(fd, e);
        if (off >= 0) return asset_new(fd, off, e->size);
        sceIoClose(fd);
    }

    l_debug("AAssetManager_open(%s): not found", filename);
    return NULL;
}

int AAsset_read(AAsset *asset, void *buf, size_t count) {
    fake_asset *a = (fake_asset *)asset;
    if (!a) return -1;
    int64_t left = a->size - a->pos;
    if (left <= 0) return 0;
    if ((int64_t)count > left) count = (size_t)left;
    int r = sceIoRead(a->fd, buf, count);
    if (r > 0) a->pos += r;
    return r;
}

int64_t AAsset_seek64(AAsset *asset, int64_t offset, int whence) {
    fake_asset *a = (fake_asset *)asset;
    if (!a) return -1;
    int64_t np;
    switch (whence) {
        case SCE_SEEK_SET: np = offset; break;
        case SCE_SEEK_CUR: np = a->pos + offset; break;
        case SCE_SEEK_END: np = a->size + offset; break;
        default: return -1;
    }
    if (np < 0 || np > a->size) return -1;
    a->pos = np;
    sceIoLseek(a->fd, a->base + np, SCE_SEEK_SET);
    return np;
}

off_t AAsset_seek(AAsset *asset, off_t offset, int whence) {
    return (off_t)AAsset_seek64(asset, offset, whence);
}

int64_t AAsset_getLength64(AAsset *asset) { return asset ? ((fake_asset *)asset)->size : 0; }
off_t AAsset_getLength(AAsset *asset) { return (off_t)AAsset_getLength64(asset); }

int64_t AAsset_getRemainingLength64(AAsset *asset) {
    fake_asset *a = (fake_asset *)asset;
    return a ? a->size - a->pos : 0;
}
off_t AAsset_getRemainingLength(AAsset *asset) { return (off_t)AAsset_getRemainingLength64(asset); }

void AAsset_close(AAsset *asset) {
    fake_asset *a = (fake_asset *)asset;
    if (!a) return;
    sceIoClose(a->fd);
    free(a);
}

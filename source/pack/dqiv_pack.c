/* dqiv_pack.c - reference loader + validator for the DQIV pack. Plain C99 + libc (fopen/fseek/fread). */
#include "dqiv_pack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* compile-time layout checks (C99-compatible) */
#define DQIV_SIZE_CHECK(T, N) typedef char dqiv_size_check_##T[(sizeof(T) == (N)) ? 1 : -1]
DQIV_SIZE_CHECK(DqvIndexHeader, 128);
DQIV_SIZE_CHECK(DqvObject, 24);
DQIV_SIZE_CHECK(DqvResEntry, 16);
DQIV_SIZE_CHECK(DqvTexture, 48);
DQIV_SIZE_CHECK(DqvMesh, 96);
DQIV_SIZE_CHECK(DqvSubmesh, 48);
DQIV_SIZE_CHECK(DqvSkinVertex, 20);
DQIV_SIZE_CHECK(DqvMaterial, 64);
DQIV_SIZE_CHECK(DqvTexEnv, 24);
DQIV_SIZE_CHECK(DqvFloatProp, 8);
DQIV_SIZE_CHECK(DqvColorProp, 20);
DQIV_SIZE_CHECK(DqvShader, 48);
DQIV_SIZE_CHECK(DqvShaderPass, 40);
DQIV_SIZE_CHECK(DqvShaderProp, 12);
DQIV_SIZE_CHECK(DqvPrefab, 64);
DQIV_SIZE_CHECK(DqvNode, 80);
DQIV_SIZE_CHECK(DqvComp, 16);
DQIV_SIZE_CHECK(DqvSkinnedData, 48);
DQIV_SIZE_CHECK(DqvCameraData, 80);
DQIV_SIZE_CHECK(DqvProp, 24);
DQIV_SIZE_CHECK(DqvAnim, 64);
DQIV_SIZE_CHECK(DqvCurve, 32);
DQIV_SIZE_CHECK(DqvAnimEvent, 24);
DQIV_SIZE_CHECK(DqvText, 16);
DQIV_SIZE_CHECK(DqvFont, 96);
DQIV_SIZE_CHECK(DqvCharRect, 44);
DQIV_SIZE_CHECK(DqvKernPair, 8);

struct DqvPack {
    DqvIndexHeader hdr;
    uint8_t* index;                  /* whole index.bin */
    uint32_t index_size;
    const DqvObject* objs;
    const DqvResEntry* res;
    const char* strs;
    FILE* df[DQIV_DF_COUNT];
    char* dir;
};

static int is_le(void) { uint32_t x = 1; return *(uint8_t*)&x == 1; }

DqvPack* dqiv_open(const char* dir) {
    char path[1024];
    FILE* f;
    DqvPack* p;
    long sz;
    int i;
    if (!is_le()) return NULL;
    p = (DqvPack*)calloc(1, sizeof(*p));
    if (!p) return NULL;
    snprintf(path, sizeof path, "%s/index.bin", dir);
    f = fopen(path, "rb");
    if (!f) { free(p); return NULL; }
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < (long)sizeof(DqvIndexHeader)) { fclose(f); free(p); return NULL; }
    p->index = (uint8_t*)malloc((size_t)sz);
    if (!p->index || fread(p->index, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); dqiv_close(p); return NULL; }
    fclose(f);
    p->index_size = (uint32_t)sz;
    memcpy(&p->hdr, p->index, sizeof p->hdr);
    if (memcmp(p->hdr.magic, "DQIVP", 4) || p->hdr.version != 1 || p->hdr.data_file_count > DQIV_DF_COUNT ||
        (uint64_t)p->hdr.objs_off + (uint64_t)p->hdr.obj_count * sizeof(DqvObject) > p->index_size ||
        (uint64_t)p->hdr.res_off + (uint64_t)p->hdr.res_count * sizeof(DqvResEntry) > p->index_size ||
        (uint64_t)p->hdr.str_off + p->hdr.str_size > p->index_size || p->hdr.str_size == 0) {
        dqiv_close(p); return NULL;
    }
    p->objs = (const DqvObject*)(p->index + p->hdr.objs_off);
    p->res = (const DqvResEntry*)(p->index + p->hdr.res_off);
    p->strs = (const char*)(p->index + p->hdr.str_off);
    if (p->strs[p->hdr.str_size - 1] != 0) { dqiv_close(p); return NULL; }
    p->dir = (char*)malloc(strlen(dir) + 1);
    if (p->dir) strcpy(p->dir, dir);
    for (i = 0; i < (int)p->hdr.data_file_count; i++) {
        snprintf(path, sizeof path, "%s/%s", dir, dqiv_str(p, p->hdr.data_files[i].name_off));
        p->df[i] = fopen(path, "rb");
        if (!p->df[i]) { dqiv_close(p); return NULL; }
    }
    return p;
}

void dqiv_close(DqvPack* p) {
    int i;
    if (!p) return;
    for (i = 0; i < DQIV_DF_COUNT; i++) if (p->df[i]) fclose(p->df[i]);
    free(p->index); free(p->dir); free(p);
}

uint32_t dqiv_object_count(const DqvPack* p) { return p->hdr.obj_count; }
const DqvObject* dqiv_object(const DqvPack* p, uint32_t id) { return id < p->hdr.obj_count ? &p->objs[id] : NULL; }
uint32_t dqiv_res_count(const DqvPack* p) { return p->hdr.res_count; }
const DqvResEntry* dqiv_res_entry(const DqvPack* p, uint32_t i) { return i < p->hdr.res_count ? &p->res[i] : NULL; }
const char* dqiv_str(const DqvPack* p, uint32_t off) { return off < p->hdr.str_size ? p->strs + off : ""; }

static int lower_cmp(const char* stored, const char* q) {   /* stored is already lower-case; q folded on the fly */
    for (;; stored++, q++) {
        unsigned a = (unsigned char)*stored, b = (unsigned char)*q;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return a < b ? -1 : 1;
        if (!a) return 0;
    }
}

uint32_t dqiv_find_range(const DqvPack* p, const char* path, uint32_t* count) {
    uint32_t lo = 0, hi = p->hdr.res_count, first;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (lower_cmp(dqiv_str(p, p->res[mid].path_off), path) < 0) lo = mid + 1; else hi = mid;
    }
    first = lo;
    while (lo < p->hdr.res_count && lower_cmp(dqiv_str(p, p->res[lo].path_off), path) == 0) lo++;
    if (count) *count = lo - first;
    return first;
}

uint32_t dqiv_find(const DqvPack* p, const char* path, uint32_t kind_mask) {
    uint32_t n, i, first = dqiv_find_range(p, path, &n);
    for (i = 0; i < n; i++) {
        const DqvResEntry* e = &p->res[first + i];
        if (!kind_mask || (kind_mask & DQIV_KMASK(e->kind))) return e->obj_id;
    }
    return 0;
}

void dqiv_free(void* blob) { if (blob) free(((void**)blob)[-1]); }

static void* aligned_alloc16(size_t n) {
    void* raw = malloc(n + 16 + sizeof(void*));
    uintptr_t a;
    if (!raw) return NULL;
    a = ((uintptr_t)raw + sizeof(void*) + 15u) & ~(uintptr_t)15u;
    ((void**)a)[-1] = raw;
    return (void*)a;
}

int dqiv_read_part(DqvPack* p, uint32_t id, uint32_t off, uint32_t len, void* dst) {
    const DqvObject* o = dqiv_object(p, id);
    if (!o || !o->size || (uint64_t)off + len > o->size || !p->df[o->data_file]) return -1;
    if (fseek(p->df[o->data_file], (long)(o->offset + off), SEEK_SET)) return -1;
    return fread(dst, 1, len, p->df[o->data_file]) == len ? 0 : -1;
}

void* dqiv_load(DqvPack* p, uint32_t id, uint32_t* size_out) {
    const DqvObject* o = dqiv_object(p, id);
    void* b;
    if (!o || !o->size) return NULL;
    b = aligned_alloc16(o->size);
    if (!b) return NULL;
    if (dqiv_read_part(p, id, 0, o->size, b)) { dqiv_free(b); return NULL; }
    if (size_out) *size_out = o->size;
    return b;
}

#define AS_IMPL(name, T, MAGIC) \
    const T* name(const void* blob, uint32_t size) { \
        const T* h = (const T*)blob; \
        if (!blob || size < sizeof(T) || h->magic != (MAGIC) || h->size != size) return NULL; \
        return h; }
AS_IMPL(dqiv_as_texture, DqvTexture, DQIV_MAGIC_TEX)
AS_IMPL(dqiv_as_mesh, DqvMesh, DQIV_MAGIC_MESH)
AS_IMPL(dqiv_as_material, DqvMaterial, DQIV_MAGIC_MAT)
AS_IMPL(dqiv_as_shader, DqvShader, DQIV_MAGIC_SHADER)
AS_IMPL(dqiv_as_prefab, DqvPrefab, DQIV_MAGIC_PREFAB)
AS_IMPL(dqiv_as_anim, DqvAnim, DQIV_MAGIC_ANIM)
AS_IMPL(dqiv_as_text, DqvText, DQIV_MAGIC_TEXT)
AS_IMPL(dqiv_as_font, DqvFont, DQIV_MAGIC_FONT)

/* ----------------------------------------------------------------------------------------- validation */
typedef struct { const DqvPack* p; const uint8_t* b; uint32_t size; char* err; size_t errsz; int bad; } V;

static int vfail(V* v, const char* fmt, uint32_t a, uint32_t b) {
    if (!v->bad) { snprintf(v->err, v->errsz, fmt, a, b); v->bad = 1; }
    return 1;
}
#define FAIL(...) do { vfail(v, __VA_ARGS__); return 1; } while (0)

static int range_ok(V* v, uint32_t off, uint64_t len, uint32_t align, const char* what) {
    if (off % align || (uint64_t)off + len > v->size) {
        if (!v->bad) { snprintf(v->err, v->errsz, "%s range [%u,+%llu) invalid (size %u, align %u)", what, off,
                                (unsigned long long)len, v->size, align); v->bad = 1; }
        return 0;
    }
    return 1;
}
static int str_ok(V* v, uint32_t off, const char* what) {
    uint32_t i;
    if (!off) return 1;
    if (off >= v->size) { if (!v->bad) { snprintf(v->err, v->errsz, "%s string offset %u out of range", what, off); v->bad = 1; } return 0; }
    for (i = off; i < v->size; i++) if (!v->b[i]) return 1;
    if (!v->bad) { snprintf(v->err, v->errsz, "%s string at %u not terminated", what, off); v->bad = 1; }
    return 0;
}
static int ref_ok(V* v, uint32_t gid, uint32_t kind, const char* what) {
    const DqvObject* o;
    if (gid == 0 || (gid & DQIV_BUILTIN_BIT)) return 1;
    o = dqiv_object(v->p, gid);
    if (!o || o->kind != kind) {
        if (!v->bad) { snprintf(v->err, v->errsz, "%s ref %u invalid (kind %u, want %u)", what, gid, o ? o->kind : 0, kind); v->bad = 1; }
        return 0;
    }
    return 1;
}
#define R(T, off) ((const T*)(v->b + (off)))

static int val_tex(V* v) {
    const DqvTexture* t = R(DqvTexture, 0);
    static const uint8_t bpp[5] = {0, 1, 2, 2, 4};
    if (t->format < 1 || t->format > 4) FAIL("bad texture format %u%.0u", t->format, 0);
    if (!t->width || !t->height) {   /* Unity's 0x0 "Font Texture" placeholder of dynamic fonts */
        if (t->data_size) FAIL("zero-size texture with data%.0u%.0u", 0, 0);
        return 0;
    }
    if (t->data_size != (uint32_t)t->width * t->height * bpp[t->format]) FAIL("texture data_size %u != expected (%u)", t->data_size, (uint32_t)t->width * t->height * bpp[t->format]);
    if (!range_ok(v, t->data_off, t->data_size, 16, "texture data")) return 1;
    if (t->wrap_mode > 1 || t->filter_mode > 2) FAIL("bad wrap/filter %u/%u", t->wrap_mode, t->filter_mode);
    return 0;
}

static int val_mesh(V* v) {
    const DqvMesh* m = R(DqvMesh, 0);
    uint32_t i, j;
    if (m->vertex_stride % 4 || m->vertex_stride < 12) FAIL("bad vertex stride %u%.0u", m->vertex_stride, 0);
    if (!range_ok(v, m->vertex_off, (uint64_t)m->vertex_count * m->vertex_stride, 16, "vertices")) return 1;
    if (!range_ok(v, m->index_off, (uint64_t)m->index_count * 2, 16, "indices")) return 1;
    if (!range_ok(v, m->submesh_off, (uint64_t)m->submesh_count * sizeof(DqvSubmesh), 4, "submeshes")) return 1;
    if (m->off_pos != 0) FAIL("pos offset %u%.0u", (uint32_t)m->off_pos, 0);
    {
        int16_t o[6]; int sz[6] = {12, 12, 4, 8, 8, 16}; o[0] = m->off_pos; o[1] = m->off_nrm; o[2] = m->off_col;
        o[3] = m->off_uv0; o[4] = m->off_uv1; o[5] = m->off_tan;
        for (i = 0; i < 6; i++) if (o[i] >= 0 && (uint32_t)o[i] + sz[i] > m->vertex_stride) FAIL("attr %u offset %u beyond stride", i, (uint32_t)o[i]);
    }
    {
        const uint16_t* idx = R(uint16_t, m->index_off);
        const DqvSubmesh* s = R(DqvSubmesh, m->submesh_off);
        for (i = 0; i < m->submesh_count; i++) {
            if ((uint64_t)s[i].first_index + s[i].index_count > m->index_count) FAIL("submesh %u index range", i, 0);
            if (s[i].topology == DQIV_TOPO_TRIS && s[i].index_count % 3) FAIL("submesh %u tri count %u", i, s[i].index_count);
            if (s[i].topology == DQIV_TOPO_QUADS) FAIL("submesh %u quads present%.0u", i, 0);
            for (j = 0; j < s[i].index_count; j++) if (idx[s[i].first_index + j] >= m->vertex_count) FAIL("submesh %u index %u >= vertex_count", i, j);
        }
    }
    if (m->flags & DQIV_MF_SKIN) {
        const DqvSkinVertex* sk;
        if (!m->bone_count || !m->skin_off || !m->bindpose_off) FAIL("skin flag without data%.0u%.0u", 0, 0);
        if (!range_ok(v, m->bindpose_off, (uint64_t)m->bone_count * 64, 4, "bindposes")) return 1;
        if (!range_ok(v, m->skin_off, (uint64_t)m->vertex_count * sizeof(DqvSkinVertex), 4, "skin")) return 1;
        sk = R(DqvSkinVertex, m->skin_off);
        for (i = 0; i < m->vertex_count; i++) for (j = 0; j < 4; j++)
            if (sk[i].weight[j] > 0.0f && sk[i].bone[j] >= m->bone_count) FAIL("vertex %u bone index %u", i, sk[i].bone[j]);
    }
    return 0;
}

static int val_mat(V* v) {
    const DqvMaterial* m = R(DqvMaterial, 0);
    uint32_t i;
    if (!str_ok(v, m->shader_name_off, "shader name")) return 1;
    if (!ref_ok(v, m->shader_id, DQIV_K_SHADER, "shader")) return 1;
    if (!range_ok(v, m->tex_off, (uint64_t)m->tex_count * 24, 4, "texenvs")) return 1;
    if (!range_ok(v, m->float_off, (uint64_t)m->float_count * 8, 4, "floats")) return 1;
    if (!range_ok(v, m->color_off, (uint64_t)m->color_count * 20, 4, "colors")) return 1;
    if (!range_ok(v, m->kw_off, (uint64_t)m->kw_count * 4, 4, "keywords")) return 1;
    for (i = 0; i < m->tex_count; i++) {
        const DqvTexEnv* t = R(DqvTexEnv, m->tex_off) + i;
        if (!str_ok(v, t->name_off, "texenv name") || !ref_ok(v, t->tex_id, DQIV_K_TEXTURE, "texenv texture")) return 1;
    }
    for (i = 0; i < m->float_count; i++) if (!str_ok(v, (R(DqvFloatProp, m->float_off) + i)->name_off, "float name")) return 1;
    for (i = 0; i < m->color_count; i++) if (!str_ok(v, (R(DqvColorProp, m->color_off) + i)->name_off, "color name")) return 1;
    for (i = 0; i < m->kw_count; i++) if (!str_ok(v, R(uint32_t, m->kw_off)[i], "keyword")) return 1;
    if (!ref_ok(v, m->main_tex, DQIV_K_TEXTURE, "main_tex")) return 1;
    return 0;
}

static int val_shader(V* v) {
    const DqvShader* s = R(DqvShader, 0);
    uint32_t i;
    if (!str_ok(v, s->name_off, "name") || !str_ok(v, s->dump_name_off, "dump") || !str_ok(v, s->fallback_off, "fallback")) return 1;
    if (!range_ok(v, s->passes_off, (uint64_t)s->pass_count * 40, 4, "passes")) return 1;
    if (!range_ok(v, s->props_off, (uint64_t)s->prop_count * 12, 4, "props")) return 1;
    for (i = 0; i < s->prop_count; i++) {
        const DqvShaderProp* p = R(DqvShaderProp, s->props_off) + i;
        if (!str_ok(v, p->name_off, "prop") || !str_ok(v, p->type_off, "prop") || !str_ok(v, p->default_off, "prop")) return 1;
    }
    return 0;
}

static int val_props(V* v, uint32_t off, uint32_t size, uint32_t nnodes) {
    const DqvPropListData* pl;
    uint32_t i;
    if (size < 8 || !range_ok(v, off, size, 4, "proplist")) return 1;
    pl = R(DqvPropListData, off);
    if (!str_ok(v, pl->class_name_off, "class name")) return 1;
    if (8 + (uint64_t)pl->count * sizeof(DqvProp) > size) FAIL("proplist count %u exceeds payload (%u)", pl->count, size);
    for (i = 0; i < pl->count; i++) {
        const DqvProp* pr = R(DqvProp, off + 8) + i;
        if (!str_ok(v, pr->name_off, "prop name")) return 1;
        if (pr->type > DQIV_P_LOCAL) FAIL("prop type %u%.0u", pr->type, 0);
        if (pr->type == DQIV_P_STR && !str_ok(v, pr->v.u[0], "prop string")) return 1;
        if (pr->type == DQIV_P_LOCAL && pr->v.u[0] >= nnodes) FAIL("local ref node %u >= %u", pr->v.u[0], nnodes);
        if (pr->type == DQIV_P_ASSET) {
            uint32_t g = pr->v.u[0];
            if (g && !(g & DQIV_BUILTIN_BIT)) { const DqvObject* o = dqiv_object(v->p, g); if (!o || !o->kind) FAIL("prop asset ref %u invalid%.0u", g, 0); }
        }
    }
    return 0;
}

static int val_prefab(V* v) {
    const DqvPrefab* pf = R(DqvPrefab, 0);
    const DqvNode* n;
    const DqvComp* c;
    uint32_t i, j, roots = 0;
    if (!range_ok(v, pf->nodes_off, (uint64_t)pf->node_count * sizeof(DqvNode), 4, "nodes")) return 1;
    if (!range_ok(v, pf->comps_off, (uint64_t)pf->comp_count * sizeof(DqvComp), 4, "comps")) return 1;
    n = R(DqvNode, pf->nodes_off); c = R(DqvComp, pf->comps_off);
    for (i = 0; i < pf->node_count; i++) {
        if (!str_ok(v, n[i].name_off, "node name")) return 1;
        if (n[i].parent >= (int32_t)i) FAIL("node %u parent %u not before child", i, (uint32_t)n[i].parent);
        if (n[i].parent < 0) roots++;
        if (n[i].first_child >= (int32_t)pf->node_count || n[i].next_sibling >= (int32_t)pf->node_count) FAIL("node %u bad links%.0u", i, 0);
        if (n[i].first_child >= 0 && n[n[i].first_child].parent != (int32_t)i) FAIL("node %u first_child parent mismatch%.0u", i, 0);
        if ((uint64_t)n[i].comp_first + n[i].comp_count > pf->comp_count) FAIL("node %u comp range", i, 0);
    }
    if (roots != pf->root_count) FAIL("root_count %u != %u", pf->root_count, roots);
    for (i = 0; i < pf->comp_count; i++) {
        uint32_t off = c[i].data_off, sz = c[i].data_size;
        if (!range_ok(v, off, sz, 4, "comp payload")) return 1;
        switch (c[i].type) {
        case DQIV_C_MESHFILTER:
            if (sz < 4) FAIL("meshfilter payload %u%.0u", sz, 0);
            if (!ref_ok(v, R(DqvMeshFilterData, off)->mesh, DQIV_K_MESH, "meshfilter mesh")) return 1;
            break;
        case DQIV_C_MESHRENDERER: {
            const DqvMeshRendererData* d = R(DqvMeshRendererData, off);
            if (sz < 16 || sz < 16 + 4 * d->mat_count) FAIL("meshrenderer payload %u%.0u", sz, 0);
            for (j = 0; j < d->mat_count; j++) if (!ref_ok(v, R(uint32_t, off + 16)[j], DQIV_K_MATERIAL, "renderer material")) return 1;
            break; }
        case DQIV_C_SKINNEDMESHRENDERER: {
            const DqvSkinnedData* d = R(DqvSkinnedData, off);
            if (sz < 48 || sz < 48 + 4 * (uint64_t)(d->mat_count + d->bone_count)) FAIL("skinned payload %u%.0u", sz, 0);
            if (!ref_ok(v, d->mesh, DQIV_K_MESH, "skinned mesh")) return 1;
            if (d->root_bone >= (int32_t)pf->node_count) FAIL("skinned root bone %u%.0u", (uint32_t)d->root_bone, 0);
            for (j = 0; j < d->mat_count; j++) if (!ref_ok(v, R(uint32_t, off + 48)[j], DQIV_K_MATERIAL, "skinned material")) return 1;
            for (j = 0; j < d->bone_count; j++) if (R(int32_t, off + 48 + 4 * d->mat_count)[j] >= (int32_t)pf->node_count) FAIL("skinned bone %u node range%.0u", j, 0);
            break; }
        case DQIV_C_ANIMATION: {
            const DqvAnimationData* d = R(DqvAnimationData, off);
            if (sz < 20 || sz < 20 + 4 * (uint64_t)d->clip_count) FAIL("animation payload %u%.0u", sz, 0);
            if (!ref_ok(v, d->default_clip, DQIV_K_ANIM, "animation default clip")) return 1;
            for (j = 0; j < d->clip_count; j++) if (!ref_ok(v, R(uint32_t, off + 20)[j], DQIV_K_ANIM, "animation clip")) return 1;
            break; }
        case DQIV_C_CAMERA:
            if (sz != 80) FAIL("camera payload %u%.0u", sz, 0);
            if (!ref_ok(v, R(DqvCameraData, off)->target_texture, DQIV_K_TEXTURE, "camera target")) return 1;
            break;
        default:
            if (val_props(v, off, sz, pf->node_count)) return 1;
        }
    }
    return 0;
}

static int val_anim(V* v) {
    const DqvAnim* a = R(DqvAnim, 0);
    static const uint8_t nk[4] = {10, 13, 10, 4};
    uint32_t i, j;
    if (!range_ok(v, a->curves_off, (uint64_t)a->curve_count * 32, 4, "curves")) return 1;
    if (!range_ok(v, a->events_off, (uint64_t)a->event_count * 24, 4, "events")) return 1;
    for (i = 0; i < a->curve_count; i++) {
        const DqvCurve* c = R(DqvCurve, a->curves_off) + i;
        if (c->type > 3) FAIL("curve %u type %u", i, c->type);
        if (!str_ok(v, c->path_off, "curve path") || !str_ok(v, c->attr_off, "curve attr")) return 1;
        if (!range_ok(v, c->keys_off, (uint64_t)c->key_count * nk[c->type] * 4, 4, "keys")) return 1;
        for (j = 1; j < c->key_count; j++) {
            const float* k = R(float, c->keys_off);
            if (k[j * nk[c->type]] < k[(j - 1) * nk[c->type]]) FAIL("curve %u keys not sorted at %u", i, j);
        }
    }
    for (i = 0; i < a->event_count; i++) {
        const DqvAnimEvent* e = R(DqvAnimEvent, a->events_off) + i;
        if (!str_ok(v, e->func_off, "event func") || !str_ok(v, e->data_off, "event data")) return 1;
    }
    return 0;
}

static int val_font(V* v) {
    const DqvFont* f = R(DqvFont, 0);
    if (!range_ok(v, f->rects_off, (uint64_t)f->rect_count * 44, 4, "char rects")) return 1;
    if (!range_ok(v, f->kern_off, (uint64_t)f->kern_count * 8, 4, "kerning")) return 1;
    if (f->ttf_size && !range_ok(v, f->ttf_off, f->ttf_size, 16, "ttf")) return 1;
    if (!str_ok(v, f->font_name_off, "font name")) return 1;
    if (!ref_ok(v, f->texture_id, DQIV_K_TEXTURE, "font texture") || !ref_ok(v, f->material_id, DQIV_K_MATERIAL, "font material")) return 1;
    return 0;
}

int dqiv_validate(const DqvPack* p, uint32_t id, const void* blob, uint32_t size, char* err, size_t errsz) {
    V vv; V* v = &vv;
    const DqvObject* o = dqiv_object(p, id);
    uint32_t magic[] = {0, DQIV_MAGIC_TEX, DQIV_MAGIC_MESH, DQIV_MAGIC_MAT, DQIV_MAGIC_SHADER, DQIV_MAGIC_PREFAB,
                        DQIV_MAGIC_ANIM, DQIV_MAGIC_TEXT, DQIV_MAGIC_FONT};
    uint32_t minsz[] = {0, 48, 96, 64, 48, 64, 64, 16, 96};
    int r;
    vv.p = p; vv.b = (const uint8_t*)blob; vv.size = size; vv.err = err; vv.errsz = errsz; vv.bad = 0;
    err[0] = 0;
    if (!o || o->kind < 1 || o->kind > 8) return snprintf(err, errsz, "bad object %u", id), 1;
    if (size != o->size || size < minsz[o->kind]) return snprintf(err, errsz, "size mismatch %u vs %u", size, o->size), 1;
    if (((const uint32_t*)blob)[0] != magic[o->kind]) return snprintf(err, errsz, "bad magic %08x", ((const uint32_t*)blob)[0]), 1;
    if (((const uint32_t*)blob)[1] != size) return snprintf(err, errsz, "header size %u != %u", ((const uint32_t*)blob)[1], size), 1;
    switch (o->kind) {
    case DQIV_K_TEXTURE: r = val_tex(v); break;
    case DQIV_K_MESH: r = val_mesh(v); break;
    case DQIV_K_MATERIAL: r = val_mat(v); break;
    case DQIV_K_SHADER: r = val_shader(v); break;
    case DQIV_K_PREFAB: r = val_prefab(v); break;
    case DQIV_K_ANIM: r = val_anim(v); break;
    case DQIV_K_TEXT: r = (((const DqvText*)blob)->data_size + 16 > size) ? (snprintf(err, errsz, "text size"), 1) : 0; break;
    case DQIV_K_FONT: r = val_font(v); break;
    default: r = 1;
    }
    return r || v->bad;
}

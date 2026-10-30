#include "native/native_bridge.hpp"
#include "game/Layout.hpp"
#include "platform/cfg.hpp"
#include "native/field_patch.hpp"

#include <cstring>
#include "config.h"
#include "game/GetLangages.hpp"
#include "loader/loader.h"
#include "utils/logger.h"

namespace NativeCall {

bool (*DevInit)(const char*, const char*, int);       // DQ4: 3 arguments (no deviceInfo)
void (*GameInit)(int*, int*, float*, const char**);
void (*ResetNativePacket)(int*, int*, float*, const char**);
void (*GameUpdate)(int, int);
void (*MenuPortInit)(short, short*, short, short*, short);
void (*MenuPortResetNativePacket)(short*, short*);
void (*UnityRenderEvent)(int);
void (*FieldSet)(int, int, int*, float*);
void* (*FileReadAllBytes)(const char*);
void (*FreeFileMemory)(void*);
void (*ParticleSystemInit)(float*, int*, float*, float*, uint8_t*);
void (*ParticleSystemResetNativePacket)(float*, int*, float*, float*, uint8_t*);
int (*IsLoadNativeCode)();
void (*DebugSetMapName)(const char*);
void (*DebugSetValue)(int, unsigned, unsigned);
void (*DebugGetValue)(int, unsigned*);

static bool g_bound;
bool g_fieldPatched = false;

template <class F>
static bool bind(F& fn, const char* sym) {
    uintptr_t a = loader_symbol("dq4core", sym);
    if (!a) { l_error("libdq4core: symbol %s not found", sym); return false; }
    fn = reinterpret_cast<F>(a);
    return true;
}

bool Bind() {
    bool ok = true;
    ok &= bind(DevInit, "DevInit");
    ok &= bind(GameInit, "GameInit");
    ok &= bind(GameUpdate, "GameUpdate");
    ok &= bind(MenuPortInit, "MenuPortInit");
    ok &= bind(UnityRenderEvent, "UnityRenderEvent");
    ok &= bind(FileReadAllBytes, "FileReadAllBytes");
    ok &= bind(FreeFileMemory, "FreeFileMemory");
    ok &= bind(IsLoadNativeCode, "IsLoadNativeCode");
    // DQ5-only exports (absent from libdq4core, verified with readelf): the C# of DQ4 has no such P/Invoke. They stay null; every caller is guarded.
    //   ResetNativePacket / MenuPortResetNativePacket / ParticleSystemResetNativePacket (packet re-registration after a GC compaction: DQ4 never does it),
    //   ParticleSystemInit (no ParticleSystem class in DQ4), FieldSet (DQ4's Field.cs fills the tile UVs itself, TODO(dq4) Field port),
    //   Java_..._LoadFileList (libdq4core parses the OBB zip itself in InitFileLoader).
    ResetNativePacket = reinterpret_cast<decltype(ResetNativePacket)>(loader_symbol("dq4core", "ResetNativePacket"));
    MenuPortResetNativePacket = reinterpret_cast<decltype(MenuPortResetNativePacket)>(loader_symbol("dq4core", "MenuPortResetNativePacket"));
    FieldSet = reinterpret_cast<decltype(FieldSet)>(loader_symbol("dq4core", "FieldSet"));
    ParticleSystemInit = reinterpret_cast<decltype(ParticleSystemInit)>(loader_symbol("dq4core", "ParticleSystemInit"));
    ParticleSystemResetNativePacket = reinterpret_cast<decltype(ParticleSystemResetNativePacket)>(loader_symbol("dq4core", "ParticleSystemResetNativePacket"));
    if (!ResetNativePacket && !FieldSet && !ParticleSystemInit) l_info("libdq4core: DQ5-only exports (ResetNativePacket, FieldSet, ParticleSystem*, LoadFileList) are absent, as expected for DQ4");
    // optional debug hooks (not counted in `ok`)
    DebugSetMapName = reinterpret_cast<void (*)(const char*)>(loader_symbol("dq4core", "DebugSetMapName"));
    DebugSetValue = reinterpret_cast<void (*)(int, unsigned, unsigned)>(loader_symbol("dq4core", "DebugSetValue"));
    DebugGetValue = reinterpret_cast<void (*)(int, unsigned*)>(loader_symbol("dq4core", "DebugGetValue"));
    g_bound = ok;
    // TODO(dq4): field_patch (wider world-map tile block) is DQ5-only: fld::FieldData::drawBlock / FieldSet do not exist in libdq4core. PatchFieldBlock() refuses to patch and logs why.
    if (ok && Layout::Wide() && Cfg::GetInt("field_patch", 1)) g_fieldPatched = PatchFieldBlock();
    if (ok) l_success("libdq4core: all required entry points bound (DevInit/GameInit/GameUpdate/MenuPortInit/UnityRenderEvent/FileReadAllBytes/FreeFileMemory/IsLoadNativeCode)");
    return ok;
}

bool IsBound() { return g_bound; }

bool DeviceInit() {
    // NativeCall.DeviceInit(): DevInit(streamingAssetsPath-derived OBB path, persistentDataPath + "/", lang)   (DQ4: no deviceInfo argument)
    bool r = DevInit(OBB_PATH, SAVE_PATH, GetLangages::m_langagesNumber);
    l_info("DevInit(\"%s\", \"%s\", lang=%d) -> %d", OBB_PATH, SAVE_PATH, GetLangages::m_langagesNumber, (int)r);
    return r;
}

}  // namespace NativeCall

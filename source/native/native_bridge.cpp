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

bool (*DevInit)(const char*, const char*, int, int);
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
void (*LoadFileList_JNI)(void*, void*, void*);

int s_deviceInfo = DQIV_DEVICE_INFO;
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
    ok &= bind(ResetNativePacket, "ResetNativePacket");
    ok &= bind(GameUpdate, "GameUpdate");
    ok &= bind(MenuPortInit, "MenuPortInit");
    ok &= bind(MenuPortResetNativePacket, "MenuPortResetNativePacket");
    ok &= bind(UnityRenderEvent, "UnityRenderEvent");
    ok &= bind(FieldSet, "FieldSet");
    ok &= bind(FileReadAllBytes, "FileReadAllBytes");
    ok &= bind(FreeFileMemory, "FreeFileMemory");
    ok &= bind(ParticleSystemInit, "ParticleSystemInit");
    ok &= bind(ParticleSystemResetNativePacket, "ParticleSystemResetNativePacket");
    ok &= bind(IsLoadNativeCode, "IsLoadNativeCode");
    ok &= bind(LoadFileList_JNI, "Java_com_cattlecall_unityandroidaccessor_OverrideUnityPlayerNativeActivity_LoadFileList");
    // optional debug hooks (not counted in `ok`)
    DebugSetMapName = reinterpret_cast<void (*)(const char*)>(loader_symbol("dq4core", "DebugSetMapName"));
    DebugSetValue = reinterpret_cast<void (*)(int, unsigned, unsigned)>(loader_symbol("dq4core", "DebugSetValue"));
    DebugGetValue = reinterpret_cast<void (*)(int, unsigned*)>(loader_symbol("dq4core", "DebugGetValue"));
    g_bound = ok;
    if (ok && Layout::Wide() && Cfg::GetInt("field_patch", 1)) g_fieldPatched = PatchFieldBlock();     // landscape: wider world-map tile block (field_patch.cpp)
    if (ok) l_success("libdq4core: all %d entry points bound", 14);
    return ok;
}

bool IsBound() { return g_bound; }

bool LoadFileList() {
    // Boot.cs: OverrideUnityPlayerNativeActivity.loadFilelistFromAssets() -> LoadFileList(env, cls, AAssetManager).
    // The "asset manager" is any non-null token: AAssetManager_fromJava() ignores it (native/asset_manager.c).
    static char fake_asset_manager;
    LoadFileList_JNI(nullptr, nullptr, &fake_asset_manager);
    return true;
}

bool DeviceInit() {
    // NativeCall.DeviceInit(): DevInit(streamingAssetsPath-derived OBB path, persistentDataPath + "/", lang, deviceInfo)
    bool r = DevInit(OBB_PATH, SAVE_PATH, GetLangages::m_langagesNumber, s_deviceInfo);
    l_info("DevInit(\"%s\", \"%s\", lang=%d, devInfo=%d) -> %d", OBB_PATH, SAVE_PATH, GetLangages::m_langagesNumber, s_deviceInfo, (int)r);
    return r;
}

}  // namespace NativeCall

/*
 * native_bridge.hpp -- typed function pointers into libdq4core.so (WP3).
 *
 * Mirrors the C# `NativeCall` / `Neon` classes (DllImport "dq4core"). All entry points take only
 * ints/pointers, so they are called directly (no softfp wrapper needed). They must be called on the
 * game thread (the one that owns the GL context: UnityRenderEvent issues GL calls).
 *
 * Buffers handed to GameInit / MenuPortInit / ParticleSystemInit are RETAINED by the native side:
 * they must be static, never freed, never moved (docs 02 §A).
 */
#pragma once
#include <cstdint>

namespace NativeCall {

bool Bind();                       // resolve every symbol with so_symbol(); false if a required one is missing
bool IsBound();

// ---- libdq4core exports (NativeCall.cs, class Normal) ----
// ResetNativePacket, MenuPortResetNativePacket, FieldSet, ParticleSystemInit/ResetNativePacket are DQ5-only: they are NULL in this game, callers must test them.
extern bool (*DevInit)(const char* assetsPath, const char* savePath, int langagesNumber);
extern void (*GameInit)(int* input, int* output, float* output_float, const char** str);
extern void (*ResetNativePacket)(int* input, int* output, float* output_float, const char** str);
extern void (*GameUpdate)(int pad_press, int pad_toggle);
extern void (*MenuPortInit)(short portNum, short* unityBuffer, short unitySize, short* nativeBuffer, short nativeSize);
extern void (*MenuPortResetNativePacket)(short* unityBuffer, short* nativeBuffer);
extern void (*UnityRenderEvent)(int eventId);
extern void (*FieldSet)(int x_size, int y_size, int* buf, float* dst /* Vector2[] */);
extern void* (*FileReadAllBytes)(const char* fname);
extern void (*FreeFileMemory)(void* data);
extern void (*ParticleSystemInit)(float* posScale /*Vector3[2]*/, int* indices /*int[1176]*/, float* vertices /*Vector3[784]*/,
                                  float* uvs /*Vector2[784]*/, uint8_t* colors /*Color32[784]*/);
extern void (*ParticleSystemResetNativePacket)(float*, int*, float*, float*, uint8_t*);
extern int (*IsLoadNativeCode)();
// debug entry points of libdq4core (optional; used by game/BattleTest): DebugSetMapName("B-<btl map>" | "C-<chapter>"), DebugSetValue(kind, a, b) (kind 8: battle setup, 12: party)
extern void (*DebugSetMapName)(const char*);
extern void (*DebugSetValue)(int, unsigned, unsigned);
extern void (*DebugGetValue)(int, unsigned*);

// ---- derived, same names as NativeCall.cs ----
// Dev hooks that depend on DQ5 semantics of DebugSetValue ids / DQ5-only mangled symbols (Encount::brew, PartyStatusUtility::setNoDamageEnable, Global::startTown(int),
// Global::loadGame, FieldPlayerManager ...). NOT re-derived for libdq4core yet: WarpTest, BattleTest and the DevCheats patches are disabled (logged) while this is false.
// TODO(dq4): re-derive with tools/re/decomp.sh (see docs/TODO_DQ4.md), then flip.
constexpr bool kDevHooksVerified = true;      // harness branch: WarpTest/BattleTest/DevCheats use the DQ4 hooks decoded in game/WarpTest.hpp (each feature is verified in Vita3K, see README "Warp harness")
bool DeviceInit();                 // DevInit(OBB path, save path, lang)

}  // namespace NativeCall

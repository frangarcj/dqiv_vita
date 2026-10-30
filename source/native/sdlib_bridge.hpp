/*
 * sdlib_bridge.hpp -- typed bindings of libsdlib_android.so's SdSoundSystem_* API (WP3; used by game/Sound.cpp).
 * Mirrors the DllImport("sdlib_android") block of Sound.cs. All int/pointer entries are called directly; the ones with float
 * arguments/results (volume, pitch, panning) go through softfp-ABI function pointers (the .so is softfp, we are hardfp).
 */
#pragma once
namespace SdLib {

bool Bind();
bool IsBound();

extern int  (*Create)(const char* config);
extern int  (*Initialize)(const char* config);
extern void (*Release)();
extern int  (*Suspend)();
extern int  (*Resume)();
extern void (*StartTimerMSEC)(int intervalTimeMSec, int priority);
extern void (*StartTimerVSYNC)(int intervalTimeMSec, int priority);
extern int  (*AddData)(const void* scdBin);
extern int  (*RemoveData)(int bankID);
extern int  (*CreateSound)(int bankID, int soundIndex);
extern int  (*SoundCtrl_Start)(int soundID, int transTimeMSec);
extern void (*SoundCtrl_Stop)(int soundID, int transTimeMSec);
extern int  (*SoundCtrl_IsExist)(int soundID);
extern void (*SoundCtrl_SetPause)(int soundID, int pauseOn, int transTimeMSec);
extern int  (*SoundCtrl_IsPaused)(int soundID);
extern int  (*BankCtrl_IsLoop)(int bankID, int soundID);
extern int  (*AddStreamDataFromPath)(const char* akbFilePath);

// float variants (softfp trampolines)
void SoundCtrl_SetVolume(int soundID, float volume, int transTimeMSec);
float SoundCtrl_GetVolume(int soundID);
void SoundCtrl_SetPitch(int soundID, float pitch, int transTimeMSec);
void SoundCtrl_SetPanning(int soundID, float panning, int transTimeMSec);

}  // namespace SdLib

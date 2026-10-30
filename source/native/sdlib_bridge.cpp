#include "native/sdlib_bridge.hpp"
#include "loader/loader.h"
#include "loader/softfp_wrappers.hpp"
#include "utils/logger.h"

namespace SdLib {

int (*Create)(const char*);
int (*Initialize)(const char*);
void (*Release)();
int (*Suspend)();
int (*Resume)();
void (*StartTimerMSEC)(int, int);
void (*StartTimerVSYNC)(int, int);
int (*AddData)(const void*);
int (*RemoveData)(int);
int (*CreateSound)(int, int);
int (*SoundCtrl_Start)(int, int);
void (*SoundCtrl_Stop)(int, int);
int (*SoundCtrl_IsExist)(int);
void (*SoundCtrl_SetPause)(int, int, int);
int (*SoundCtrl_IsPaused)(int);
int (*BankCtrl_IsLoop)(int, int);
int (*AddStreamDataFromPath)(const char*);

static SOFTFP_FN(void, p_setVolume, int, float, int);
static SOFTFP_FN(float, p_getVolume, int);
static SOFTFP_FN(void, p_setPitch, int, float, int);
static SOFTFP_FN(void, p_setPanning, int, float, int);
static bool g_bound;

template <class F>
static bool bind(F& fn, const char* sym) {
  uintptr_t a = loader_symbol("sdlib_android", sym);
  if (!a) { l_error("libsdlib_android: symbol %s not found", sym); return false; }
  fn = reinterpret_cast<F>(a);
  return true;
}

bool Bind() {
  bool ok = true;
  ok &= bind(Create, "SdSoundSystem_Create");
  ok &= bind(Initialize, "SdSoundSystem_Initialize");
  ok &= bind(Release, "SdSoundSystem_Release");
  ok &= bind(Suspend, "SdSoundSystem_Suspend");
  ok &= bind(Resume, "SdSoundSystem_Resume");
  ok &= bind(StartTimerMSEC, "SdSoundSystem_StartTimerMSEC");
  ok &= bind(StartTimerVSYNC, "SdSoundSystem_StartTimerVSYNC");
  ok &= bind(AddData, "SdSoundSystem_AddData");
  ok &= bind(RemoveData, "SdSoundSystem_RemoveData");
  ok &= bind(CreateSound, "SdSoundSystem_CreateSound");
  ok &= bind(SoundCtrl_Start, "SdSoundSystem_SoundCtrl_Start");
  ok &= bind(SoundCtrl_Stop, "SdSoundSystem_SoundCtrl_Stop");
  ok &= bind(SoundCtrl_IsExist, "SdSoundSystem_SoundCtrl_IsExist");
  ok &= bind(SoundCtrl_SetPause, "SdSoundSystem_SoundCtrl_SetPause");
  ok &= bind(SoundCtrl_IsPaused, "SdSoundSystem_SoundCtrl_IsPaused");
  ok &= bind(BankCtrl_IsLoop, "SdSoundSystem_BankCtrl_IsLoop");
  ok &= bind(AddStreamDataFromPath, "SdSoundSystem_AddStreamDataFromPath");
  ok &= bind(p_setVolume, "SdSoundSystem_SoundCtrl_SetVolume");
  ok &= bind(p_getVolume, "SdSoundSystem_SoundCtrl_GetVolume");
  ok &= bind(p_setPitch, "SdSoundSystem_SoundCtrl_SetPitch");
  ok &= bind(p_setPanning, "SdSoundSystem_SoundCtrl_SetPanning");
  g_bound = ok;
  if (ok) l_success("libsdlib_android: SdSoundSystem_* bound");
  return ok;
}
bool IsBound() { return g_bound; }

void SoundCtrl_SetVolume(int id, float v, int t) { p_setVolume(id, v, t); }
float SoundCtrl_GetVolume(int id) { return p_getVolume(id); }
void SoundCtrl_SetPitch(int id, float v, int t) { p_setPitch(id, v, t); }
void SoundCtrl_SetPanning(int id, float v, int t) { p_setPanning(id, v, t); }

}  // namespace SdLib

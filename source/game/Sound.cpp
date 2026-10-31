// game/Sound.cpp -- OWNER: WP3. Port of Sound.cs (re/cs_out/Assembly-CSharp/Sound.cs), line for line; see Sound.hpp.
// Deviations: DllImport calls go through SdLib:: (native/sdlib_bridge.hpp, float arguments via softfp trampolines); NULL checks where C# would throw;
// StreamPlay/s_streamPath dropped (never called); the debug audio self test at the bottom is not in the C#.
#include "game/Sound.hpp"

#include <cstdio>
#include <cstring>

#include "config.h"
#include "native/native_bridge.hpp"
#include "native/opensles_shim.hpp"
#include "native/sdlib_bridge.hpp"
#include "platform/cfg.hpp"
#include "utils/logger.h"

float Sound::SoundControl::s_volume[1171];
void* Sound::SoundControl::s_seData = nullptr;
LinkList Sound::s_seList;
float Sound::s_seVolume = 0.6f, Sound::s_bgmVolume = 0.6f;
int Sound::s_state = 0, Sound::s_bgmNext = 0;

static bool m_isInitialized = false;
static Sound::SoundControl s_bgm1, s_bgm2;
static Sound::SoundControl* s_se[4];

static float VolOf(int no) { return (no >= 0 && no < 1171) ? Sound::SoundControl::s_volume[no] : 1.f; }

// ------------------------------------------------------------------------------------------------ SoundControl
void Sound::SoundControl::Stop() {
  if (m_soundID != 0) SdLib::SoundCtrl_Stop(m_soundID, 0);
  if (m_bankID != 0) SdLib::RemoveData(m_bankID);
  if (m_addr != nullptr) NativeCall::FreeFileMemory(m_addr);
  m_bankID = 0;
  m_soundID = 0;
  m_addr = nullptr;
  m_no = -1;
}

void Sound::SoundControl::Play(const char* fname, int no, float vol) {
  if (m_no != no) {
    Stop();
    if (no != 307) {
      char path[64];
      snprintf(path, sizeof(path), "sound/%s.akb", fname);
      void* p = NativeCall::FileReadAllBytes(path);
      if (p == nullptr) {                         // the C# returns silently too: the native asks for SE 0 ("no sound") and the OBB has no sound/SE0000.akb (SE files are 0300..1170, BGM01..)
        if (strncmp(fname, "SE", 2) == 0) l_info("Sound: %s not in the OBB (silent, as in the C#)", path); else l_warn("Sound: %s not found", path);
        return;
      }
      m_bankID = SdLib::AddData(p);
      m_addr = p;
    } else {
      m_bankID = SdLib::AddData(s_seData);
    }
  } else if (m_soundID != 0) {
    SdLib::SoundCtrl_Stop(m_soundID, 0);
  }
  int num = SdLib::CreateSound(m_bankID, no);
  if (num != 0) {
    SdLib::SoundCtrl_Start(num, 0);
    SdLib::SoundCtrl_SetVolume(num, vol * VolOf(no), 0);
    m_soundID = num;
    m_no = no;
  }
}

void Sound::SoundControl::SetVolume(float vol) {
  if (m_soundID != 0) SdLib::SoundCtrl_SetVolume(m_soundID, vol * VolOf(m_no), 0);
}

// ------------------------------------------------------------------------------------------------ Sound
static bool InitializePlugin() {
  if (m_isInitialized) return true;
  if (!SdLib::Bind()) return false;
  int num = SdLib::Create("");
  l_info("SdSoundSystem_Create -> %d", num);
  if (num < 0) return false;
  int intervalTimeMSec = 16;
  int priority = -1;
  SdLib::StartTimerMSEC(intervalTimeMSec, priority);
  SdLib::StartTimerVSYNC(intervalTimeMSec, priority);
  m_isInitialized = true;
  return true;
}

bool Sound::IsInitialized() { return m_isInitialized; }

// GetSEControl(): first free control from the top of the list, else the top (oldest) one
static Sound::SoundControl* GetSEControl() {
  LinkList::Node* top = Sound::s_seList.GetTop();
  for (LinkList::Node* n = top; n != nullptr && n != &Sound::s_seList.m_tail; n = n->m_after) {   // C#: `m_after as SoundControl` is null at the tail sentinel
    auto* c = static_cast<Sound::SoundControl*>(n);
    if (c->m_soundID == 0 || SdLib::SoundCtrl_IsExist(c->m_soundID) == 0) return c;
  }
  return static_cast<Sound::SoundControl*>(top);
}

void Sound::StopAll() {
  s_bgm1.Stop();
  s_bgm2.Stop();
  for (SoundControl* c : s_se) if (c) c->Stop();
}

void Sound::SystemPause(bool pause) {
  if (!m_isInitialized) return;
  if (pause) SdLib::Suspend(); else SdLib::Resume();
}

void Sound::SystemQuit() {
  StopAll();
  if (m_isInitialized) { SdLib::Release(); m_isInitialized = false; }
}

void Sound::SetVolumeBGM(int vol) {
  s_bgmVolume = (float)(vol + 1) * 0.2f;
  if (m_isInitialized) s_bgm1.SetVolume(s_bgmVolume);
}

void Sound::SetVolumeSE(int vol) {
  s_seVolume = (float)(vol + 1) * 0.2f;
  if (!m_isInitialized) return;
  for (SoundControl* c : s_se) if (c) c->SetVolume(s_seVolume);
}

void Sound::SystemInit() {
  InitializePlugin();
  for (int i = 0; i < 4; i++) {
    s_se[i] = new SoundControl();
    s_seList.SetTail(s_se[i]);
  }
  // SoundControl.s_streamPath is only used by StreamPlay (never called)
  SoundControl::s_seData = NativeCall::FileReadAllBytes("sound/SE0307.akb");
  if (!SoundControl::s_seData) l_warn("Sound: sound/SE0307.akb not found (SE 307 will be silent)");
  void* p = NativeCall::FileReadAllBytes("sound/snd_vol.dat");
  if (p) {
    memcpy(SoundControl::s_volume, p, sizeof(SoundControl::s_volume));
    NativeCall::FreeFileMemory(p);
    l_info("Sound: snd_vol.dat loaded (vol[0..2] = %.3f %.3f %.3f)", SoundControl::s_volume[0], SoundControl::s_volume[1], SoundControl::s_volume[2]);
  } else {
    l_error("Sound: sound/snd_vol.dat not found: all sounds play at 0 volume");
  }
  if (Cfg::GetInt("audiotest", 0)) AudioTestStart();
}

void Sound::PlayBGM(int no) {
  if (!m_isInitialized) return;
  if (no >= 4096) {
    no -= 4096;
    s_bgmNext = s_bgm1.m_no;
    s_state = STATE_CHANGE;
  }
  char name[16];
  snprintf(name, sizeof(name), "BGM%02d", no);
  l_info("Sound.PlayBGM(%s) bgmVolume=%.2f state=%d next=%d", name, s_bgmVolume, s_state, s_bgmNext);
  s_bgm1.Play(name, no, s_bgmVolume);
}

void Sound::StopBGM(int fade) {
  (void)fade;
  if (!m_isInitialized) return;
  l_info("Sound.StopBGM");
  s_bgm1.Stop();
}

void Sound::PlaySE(int no) {
  if (!m_isInitialized) return;
  SoundControl* c = GetSEControl();
  s_seList.SetTail(c);
  char name[16];
  snprintf(name, sizeof(name), "SE%04d", no);
  c->Play(name, no, s_seVolume);
}

void Sound::StopSE(int no) {
  if (!m_isInitialized) return;
  for (SoundControl* c : s_se) {
    if (c && c->m_no == no && SdLib::SoundCtrl_IsExist(c->m_soundID) != 0) {
      c->Stop();
      s_seList.SetTop(c);
    }
  }
}

void Sound::SystemUpdate() {
  if (!m_isInitialized) return;
  int num = s_state;
  if (num == STATE_CHANGE && SdLib::SoundCtrl_IsExist(s_bgm1.m_soundID) == 0) {
    PlayBGM(s_bgmNext);
    s_state = STATE_NONE;
  }
}

// ------------------------------------------------------------------------------------------------ debug audio self test (cfg audiotest=1)
static int s_testFrame = -1;

void Sound::AudioTestStart() {
  l_info("audiotest: BGM01 now, SE0101 after 60 frames; sdlib levels are logged by the audio thread every 2 s");
  SetVolumeBGM(4);       // what MenuPort cmd 5 does with the default 127: 127/25 - 1 = 4  -> 1.0
  SetVolumeSE(4);
  PlayBGM(1);
  s_testFrame = 0;
}

void Sound::AudioTestUpdate() {
  if (s_testFrame < 0) return;
  s_testFrame++;
  if (s_testFrame == 60) PlaySE(101);
  if (s_testFrame % 150 == 0) {
    AudioOut::Stats s = AudioOut::GetStats();
    l_info("audiotest f%d: cb=%u grains=%u underruns=%u peakEver=%d port=%s %d Hz %d ch", s_testFrame, s.callbacks, s.grains, s.underruns, s.peakEver, s.open ? "open" : "CLOSED", s.rate,
           s.channels);
  }
}

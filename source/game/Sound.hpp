// game/Sound.hpp -- OWNER: WP3.  Port of Sound.cs (re/cs_out/Assembly-CSharp/Sound.cs) on libsdlib_android (native/sdlib_bridge.hpp).
// BGM = `sound/BGMnn.akb`, SE = `sound/SEnnnn.akb`, both read whole into RAM through libdq4core's FileReadAllBytes (the C# StreamPlay path is unused).
// Volumes: per-sound factor from sound/snd_vol.dat (1179 floats) * s_bgmVolume / s_seVolume ((vol+1)*0.2, vol from MenuPort port-0 cmds 4/5: param/25-1).
// Contract for the others: NativeManager calls PlaySE/StopSE/PlayBGM/StopBGM/SystemUpdate; MenuPortSystem port-0 handler calls SetVolumeSE/SetVolumeBGM(int);
// MainLoop calls SystemPause/SystemQuit.
#pragma once
#include "game/LinkList.hpp"
#include "miniunity.hpp"
using namespace miniunity;

class Sound {
public:
  class SoundControl : public LinkList::Node {
  public:
    static float s_volume[1179];
    static void* s_seData;
    void* m_addr = nullptr;
    int m_bankID = 0, m_soundID = 0, m_no = -1;
    void Stop();
    void Play(const char* fname, int no, float vol);
    void SetVolume(float vol);
    void Pause(int pauseOn);
  };
  static constexpr int BGM_STATE_NONE = 0, BGM_STATE_RESTART = 1, BGM_STATE_CHANGE = 2;
  static LinkList s_seList;
  static float s_seVolume, s_bgmVolume;
  static int s_bgmState, s_bgmLayer, s_bgmParam;

  static void SystemInit();
  static void SystemUpdate();
  static void SystemPause(bool pause);
  static void SystemQuit();
  static void StopAll();
  static void PlaySE(int no);
  static void StopSE(int no);
  static void PlayBGM(int no);
  static void StopBGM(int fade);
  static void SetVolumeSE(int vol);
  static void SetVolumeBGM(int vol);
  static bool IsInitialized();

  // debug (not in the C#): ux0:data/dqiv/dqivn<slot>.cfg  audiotest=1  plays BGM + SE at boot; LogAudio() prints sdlib/OpenSL stats (peak levels).
  static void AudioTestStart();
  static void AudioTestUpdate();
};

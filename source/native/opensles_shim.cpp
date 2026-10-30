// native/opensles_shim.cpp -- OWNER: WP3. See opensles_shim.hpp.
#include "native/opensles_shim.hpp"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>

#include "utils/logger.h"

namespace {

constexpr int GRAIN = 512;           // frames per sceAudioOutOutput block (== libsdlib's mix block)
constexpr int MAXQ = 8;
constexpr int AUDIO_THREAD_STACK = 256 * 1024;

struct Core;
struct Itf { const void* vt; Core* core; };   // an OpenSL "interface": first word = vtable pointer, so &itf.vt is the SLxxxItf handle

struct Core {
  int kind = 0;                      // 1 engine, 2 output mix, 3 audio player
  Itf obj{}, engine{}, play{}, bq{}, vol{};
  // ---- player ----
  int rate = 32000, channels = 2;
  volatile uint32_t state = SL_PLAYSTATE_STOPPED;
  struct Buf { const uint8_t* p; uint32_t n; } q[MAXQ];
  int qhead = 0, qcount = 0; uint32_t qindex = 0;
  slAndroidSimpleBufferQueueCallback cb = nullptr; void* cbCtx = nullptr;
  SLmillibel level = 0; SLboolean mute = SL_BOOLEAN_FALSE;
  SceUID thread = -1; volatile bool quit = false;
  int port = -1;
  SceKernelLwMutexWork mtx;
};

SLObjectItf_ g_objVt; SLEngineItf_ g_engVt; SLPlayItf_ g_playVt; SLAndroidSimpleBufferQueueItf_ g_bqVt; SLVolumeItf_ g_volVt;
bool g_vtReady;

std::atomic<unsigned> s_callbacks{0}, s_grains{0}, s_underruns{0};
std::atomic<int> s_peak{0}, s_peakEver{0}, s_rate{0}, s_channels{0};
std::atomic<bool> s_open{false};

inline Core* CoreOf(const void* self) { return reinterpret_cast<const Itf*>(self)->core; }
inline Core* CoreOfH(const void* const* handle) { return reinterpret_cast<const Itf*>(handle)->core; }

SLresult Unsupported(...) { return SL_RESULT_FEATURE_UNSUPPORTED; }
SLresult Ok(...) { return SL_RESULT_SUCCESS; }

template <class T, class F> void FillAll(T& vt, F fn) {
  void** p = reinterpret_cast<void**>(&vt);
  for (size_t i = 0; i < sizeof(T) / sizeof(void*); i++) p[i] = reinterpret_cast<void*>(fn);
}

void ApplyVolume(Core* c) {
  if (c->port < 0) return;
  float lin = c->mute ? 0.f : std::pow(10.f, (float)c->level / 2000.f);
  if (lin > 1.f) lin = 1.f;
  int v = (int)(lin * (float)SCE_AUDIO_OUT_MAX_VOL);
  int vols[2] = { v, v };
  sceAudioOutSetVolume(c->port, (SceAudioOutChannelFlag)(SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH), vols);
}

// ---------------------------------------------------------------------------------------------- the audio thread
int AudioThread(SceSize, void* argp) {
  Core* c = *reinterpret_cast<Core**>(argp);
  c->port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, GRAIN, c->rate, c->channels == 1 ? SCE_AUDIO_OUT_MODE_MONO : SCE_AUDIO_OUT_MODE_STEREO);
  if (c->port < 0) {
    l_error("audio: sceAudioOutOpenPort(BGM, %d, %d Hz, %d ch) failed: 0x%08x", GRAIN, c->rate, c->channels, c->port);
    return 0;
  }
  ApplyVolume(c);
  s_rate = c->rate; s_channels = c->channels; s_open = true;
  l_success("audio: sceAudioOut BGM port %d open, %d Hz, %d ch, grain %d frames (%.1f ms), thread stack %d KiB", c->port, c->rate, c->channels, GRAIN,
            1000.f * GRAIN / c->rate, AUDIO_THREAD_STACK / 1024);

  alignas(64) static int16_t chunk[GRAIN * 2];
  alignas(64) static int16_t silence[GRAIN * 2];
  memset(silence, 0, sizeof(silence));
  const int ch = c->channels == 1 ? 1 : 2;
  unsigned winCb = 0, winGr = 0, winUnder = 0; int winPeak = 0;
  uint64_t t0 = sceKernelGetProcessTimeWide();

  while (!c->quit) {
    if (c->state != SL_PLAYSTATE_PLAYING) { sceKernelDelayThread(4000); continue; }
    const uint8_t* data = nullptr; uint32_t size = 0;
    sceKernelLockLwMutex(&c->mtx, 1, nullptr);
    if (c->qcount > 0) { data = c->q[c->qhead].p; size = c->q[c->qhead].n; }
    sceKernelUnlockLwMutex(&c->mtx, 1);

    if (!data) {                      // underrun: keep the clock running with silence (queue is refilled by the callback of the previous buffer)
      sceAudioOutOutput(c->port, silence);
      s_underruns++; s_grains++; winUnder++; winGr++;
      continue;
    }
    const uint32_t frames = size / (uint32_t)(ch * 2);
    const int16_t* src = reinterpret_cast<const int16_t*>(data);
    for (uint32_t off = 0; off < frames && !c->quit; off += GRAIN) {
      uint32_t n = frames - off < (uint32_t)GRAIN ? frames - off : (uint32_t)GRAIN;
      memcpy(chunk, src + (size_t)off * ch, (size_t)n * ch * 2);
      if (n < (uint32_t)GRAIN) memset(chunk + (size_t)n * ch, 0, (size_t)(GRAIN - n) * ch * 2);
      int pk = 0;
      for (int i = 0; i < GRAIN * ch; i++) { int v = chunk[i]; if (v < 0) v = -v; if (v > pk) pk = v; }
      if (pk > winPeak) winPeak = pk;
      if (pk > s_peak.load()) s_peak = pk;
      if (pk > s_peakEver.load()) s_peakEver = pk;
      sceAudioOutOutput(c->port, chunk);   // blocks until the previous block was consumed: this is the 16 ms clock
      s_grains++; winGr++;
    }
    sceKernelLockLwMutex(&c->mtx, 1, nullptr);
    if (c->qcount > 0) { c->qhead = (c->qhead + 1) % MAXQ; c->qcount--; c->qindex++; }
    slAndroidSimpleBufferQueueCallback cb = c->cb; void* ctx = c->cbCtx;
    sceKernelUnlockLwMutex(&c->mtx, 1);
    s_callbacks++; winCb++;
    if (cb) cb(reinterpret_cast<SLAndroidSimpleBufferQueueItf>(&c->bq.vt), ctx);   // libsdlib mixes the next block and Enqueue()s it (re-entrant, same thread)

    uint64_t now = sceKernelGetProcessTimeWide();
    if (now - t0 >= 2000000ull) {
      float secs = (float)(now - t0) / 1e6f;
      l_info("audio: %.1f callbacks/s, %.1f grains/s, peak %d, underruns %u (queued %d)", (float)winCb / secs, (float)winGr / secs, winPeak, winUnder, c->qcount);
      t0 = now; winCb = winGr = winUnder = 0; winPeak = 0;
    }
  }
  sceAudioOutReleasePort(c->port);
  c->port = -1; s_open = false;
  return 0;
}

void StartThread(Core* c) {
  if (c->thread >= 0) return;
  c->quit = false;
  c->thread = sceKernelCreateThread("dqivn_audio", AudioThread, 0x40, AUDIO_THREAD_STACK, 0, 0, nullptr);
  if (c->thread < 0) { l_error("audio: cannot create the audio thread: 0x%08x", c->thread); return; }
  Core* arg = c;
  sceKernelStartThread(c->thread, sizeof(arg), &arg);
}

// ---------------------------------------------------------------------------------------------- SLObjectItf
SLresult Obj_GetState(SLObjectItf, SLuint32* st) { if (st) *st = SL_OBJECT_STATE_REALIZED; return SL_RESULT_SUCCESS; }

SLresult Obj_GetInterface(SLObjectItf self, const SLInterfaceID iid, void* out) {
  Core* c = CoreOfH(reinterpret_cast<const void* const*>(self));
  void** o = reinterpret_cast<void**>(out);
  if (iid == SL_IID_OBJECT) { *o = &c->obj.vt; return SL_RESULT_SUCCESS; }
  if (c->kind == 1 && iid == SL_IID_ENGINE) { *o = &c->engine.vt; return SL_RESULT_SUCCESS; }
  if (c->kind == 3) {
    if (iid == SL_IID_PLAY) { *o = &c->play.vt; return SL_RESULT_SUCCESS; }
    if (iid == SL_IID_ANDROIDSIMPLEBUFFERQUEUE) { *o = &c->bq.vt; return SL_RESULT_SUCCESS; }
    if (iid == SL_IID_VOLUME) { *o = &c->vol.vt; return SL_RESULT_SUCCESS; }
  }
  *o = nullptr;
  return SL_RESULT_FEATURE_UNSUPPORTED;
}

void Obj_Destroy(SLObjectItf self) {
  Core* c = CoreOfH(reinterpret_cast<const void* const*>(self));
  if (c->kind == 3 && c->thread >= 0) {
    c->quit = true;
    sceKernelWaitThreadEnd(c->thread, nullptr, nullptr);
    sceKernelDeleteThread(c->thread);
    c->thread = -1;
    sceKernelDeleteLwMutex(&c->mtx);
  }
  delete c;
}

Core* NewCore(int kind) {
  Core* c = new Core();
  c->kind = kind;
  c->obj = { &g_objVt, c }; c->engine = { &g_engVt, c }; c->play = { &g_playVt, c }; c->bq = { &g_bqVt, c }; c->vol = { &g_volVt, c };
  return c;
}

// ---------------------------------------------------------------------------------------------- SLEngineItf
SLresult Eng_CreateOutputMix(SLEngineItf, SLObjectItf* pMix, SLuint32, const SLInterfaceID*, const SLboolean*) {
  Core* c = NewCore(2);
  *pMix = reinterpret_cast<SLObjectItf>(&c->obj.vt);
  return SL_RESULT_SUCCESS;
}

SLresult Eng_CreateAudioPlayer(SLEngineItf, SLObjectItf* pPlayer, SLDataSource* src, SLDataSink*, SLuint32, const SLInterfaceID*, const SLboolean*) {
  Core* c = NewCore(3);
  if (src && src->pFormat) {
    const SLDataFormat_PCM* f = reinterpret_cast<const SLDataFormat_PCM*>(src->pFormat);
    if (f->formatType == SL_DATAFORMAT_PCM) {
      c->channels = (int)f->numChannels;
      c->rate = (int)(f->samplesPerSec / 1000);     // milliHertz
      if (f->bitsPerSample != 16) l_warn("audio: player format has %u bits per sample (only 16 supported)", (unsigned)f->bitsPerSample);
    }
  }
  sceKernelCreateLwMutex(&c->mtx, "dqivn_audio_q", 0, 0, nullptr);
  l_info("audio: CreateAudioPlayer PCM %d Hz %d ch", c->rate, c->channels);
  *pPlayer = reinterpret_cast<SLObjectItf>(&c->obj.vt);
  return SL_RESULT_SUCCESS;
}

// ---------------------------------------------------------------------------------------------- SLPlayItf
SLresult Play_SetPlayState(SLPlayItf self, SLuint32 state) {
  Core* c = CoreOfH(reinterpret_cast<const void* const*>(self));
  c->state = state;
  l_info("audio: SetPlayState(%u)", (unsigned)state);
  if (state == SL_PLAYSTATE_PLAYING) StartThread(c);
  return SL_RESULT_SUCCESS;
}
SLresult Play_GetPlayState(SLPlayItf self, SLuint32* st) { *st = CoreOfH(reinterpret_cast<const void* const*>(self))->state; return SL_RESULT_SUCCESS; }

// ---------------------------------------------------------------------------------------------- SLAndroidSimpleBufferQueueItf
SLresult Bq_Enqueue(SLAndroidSimpleBufferQueueItf self, const void* buf, SLuint32 size) {
  Core* c = CoreOfH(reinterpret_cast<const void* const*>(self));
  sceKernelLockLwMutex(&c->mtx, 1, nullptr);
  SLresult r = SL_RESULT_SUCCESS;
  if (c->qcount >= MAXQ) r = SL_RESULT_BUFFER_INSUFFICIENT;
  else { int t = (c->qhead + c->qcount) % MAXQ; c->q[t] = { reinterpret_cast<const uint8_t*>(buf), size }; c->qcount++; }
  sceKernelUnlockLwMutex(&c->mtx, 1);
  return r;
}
SLresult Bq_Clear(SLAndroidSimpleBufferQueueItf self) {
  Core* c = CoreOfH(reinterpret_cast<const void* const*>(self));
  sceKernelLockLwMutex(&c->mtx, 1, nullptr);
  c->qcount = 0;
  sceKernelUnlockLwMutex(&c->mtx, 1);
  return SL_RESULT_SUCCESS;
}
SLresult Bq_GetState(SLAndroidSimpleBufferQueueItf self, SLAndroidSimpleBufferQueueState* st) {
  Core* c = CoreOfH(reinterpret_cast<const void* const*>(self));
  st->count = (SLuint32)c->qcount; st->index = c->qindex;
  return SL_RESULT_SUCCESS;
}
SLresult Bq_RegisterCallback(SLAndroidSimpleBufferQueueItf self, slAndroidSimpleBufferQueueCallback cb, void* ctx) {
  Core* c = CoreOfH(reinterpret_cast<const void* const*>(self));
  c->cb = cb; c->cbCtx = ctx;
  return SL_RESULT_SUCCESS;
}

// ---------------------------------------------------------------------------------------------- SLVolumeItf
SLresult Vol_SetVolumeLevel(SLVolumeItf self, SLmillibel lv) { Core* c = CoreOfH(reinterpret_cast<const void* const*>(self)); c->level = lv; ApplyVolume(c); return SL_RESULT_SUCCESS; }
SLresult Vol_GetVolumeLevel(SLVolumeItf self, SLmillibel* lv) { *lv = CoreOfH(reinterpret_cast<const void* const*>(self))->level; return SL_RESULT_SUCCESS; }
SLresult Vol_GetMaxVolumeLevel(SLVolumeItf, SLmillibel* lv) { *lv = 0; return SL_RESULT_SUCCESS; }
SLresult Vol_SetMute(SLVolumeItf self, SLboolean m) { Core* c = CoreOfH(reinterpret_cast<const void* const*>(self)); c->mute = m; ApplyVolume(c); return SL_RESULT_SUCCESS; }
SLresult Vol_GetMute(SLVolumeItf self, SLboolean* m) { *m = CoreOfH(reinterpret_cast<const void* const*>(self))->mute; return SL_RESULT_SUCCESS; }

void InitVtables() {
  if (g_vtReady) return;
  FillAll(g_objVt, (void*)Ok); FillAll(g_engVt, (void*)Unsupported); FillAll(g_playVt, (void*)Ok); FillAll(g_bqVt, (void*)Unsupported); FillAll(g_volVt, (void*)Ok);
  g_objVt.GetState = Obj_GetState; g_objVt.GetInterface = Obj_GetInterface; g_objVt.Destroy = Obj_Destroy;
  g_engVt.CreateOutputMix = Eng_CreateOutputMix; g_engVt.CreateAudioPlayer = Eng_CreateAudioPlayer;
  g_playVt.SetPlayState = Play_SetPlayState; g_playVt.GetPlayState = Play_GetPlayState;
  g_bqVt.Enqueue = Bq_Enqueue; g_bqVt.Clear = Bq_Clear; g_bqVt.GetState = Bq_GetState; g_bqVt.RegisterCallback = Bq_RegisterCallback;
  g_volVt.SetVolumeLevel = Vol_SetVolumeLevel; g_volVt.GetVolumeLevel = Vol_GetVolumeLevel; g_volVt.GetMaxVolumeLevel = Vol_GetMaxVolumeLevel;
  g_volVt.SetMute = Vol_SetMute; g_volVt.GetMute = Vol_GetMute;
  g_vtReady = true;
}

}  // namespace

extern "C" SLresult dqiv_slCreateEngine(SLObjectItf* pEngine, SLuint32, const SLEngineOption*, SLuint32, const SLInterfaceID*, const SLboolean*) {
  InitVtables();
  Core* c = NewCore(1);
  *pEngine = reinterpret_cast<SLObjectItf>(&c->obj.vt);
  l_info("audio: slCreateEngine -> OpenSL shim over sceAudioOut");
  return SL_RESULT_SUCCESS;
}

namespace AudioOut {
Stats GetStats(bool resetPeak) {
  Stats s;
  s.callbacks = s_callbacks; s.grains = s_grains; s.underruns = s_underruns; s.peak = s_peak; s.peakEver = s_peakEver;
  s.rate = s_rate; s.channels = s_channels; s.open = s_open;
  if (resetPeak) s_peak = 0;
  return s;
}
}  // namespace AudioOut

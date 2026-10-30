/*
 * opensles_shim.hpp -- WP3. A tiny OpenSL ES (Android flavour) implementation on top of sceAudioOut, used instead of the SDK's libOpenSLES.
 *
 * libsdlib_android.so (REPORT_RUNTIME §D) only needs: slCreateEngine -> Engine -> OutputMix + AudioPlayer(ANDROIDSIMPLEBUFFERQUEUE, PCM s16 32 kHz
 * stereo) -> Play / BufferQueue(Enqueue, RegisterCallback) / Volume. CoreAudioOutCallback (the whole mixer, including the Ogg Vorbis decoder) runs inside
 * the buffer-queue callback. The SDK's OpenSLES runs that callback on its own audio thread with a 64 KiB stack (risk: Vorbis decode); this shim runs it
 * on a thread with a 256 KiB stack, paces itself on the blocking sceAudioOutOutput (512 frames @ 32 kHz = 16 ms grain, the engine's mix block) and
 * counts callbacks/peak level so a headless emulator run can prove that audio flows (log line "audio:" once per 2 s).
 *
 * Registered under the original import names in loader/dynlib_table.inc (slCreateEngine -> dqiv_slCreateEngine; SL_IID_* keep the SDK ids).
 */
#pragma once
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

extern "C" SLresult dqiv_slCreateEngine(SLObjectItf* pEngine, SLuint32 numOptions, const SLEngineOption* pEngineOptions, SLuint32 numInterfaces,
                                       const SLInterfaceID* pInterfaceIds, const SLboolean* pInterfaceRequired);

namespace AudioOut {
struct Stats {
  unsigned callbacks = 0;     // buffer-queue callbacks run (libsdlib mixed + re-enqueued one block)
  unsigned grains = 0;        // 512-frame blocks handed to sceAudioOutOutput
  unsigned underruns = 0;     // blocks of silence inserted because the queue was empty
  int      peak = 0;          // peak |sample| since the last GetStats(reset=true)
  int      peakEver = 0;
  int      rate = 0, channels = 0;
  bool     open = false;
};
Stats GetStats(bool resetPeak = false);
}  // namespace AudioOut

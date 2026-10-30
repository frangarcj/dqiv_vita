/*
 * packet_recorder.hpp -- trace of everything libdq4core exchanges with the host, one file per frame (WP3, docs 05 §3 item 6).
 *
 * Enabled when the file  ux0:data/dqiv/trace<slot>.on  exists.  Writes  ux0:data/dqiv/trace<slot>/NNNN.pkt  for the first MAX_FRAMES frames.
 * The idea: UI / scene packages can be developed and regression-tested with `--replay` (feeds the recorded frames into
 * NativeManager::Update without libdq4core; TODO) on the Vita or a host build.
 *
 * .pkt layout (all little endian):
 *   u32 magic 'DQPK'   u32 version(1)   u32 frame   u32 pad_press   u32 pad_toggle   u32 flag_in (out[0] written before GameUpdate)
 *   u32 n_in           then n_in  x i32  m_input  (the host->native stream incl. the -1 terminator)
 *   u32 n_out          then n_out x i32  m_output (native->host, [0] = flag, ..., -1 terminator)
 *   u32 n_float        then n_float x f32 m_output_float (used part: found with a sentinel pre-fill)
 *   u32 str_len        then str_len bytes: m_string[0] (NUL terminated), 0 if null
 *   u32 n_menu_unity   then n_menu_unity  x i16  MenuPort host->native buffer (whole 129)
 *   u32 n_menu_native  then n_menu_native x i16  MenuPort native->host buffer (up to the write cursor buf[64])
 */
#pragma once
#include <cstdint>

namespace PacketRecorder {

bool Init();                   // checks trace.on; creates the trace dir
bool Enabled();
constexpr int32_t SENTINEL_I = 0x7EADBEEF;
constexpr uint32_t SENTINEL_F_BITS = 0x7FC0DEAD;     // a NaN pattern
void PrepareFrame(int32_t* out, int nOut, float* outF, int nOutF);     // sentinel pre-fill (only when enabled)

struct FrameView {
  int frame, padPress, padToggle, flagIn;
  const int32_t* in; int nIn;
  const int32_t* out; int nOut;
  const float* outF; int nOutF;
  const char* str0;
  const int16_t* menuUnity; int nMenuUnity;
  const int16_t* menuNative; int nMenuNative;
};
void Record(const FrameView& f);
int UsedInts(const int32_t* out, int cap);       // index of the last non-sentinel + 1  (valid only after PrepareFrame)
int UsedFloats(const float* outF, int cap);

}  // namespace PacketRecorder

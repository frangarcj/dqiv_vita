/*
 * packet_replay.hpp -- WP3. Feeds recorded frames (packet_recorder .pkt files) into NativeManager::Update instead of calling libdq4core's GameUpdate, so the
 * presentation packages (scenes, UI, effects) can be exercised and regression-tested from a trace without the game logic driving them.
 *
 * Enable: create  ux0:data/dqiv/replay<slot>.on  (optional first line `dir=<path>` to read another directory than ux0:data/dqiv/trace<slot>/; `quit=1` quits after the last frame).
 * The libraries are still loaded and DevInit/GameInit still run (the boot is unchanged); only the per-frame GameUpdate call is replaced:
 * out[]/outF[]/m_string[0] and the MenuPort native->host mailbox come from NNNN.pkt (N = NativeManager frame counter). After the last file the stream is empty.
 * Offline equivalent: tools/pktcheck.py (same operand walk, no emulator needed).
 */
#pragma once
namespace PacketReplay {
bool Init();                       // reads replay.on
bool Active();
bool QuitAtEnd();
// Copies frame `frame` into the arrays (out: 8192 ints, outF: 8192 floats, str0: strCap bytes, menuNative: 1089 shorts (DQ4)). False if there is no such frame (end of trace).
bool LoadFrame(int frame, int* out, float* outF, char* str0, int strCap, short* menuNative);
int  FramesPlayed();
}

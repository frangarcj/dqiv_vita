// game/SoakStats.hpp -- OWNER: WP4 (dev aid).  One log line per call with everything a long-session memory soak needs (see DEVELOPERS.md "Soak test"):
//   SOAK f=<frame> t=<s> | frame ms avg/max | heap used/peak/arena MB (newlib mallinfo: includes libdq4core/sdlib mallocs) | vitaGL free/total MB per pool |
//   live GameObjects/components/materials/meshes/textures (+ resident GL bytes, CPU mesh bytes) | open fds | open MenuPorts
// Tick() is called every frame (cheap: samples the heap peak every 30 frames); Log() every 300 frames from GameLoop.
#pragma once
#include "platform/vita_platform.hpp"
namespace SoakStats {
void Tick(unsigned frame);
void Log(unsigned frame, const vita_platform::PaceStats& ps);
}

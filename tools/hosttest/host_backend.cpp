// Host stand-in for engine/backend_vitagl.cpp: no GL, textures are just ids. Used by tools/hosttest/run.sh.
#include "engine/backend.hpp"
#include <cstdio>
namespace miniunity { namespace backend {
void SetBlitRect(int, int, int, int) {}
FrameStats& Stats() { static FrameStats s; return s; }
static unsigned g_next = 1; static int g_live;
bool Init(int, int, int, int, bool) { return true; }
void Shutdown() {}
void BindTarget(RenderTexture*) {}
void BeginCamera(int, int, int, int, bool, bool, Color, const Matrix4x4&, const Matrix4x4&) {}
void Draw(const DrawCall&) {}
void EndFrameAndPresent() {}
unsigned CreateTexture(int, int, int, const void*) { g_live++; return g_next++; }
void UpdateTexture(unsigned, int, int, int, const void*) {}
void DeleteTexture(unsigned) { g_live--; }
void SetTextureSampler(unsigned, int, int) {}
bool CreateRenderTexture(int, int, unsigned* fbo, unsigned* col, unsigned* dep) { *fbo = g_next++; *col = g_next++; *dep = g_next++; return true; }
void DestroyRenderTexture(unsigned, unsigned, unsigned) {}
int HostLiveTextures() { return g_live; }
}}

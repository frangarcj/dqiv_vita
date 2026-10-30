// engine/assets_gl.cpp -- Texture2D / RenderTexture GL-side glue (WP1 data + WP2 upload).
#include "miniunity.hpp"
#include "engine/backend.hpp"
#include "pack/dqiv_pack.h"
#include "utils/logger.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace miniunity {

static int PackFormatOf(TextureFormat f) {
  switch (f) {
    case TextureFormat::Alpha8: return DQIV_TF_A8;
    case TextureFormat::RGB565: return DQIV_TF_RGB565;
    case TextureFormat::RGBA4444: return DQIV_TF_RGBA4444;
    default: return DQIV_TF_RGBA8888;     // RGB24 would be expanded to RGBA32 by SetPixels32 users
  }
}

static std::unordered_map<const Texture2D*, std::vector<Color32>> g_cpuPixels;
Texture2D::Texture2D(int w, int h, TextureFormat f, bool /*mipmap*/) : format(f) {
  width = w; height = h;
  packFormat = PackFormatOf(f);
  origFormat = (int)f;
  glId = backend::CreateTexture(w, h, packFormat, nullptr);        // storage allocated (the native text canvas glTexSubImage2D()s into it)
  ApplySampler();
}
Texture2D::~Texture2D() { g_cpuPixels.erase(this); if (glId) backend::DeleteTexture(glId); }
// SetPixels32/Apply (minimaps 256x256 / 1024x1024, test textures): the CPU copy lives in a side table until Apply() uploads it in the texture's GL format.
void Texture2D::SetPixels32(const Color32* px, int n) {
  std::vector<Color32>& v = g_cpuPixels[this];
  size_t want = (size_t)width * height;
  v.assign(px, px + std::min<size_t>(n, want)); v.resize(want, Color32(0, 0, 0, 0));
}
void Texture2D::Apply(bool) {
  auto it = g_cpuPixels.find(this);
  if (it == g_cpuPixels.end() || !glId) return;
  const std::vector<Color32>& v = it->second; size_t n = v.size();
  std::vector<uint8_t> buf;
  switch (packFormat) {
    case DQIV_TF_A8:       buf.resize(n); for (size_t i = 0; i < n; i++) buf[i] = v[i].a; break;
    case DQIV_TF_RGB565:   buf.resize(n * 2); for (size_t i = 0; i < n; i++) { uint16_t p = (uint16_t)(((v[i].r >> 3) << 11) | ((v[i].g >> 2) << 5) | (v[i].b >> 3)); memcpy(&buf[i * 2], &p, 2); } break;
    case DQIV_TF_RGBA4444: buf.resize(n * 2); for (size_t i = 0; i < n; i++) { uint16_t p = (uint16_t)(((v[i].r >> 4) << 12) | ((v[i].g >> 4) << 8) | ((v[i].b >> 4) << 4) | (v[i].a >> 4)); memcpy(&buf[i * 2], &p, 2); } break;
    default:              buf.resize(n * 4); for (size_t i = 0; i < n; i++) { buf[i * 4] = v[i].r; buf[i * 4 + 1] = v[i].g; buf[i * 4 + 2] = v[i].b; buf[i * 4 + 3] = v[i].a; } break;
  }
  backend::UpdateTexture(glId, width, height, packFormat, buf.data());
}
void Texture2D::ApplySampler() {
  bool npot = (width & (width - 1)) || (height & (height - 1));
  backend::SetTextureSampler(glId, filterMode == FilterMode::Point ? 0 : 1, (npot || wrapMode == TextureWrapMode::Clamp) ? 1 : 0);   // GXM linear textures: clamp only
}
Texture2D* Texture2D::FromPack(const char* name, int w, int h, int pf, int wrap, int filter, int origFmt, const void* pixels) {
  Texture2D* t = new Texture2D();
  t->name = name; t->width = w; t->height = h; t->packFormat = pf; t->origFormat = origFmt;
  t->format = pf == DQIV_TF_A8 ? TextureFormat::Alpha8 : pf == DQIV_TF_RGB565 ? TextureFormat::RGB565 : pf == DQIV_TF_RGBA4444 ? TextureFormat::RGBA4444 : TextureFormat::RGBA32;
  t->wrapMode = wrap ? TextureWrapMode::Clamp : TextureWrapMode::Repeat;
  t->filterMode = filter == 0 ? FilterMode::Point : filter == 1 ? FilterMode::Bilinear : FilterMode::Trilinear;
  t->glId = backend::CreateTexture(w, h, pf, pixels);
  t->ApplySampler();
  return t;
}

RenderTexture::RenderTexture(int w, int h, int /*depthBits*/) {
  width = w; height = h;
  backend::CreateRenderTexture(w, h, &fbo, &colorTex, &depthRb);
}
RenderTexture::~RenderTexture() { backend::DestroyRenderTexture(fbo, colorTex, depthRb); }

}  // namespace miniunity

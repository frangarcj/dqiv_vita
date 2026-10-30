// engine/camera.cpp -- Camera, the render loop (queues/sorting/culling), Graphics.Blit, Engine::Init/Render/Present (WP2).
// Rendering model: docs 03 §2.3/2.4/4 and 01 §6.
//   per camera (ascending depth, ties: creation order): viewport = `rect` of the target (scissor), clear per clearFlags
//   (Skybox/Color -> colour+depth, Depth -> depth only, Nothing), cull by layer mask, sort by render queue (opaque < 2500 front-to-back,
//   transparent back-to-front by view depth of the bounds centre), draw every pass of every material (submesh i -> material i).
#include "miniunity.hpp"
#include "engine/backend.hpp"
#include "engine/engine_internal.hpp"
#include "engine/textmesh.hpp"
#include "config.h"
#include "utils/logger.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace miniunity {

// ------------------------------------------------------------------------------------------------ small matrix helpers
static bool Inv4(const Matrix4x4& a, Matrix4x4& out) {
  const float* m = a.m; float inv[16];
  inv[0]  =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
  inv[4]  = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
  inv[8]  =  m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
  inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
  inv[1]  = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
  inv[5]  =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
  inv[9]  = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
  inv[13] =  m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
  inv[2]  =  m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
  inv[6]  = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
  inv[10] =  m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
  inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
  inv[3]  = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
  inv[7]  =  m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
  inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
  inv[15] =  m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];
  float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
  if (std::fabs(det) < 1e-20f) return false;
  det = 1.f / det;
  for (int i = 0; i < 16; i++) out.m[i] = inv[i] * det;
  return true;
}

// ------------------------------------------------------------------------------------------------ Camera
static Camera* g_main;
float Camera::aspect() const {
  float w = (targetTexture ? targetTexture->width : Screen::width) * rect.width, h = (targetTexture ? targetTexture->height : Screen::height) * rect.height;
  return h > 0 ? w / h : 1.f;
}
void Camera::setProjectionMatrix(const Matrix4x4& m) { customProjection = m; hasCustomProjection = true; }
Matrix4x4 Camera::projectionMatrix() {
  if (hasCustomProjection) return customProjection;
  float a = aspect();
  if (orthographic) { float h = orthographicSize, w = h * a; return Matrix4x4::Ortho(-w, w, -h, h, nearClipPlane, farClipPlane); }
  return Matrix4x4::Perspective(fieldOfView, a, nearClipPlane, farClipPlane);
}
Rect Camera::pixelRect() const {
  float tw = targetTexture ? (float)targetTexture->width : (float)Screen::width, th = targetTexture ? (float)targetTexture->height : (float)Screen::height;
  return Rect(rect.x * tw, rect.y * th, rect.width * tw, rect.height * th);
}
void Camera::setPixelRect(Rect r) {
  float tw = targetTexture ? (float)targetTexture->width : (float)Screen::width, th = targetTexture ? (float)targetTexture->height : (float)Screen::height;
  rect = Rect(r.x / tw, r.y / th, r.width / tw, r.height / th);
}
Camera* Camera::main() {
  if (g_main && engine::IsLive(g_main) && g_main->enabled && g_main->gameObject->activeInHierarchy()) return g_main;
  for (GameObject* g : engine::g_gameObjects)
    if (g->tag == "MainCamera" && g->activeInHierarchy())
      if (Camera* c = g->camera()) if (c->enabled) return g_main = c;
  return nullptr;
}
// View matrix as the GL backend wants it: Unity's camera looks along +Z, GL's along -Z => flip Z after world->camera (03 §2.3).
static Matrix4x4 ViewMatrix(const Camera* c) {
  Transform* t = c->transform();
  Quaternion inv = Quaternion::Inverse(t->rotation());
  Matrix4x4 w2c = Matrix4x4::TRS(Vector3() - (inv * t->position()), inv, Vector3::one);
  Matrix4x4 flip = Matrix4x4::identity(); flip.m[10] = -1.f;
  return flip * w2c;
}
// Unity: ray from the near plane through the pixel (screen coordinates: pixels of the target, origin bottom-left; z ignored).
Ray Camera::ScreenPointToRay(Vector3 p) const {
  Camera* self = const_cast<Camera*>(this);
  Rect pr = pixelRect();
  float nx = (p.x - pr.x) / pr.width * 2.f - 1.f, ny = (p.y - pr.y) / pr.height * 2.f - 1.f;
  Matrix4x4 vp = self->projectionMatrix() * ViewMatrix(this), ivp;
  Ray r;
  if (!Inv4(vp, ivp)) { r.origin = transform()->position(); r.direction = transform()->rotation() * Vector3::forward; return r; }
  Vector3 a = ivp.MultiplyPoint(Vector3(nx, ny, -1.f)), b = ivp.MultiplyPoint(Vector3(nx, ny, 1.f));
  r.origin = a; r.direction = (b - a).normalized();
  return r;
}
Vector3 Camera::WorldToScreenPoint(Vector3 w) const {
  Camera* self = const_cast<Camera*>(this);
  Matrix4x4 view = ViewMatrix(this);
  Vector3 v = view.MultiplyPoint(w);                                     // GL view space: camera looks along -Z
  Vector3 n = (self->projectionMatrix()).MultiplyPoint(v);
  Rect pr = pixelRect();
  return Vector3(pr.x + (n.x * 0.5f + 0.5f) * pr.width, pr.y + (n.y * 0.5f + 0.5f) * pr.height, -v.z);   // z = distance in front of the camera (world units)
}
Vector3 Camera::WorldToViewportPoint(Vector3 w) const { Vector3 s = WorldToScreenPoint(w); Rect pr = pixelRect(); return Vector3((s.x - pr.x) / pr.width, (s.y - pr.y) / pr.height, s.z); }
Vector3 Camera::ScreenToWorldPoint(Vector3 p) const {
  Camera* self = const_cast<Camera*>(this);
  Rect pr = pixelRect();
  float nx = (p.x - pr.x) / pr.width * 2.f - 1.f, ny = (p.y - pr.y) / pr.height * 2.f - 1.f;
  Matrix4x4 proj = self->projectionMatrix(), vp = proj * ViewMatrix(this), ivp;
  if (!Inv4(vp, ivp)) return transform()->position();
  // NDC depth of the plane at view distance p.z (GL view space looks down -Z)
  float zc = proj.m[2] * 0 + proj.m[10] * -p.z + proj.m[14], wc = proj.m[11] * -p.z + proj.m[15];
  float nz = wc != 0.f ? zc / wc : 0.f;
  return ivp.MultiplyPoint(Vector3(nx, ny, nz));
}

// ------------------------------------------------------------------------------------------------ render loop
namespace engine {

struct Item { Renderer* r; Mesh* mesh; int sub; Material* mat; const backend::ShaderDesc* sd; int queue; float depth; int mi; Texture* fontTex; };
struct Renderable { GameObject* g; MeshRenderer* r; MeshFilter* mf; TextMesh* tm; Matrix4x4 model; };

static std::vector<Renderable> s_renderables;
static RenderTexture* g_imageSrc;                  // OnRenderImage source: the camera renders straight into it (g_forceTarget) when one of its components has an OnRenderImage
static RenderTexture* g_forceTarget;
static bool g_imageFilled;
static std::vector<Matrix4x4> s_models;

static void GatherRenderables() {
  s_renderables.clear();
  for (GameObject* g : g_gameObjects) {
    if (g->destroyed || !g->activeInHierarchy()) continue;
    MeshRenderer* r = nullptr; MeshFilter* mf = nullptr; TextMesh* tm = nullptr;
    for (Component* c : g->components) {
      if (c->destroyed) continue;
      switch (c->RenderKind()) {
        case 2: if (!r) r = static_cast<MeshRenderer*>(static_cast<Renderer*>(c)); break;
        case 1: if (!mf) mf = static_cast<MeshFilter*>(c); break;
        case 3: if (!tm) tm = static_cast<TextMesh*>(c); break;
        default: break;
      }
    }
    if (!r || !r->enabled) continue;
    if (!(tm || mf)) continue;
    s_renderables.push_back(Renderable{ g, r, mf, tm, Matrix4x4() });
  }
}

static Vector3 MeshCenter(const Mesh* m) {
  size_t n = m->vertices.size();
  if (n == 0) return Vector3();
  if (n > 4096) return m->bounds.center;
  Vector3 mn = m->vertices[0], mx = mn;
  for (const Vector3& v : m->vertices) {
    mn.x = std::min(mn.x, v.x); mn.y = std::min(mn.y, v.y); mn.z = std::min(mn.z, v.z);
    mx.x = std::max(mx.x, v.x); mx.y = std::max(mx.y, v.y); mx.z = std::max(mx.z, v.z);
  }
  return (mn + mx) * 0.5f;
}

static std::vector<Color32> s_colScratch;

static void RenderCamera(Camera* cam) {
  RenderTexture* rt = cam->targetTexture ? cam->targetTexture : g_forceTarget;
  backend::BindTarget(rt);
  Rect pr = cam->pixelRect();
  int x = (int)std::lround(pr.x), y = (int)std::lround(pr.y), w = (int)std::lround(pr.width), h = (int)std::lround(pr.height);
  if (w <= 0 || h <= 0) return;
  bool clearColor = cam->clearFlags == CameraClearFlags::Skybox || cam->clearFlags == CameraClearFlags::Color;
  bool clearDepth = cam->clearFlags != CameraClearFlags::Nothing;
  Matrix4x4 view = ViewMatrix(cam), proj = cam->projectionMatrix();
  backend::BeginCamera(x, y, w, h, clearColor, clearDepth, cam->backgroundColor, view, proj);
  backend::Stats().camPasses++; backend::Stats().renderables += (int)s_renderables.size();

  Transform* ct = cam->transform();
  Vector3 cpos = ct->position(), cfwd = ct->rotation() * Vector3::forward;
  unsigned mask = (unsigned)cam->cullingMask;

  static std::vector<Item> items; items.clear();            // scratch: no per-frame allocation
  s_models.clear(); s_models.reserve(s_renderables.size());
  for (Renderable& rb : s_renderables) {
    GameObject* g = rb.g;
    if (!((mask >> (unsigned)(g->layer & 31)) & 1u)) continue;
    Mesh* mesh = rb.tm ? TextMeshGetMesh(rb.tm) : (rb.mf ? rb.mf->sharedMesh : nullptr);
    if (!mesh || mesh->vertices.empty()) continue;
    s_models.push_back(g->transform->localToWorldMatrix());
    int mi = (int)s_models.size() - 1;
    Vector3 c = s_models.back().MultiplyPoint(MeshCenter(mesh));
    float depth = Vector3::Dot(c - cpos, cfwd);
    Texture* fontTex = (rb.tm && rb.tm->font && rb.tm->font->material) ? rb.tm->font->material->mainTexture : (rb.tm && rb.tm->font ? (Texture*)rb.tm->font->texture : nullptr);
    int nsub = std::max(1, mesh->subMeshCount());
    for (int sub = 0; sub < nsub; sub++) {
      Material* mat = sub < (int)rb.r->mats.size() ? rb.r->mats[sub] : nullptr;     // extra submeshes without a material are not rendered
      if (!mat || !mat->shader) continue;
      const backend::ShaderDesc* sd = mat->shader->id >= 0 ? backend::ShaderAt(mat->shader->id) : backend::FindShader(mat->shader->name.c_str());     // Shader::id = table index (no strcasecmp scan per item per frame)
      int q = mat->renderQueue >= 0 ? mat->renderQueue : sd->queue;
      items.push_back(Item{ rb.r, mesh, sub, mat, sd, q, depth, mi, fontTex });
    }
  }

  std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
    if (a.queue != b.queue) return a.queue < b.queue;
    return a.queue < 2500 ? a.depth < b.depth : a.depth > b.depth;
  });

  for (const Item& it : items) {
    const Mesh* m = it.mesh;
    const std::vector<int>* tris = nullptr;
    if (m->subMeshCount() == 0) { static std::vector<int> none; tris = &none; }
    else tris = &m->subTriangles[it.sub];
    if (tris->empty()) continue;
    size_t nv = m->vertices.size();
    backend::DrawCall dc;
    dc.sd = it.sd;
    Texture* tex = it.mat->mainTexture ? it.mat->mainTexture : it.fontTex;
    dc.texture = tex ? tex->GetNativeTextureID() : 0;
    dc.tint = it.mat->color;
    dc.brightness = it.mat->HasProperty("_Brightness") ? it.mat->GetFloat("_Brightness") : 0.3f;     // ButtonGray is the only user; shader property default = 0.3
    dc.stScale = it.mat->mainTextureScale; dc.stOffset = it.mat->mainTextureOffset;
    dc.positions = m->vertices.data(); dc.vertexCount = (int)nv;
    dc.uvs = m->uv.size() == nv ? m->uv.data() : nullptr;
    dc.colors = nullptr;
    if (m->colors.size() == nv) {                       // float colours (Mesh.colors) -> bytes
      s_colScratch.resize(nv);
      for (size_t i = 0; i < nv; i++) s_colScratch[i] = Color32(m->colors[i]);
      dc.colors = s_colScratch.data();
    } else if (m->colors32.size() == nv) dc.colors = m->colors32.data();
    dc.indices = tris->data(); dc.indexCount = (int)tris->size();
    dc.mesh = m; dc.sub = it.sub;
    {   // u16 index cache (per mesh revision): saves the int -> u16 conversion + max scan on every draw
      Mesh::GpuCache& gc = m->gpu;
      if (gc.idxRev != m->revision || gc.idx16.size() != m->subTriangles.size()) {
        gc.idx16.assign(m->subTriangles.size(), std::vector<uint16_t>()); gc.idxMax.assign(m->subTriangles.size(), -1);
        for (size_t k = 0; k < m->subTriangles.size(); k++) { int mx = -1; gc.idx16[k].resize(m->subTriangles[k].size()); for (size_t j = 0; j < m->subTriangles[k].size(); j++) { int v = m->subTriangles[k][j]; mx = v > mx ? v : mx; gc.idx16[k][j] = (uint16_t)v; } gc.idxMax[k] = mx; }
        gc.idxRev = m->revision;
      }
      if (it.sub < (int)gc.idx16.size()) { dc.indices16 = gc.idx16[it.sub].data(); dc.maxIndex = gc.idxMax[it.sub]; }
    }
    dc.model = s_models[it.mi];
    for (int pass = 0; pass < it.sd->npass; pass++) { dc.pass = pass; backend::Draw(dc); }
  }
}

void RenderAllCameras() {
  GatherRenderables();
  static std::vector<Camera*> cams; cams.clear();
  for (GameObject* g : g_gameObjects) {
    if (g->destroyed || !g->activeInHierarchy()) continue;
    for (Component* c : g->components) if (c->RenderKind() == 4) { Camera* cam = static_cast<Camera*>(c); if (cam->enabled && !cam->destroyed) cams.push_back(cam); }
  }
  std::stable_sort(cams.begin(), cams.end(), [](Camera* a, Camera* b) { return a->depth < b->depth; });
  for (Camera* c : cams) {
    // OnRenderImage (Sepia): Unity renders the camera into a temporary texture, then the component Blits it to the screen. `src` is a screen-sized
    // RenderTexture the camera renders into directly (no copy); the Blit then replaces the screen content.
    bool post = false;
    if (!c->targetTexture) for (Component* comp : c->gameObject->components) if (comp->enabled && !comp->destroyed && comp->HasRenderImage()) post = true;
    if (post) { if (!g_imageSrc) g_imageSrc = new RenderTexture(Screen::width, Screen::height, 16); g_forceTarget = g_imageSrc; }
    RenderCamera(c);
    g_forceTarget = nullptr;
    if (post) {
      backend::BindTarget(nullptr);
      g_imageFilled = true;
      { Rect pr = c->pixelRect(); backend::SetBlitRect((int)std::lround(pr.x), (int)std::lround(pr.y), (int)std::lround(pr.width), (int)std::lround(pr.height)); }
      for (Component* comp : c->gameObject->components) if (comp->enabled && !comp->destroyed && comp->HasRenderImage()) comp->OnRenderImage(g_imageSrc, nullptr);
      g_imageFilled = false;
      backend::SetBlitRect(0, 0, 0, 0);
    }
  }
}

void RenderOne(Camera* cam) {          // Camera.Render(): immediate, even if the camera is disabled
  GatherRenderables();
  RenderCamera(cam);
}

}  // namespace engine

void Camera::Render() { engine::RenderOne(this); }

// ------------------------------------------------------------------------------------------------ Graphics.Blit
void Graphics::Blit(Texture* src, Texture* dst, Material* mat) {
  if (!mat || !mat->shader) return;
  (void)dst;                                   // always the camera's current target (OnRenderImage on the screen cameras)
  const backend::ShaderDesc* sd = backend::FindShader(mat->shader->name.c_str());
  Texture* t = mat->mainTexture ? mat->mainTexture : src;
  if (src && t == src && !engine::g_imageFilled) { RenderTexture* r = dynamic_cast<RenderTexture*>(src); if (r) backend::CopyTargetTo(r->colorTex, r->width, r->height); }   // OnRenderImage source = the target as rendered so far
  backend::BlitFullscreen(t ? t->GetNativeTextureID() : 0, sd, mat->color, 1.f);
}

// ------------------------------------------------------------------------------------------------ Engine entry points
namespace Engine {

bool Init(int viewW, int viewH, int viewX0, int viewY0, bool rotated90) {
  Screen::width = viewW; Screen::height = viewH;
  return backend::Init(viewW, viewH, viewX0, viewY0, rotated90);
}
void Render() {
  backend::BeginFrame();                       // black clear of the logical surface (Unity: undefined if no camera clears; black is deterministic)
  engine::RenderAllCameras();
}
void Present() { backend::EndFrameAndPresent(); }
void Shutdown() { backend::Shutdown(); }

}  // namespace Engine
}  // namespace miniunity

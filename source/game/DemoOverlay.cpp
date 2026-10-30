#ifdef DQIV_DEV_TOOLS
#include "game/DemoOverlay.hpp"
#include "game/Layout.hpp"
#include "config.h"
#include "miniunity.hpp"
#include "utils/logger.h"

#include <cmath>
#include <cstdio>

using namespace miniunity;

namespace DemoOverlay {

// ---------------------------------------------------------------------------------------------------------------------------------------
// Renderer self test (create ux0:data/dqiv/rendertest<slot>.on): exercises what the milestone content needs - ortho/perspective cameras, layers, clear
// flags, partial rects + custom projection, RenderTexture + Camera.Render, shader ops (MUL tint, A8, INVERT, vertex colours, alpha test),
// TextMesh with a synthetic bitmap font (incl. a flipped glyph) - without depending on the pack's fonts/prefabs. Layers 13/14/15.
// ---------------------------------------------------------------------------------------------------------------------------------------
namespace {

Mesh* MakeQuadMesh(float w, float h, bool colors, Color c0 = Color::white, Color c1 = Color::white) {
  Mesh* m = new Mesh();
  float x = w * 0.5f, y = h * 0.5f;
  m->vertices = { Vector3(-x, -y, 0), Vector3(x, -y, 0), Vector3(x, y, 0), Vector3(-x, y, 0) };
  m->uv = { Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1) };
  if (colors) m->colors32 = { Color32(c0), Color32(c0), Color32(c1), Color32(c1) };
  m->SetTriangles(std::vector<int>{ 0, 2, 1, 0, 3, 2 });          // clockwise from -Z
  return m;
}
GameObject* MakeQuad(const char* name, int layer, float x, float y, float z, float w, float h, const char* shader, Texture* tex, Color tint, bool vcol = false,
                     Color c0 = Color::white, Color c1 = Color::white) {
  GameObject* g = new GameObject(name);
  g->layer = layer;
  g->transform->localPosition = Vector3(x, y, z);
  g->AddComponent<MeshFilter>()->setMesh(MakeQuadMesh(w, h, vcol, c0, c1));
  g->AddComponent<MeshRenderer>();
  Material* m = new Material(Shader::Find(shader));
  m->mainTexture = tex; m->color = tint;
  g->renderer()->setMaterial(m);
  Object::DontDestroyOnLoad(g);
  return g;
}
Camera* MakeCamera(const char* name, int layer, float depth, CameraClearFlags cf, Color bg, bool ortho) {
  GameObject* g = new GameObject(name);
  Camera* c = g->AddComponent<Camera>();
  c->orthographic = ortho; c->orthographicSize = Screen::height * 0.5f; c->nearClipPlane = 0.3f; c->farClipPlane = 1000.f;
  c->clearFlags = cf; c->backgroundColor = bg; c->depth = depth; c->cullingMask = 1 << layer;
  Object::DontDestroyOnLoad(g);
  return c;
}
Texture2D* MakeAlphaDisc() {
  Texture2D* t = new Texture2D(64, 64, TextureFormat::Alpha8);
  std::vector<Color32> px(64 * 64);
  for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
    float d = std::sqrt((x - 31.5f) * (x - 31.5f) + (y - 31.5f) * (y - 31.5f));
    float a = d < 28 ? 1.f : d < 32 ? (32 - d) / 4.f : 0.f;
    if (x < 6) a = 1.f;                                            // solid bar on the left: shows orientation
    px[y * 64 + x] = Color32(0, 0, 0, (uint8_t)(a * 255));
  }
  t->SetPixels32(px.data(), (int)px.size()); t->Apply();
  return t;
}
// synthetic 64x64 A8 font atlas: 'L' (upright glyph 12w x 16h, stored upright at (2,2)), 'F' = same L stored rotated 90 deg CW (flipped glyph), 'B' = hollow box
Font* MakeTestFont() {
  const int W = 64;
  std::vector<Color32> px(W * W, Color32(0, 0, 0, 0));
  auto put = [&](int x, int y, bool on) { px[y * W + x] = Color32(255, 255, 255, on ? 255 : 0); };     // y: row 0 = bottom (texture space)
  auto Lshape = [](int gx, int gy, int gw, int gh) { return gx < 3 || gy >= gh - 3; };                 // glyph space: (0,0) = top-left, y down
  // 'L' at atlas (2,2)..(14,18): glyph (gx,gy) -> atlas x=2+gx, row-from-top
  for (int gy = 0; gy < 16; gy++) for (int gx = 0; gx < 12; gx++) put(2 + gx, 18 - 1 - gy + 0, Lshape(gx, gy, 12, 16));   // v up: glyph top at larger row
  // 'F': glyph L rotated 90 deg CW and stored at atlas x 20..36 (16 wide), rows 2..14 (12 tall): TL->TR, TR->BR, BR->BL, BL->TL
  for (int gy = 0; gy < 16; gy++) for (int gx = 0; gx < 12; gx++) {
    int ax = 15 - gy;                       // glyph row (from top) -> atlas column (BL of the glyph ends up at the left)
    int row = 2 + 11 - gx;                  // glyph column (from left) -> atlas row counted from the top of the 12-row stored image
    put(20 + ax, row, Lshape(gx, gy, 12, 16));
  }
  for (int y = 0; y < 16; y++) for (int x = 0; x < 12; x++) put(40 + x, 2 + y, x < 2 || x > 9 || y < 2 || y > 13);   // 'B'
  Texture2D* tex = new Texture2D(W, W, TextureFormat::Alpha8);
  tex->SetPixels32(px.data(), (int)px.size()); tex->Apply();
  Font* f = new Font();
  f->name = "TestFont"; f->texture = tex; f->lineSpacing = 20; f->ascent = 16; f->fontSize = 16; f->pixelScale = 0.1f;
  Material* m = new Material(Shader::Find("Custom/FontAlpha8")); m->mainTexture = tex; f->material = m;
  float inv = 1.f / W;
  Font::CharInfo L{}; L.index = 'L'; L.uv = Rect(2 * inv, 18 * inv, 12 * inv, -16 * inv); L.vert = Rect(0, -2, 12, -16); L.width = 14; L.flipped = false;
  Font::CharInfo F{}; F.index = 'F'; F.uv = Rect(20 * inv, 14 * inv, 16 * inv, -12 * inv); F.vert = Rect(0, -2, 12, -16); F.width = 14; F.flipped = true;
  Font::CharInfo B{}; B.index = 'B'; B.uv = Rect(40 * inv, 18 * inv, 12 * inv, -16 * inv); B.vert = Rect(0, -2, 12, -16); B.width = 14; B.flipped = false;
  Font::CharInfo S{}; S.index = ' '; S.uv = Rect(0, 0, 0, 0); S.vert = Rect(0, 0, 0, 0); S.width = 8;
  f->chars = { L, F, B, S };
  return f;
}

class RTDriver : public Component { public:
  Camera* rtCam = nullptr; GameObject* spin = nullptr; GameObject* spin3d = nullptr; float t = 0;
  void LateUpdate() override {
    t += 1.f / 30.f;
    if (spin) spin->transform->localRotation = Quaternion::Euler(0, 0, t * 90.f);
    if (spin3d) spin3d->transform->localRotation = Quaternion::Euler(0, t * 60.f, 0);
    if (rtCam) rtCam->Render();                                    // Camera.Render of a disabled camera into its RenderTexture (Capture does this in LateUpdate)
  }
};

void CreateRenderTest() {
  Texture* logo = Resources::Load<Texture>("boot/sqex");
  Texture2D* disc = MakeAlphaDisc();
  Font* font = MakeTestFont();

  // main ortho camera, 1 unit = 1 pixel of the 408x544 surface (y up, origin centre)
  Camera* cam = MakeCamera("RT_Main", 13, 100.f, CameraClearFlags::Color, Color(0.15f, 0.15f, 0.2f, 1.f), true);
  cam->transform()->localPosition = Vector3(0, 0, -10);

  const float W = (float)Screen::width, H = (float)Screen::height;
  float y = H * 0.5f - 40;
  // row 1: logo plain / tint alpha 0.5 / tint red
  MakeQuad("logo", 13, -130, y, 0, 120, 40, "Custom/Picture", logo, Color::white);
  MakeQuad("logoA", 13, 0, y, 0, 120, 40, "Custom/Picture", logo, Color(1, 1, 1, 0.5f));
  MakeQuad("logoR", 13, 130, y, 0, 120, 40, "Custom/Picture", logo, Color(1, 0.3f, 0.3f, 1));
  y -= 60;
  // row 2: A8 disc tinted white/red/green over a bright background (FontAlpha8: rgb from colour, alpha from texture)
  MakeQuad("bg2", 13, 0, y, 1, W - 20, 52, "Custom/Picture", nullptr, Color(0.9f, 0.9f, 0.3f, 1));
  MakeQuad("a8w", 13, -130, y, 0, 48, 48, "Custom/FontAlpha8", disc, Color::white);
  MakeQuad("a8r", 13, -65, y, 0, 48, 48, "Custom/FontAlpha8", disc, Color::red);
  MakeQuad("a8c", 13, 0, y, 0, 48, 48, "Custom/CanvasAlpha8", disc, Color(0, 0.4f, 0.8f, 1));
  MakeQuad("a8v", 13, 65, y, 0, 48, 48, "Custom/FontAlpha8", disc, Color::white, true, Color::red, Color(0, 0, 1, 1));
  y -= 60;
  // row 3: INVERT (ButtonReverse), GRAY (ButtonGray), vertex-colour gradient (DssaNormal), alpha test (Chara) on the A8 disc ... and the RGB565 logo
  MakeQuad("inv", 13, -130, y, 0, 120, 40, "Custom/ButtonReverse", logo, Color::white);
  MakeQuad("gray", 13, 0, y, 0, 120, 40, "Custom/ButtonGray", logo, Color::white);
  MakeQuad("grad", 13, 130, y, 0, 120, 40, "Custom/DssaNormal", nullptr, Color::white, true, Color(1, 0, 0, 1), Color(0, 1, 0, 1));
  y -= 60;
  // row 4: TextMesh (synthetic font): left/upper, centred/middle with rich colour, right/lower with 2 lines, flipped glyph
  GameObject* t1 = new GameObject("T1"); t1->layer = 13;
  { TextMesh* tm = t1->AddComponent<TextMesh>(); tm->font = font; tm->text = "LBFL"; tm->characterSize = 10; tm->anchor = 0; tm->alignment = 0;
    t1->renderer()->setSharedMaterial(font->material); t1->transform->localPosition = Vector3(-190, y + 20, 0); }
  GameObject* t2 = new GameObject("T2"); t2->layer = 13;
  { TextMesh* tm = t2->AddComponent<TextMesh>(); tm->font = font; tm->text = "L<color=red>B</color><color=#00ff00>F</color>\nLBL"; tm->characterSize = 10; tm->anchor = 4; tm->alignment = 1;
    Material* m = new Material(Shader::Find("Custom/FontAlpha8")); m->mainTexture = font->texture; m->color = Color(1, 1, 0.5f, 1); t2->renderer()->setSharedMaterial(m);
    t2->transform->localPosition = Vector3(0, y, 0); }
  GameObject* t3 = new GameObject("T3"); t3->layer = 13;
  { TextMesh* tm = t3->AddComponent<TextMesh>(); tm->font = font; tm->text = "BB\nLLL"; tm->characterSize = 10; tm->anchor = 8; tm->alignment = 2;
    t3->renderer()->setSharedMaterial(font->material); t3->transform->localPosition = Vector3(190, y - 20, 0); }
  Object::DontDestroyOnLoad(t1); Object::DontDestroyOnLoad(t2); Object::DontDestroyOnLoad(t3);
  y -= 70;

  // RenderTexture: a disabled ortho camera (layer 14) rendered by Camera.Render each LateUpdate; shown on a quad
  RenderTexture* rt = new RenderTexture(128, 128, 16);
  Camera* rcam = MakeCamera("RT_Cam", 14, -5.f, CameraClearFlags::Color, Color(0.1f, 0.3f, 0.6f, 1.f), true);
  rcam->orthographicSize = 64; rcam->targetTexture = rt; rcam->enabled = false;
  rcam->transform()->localPosition = Vector3(0, 0, -10);
  GameObject* spin = MakeQuad("rt_red", 14, 0, 0, 0, 70, 30, "Custom/DssaNormal", nullptr, Color::white, true, Color::red, Color::yellow);
  MakeQuad("rt_logo", 14, 0, -40, 1, 100, 33, "Custom/Picture", logo, Color::white);
  MakeQuad("rt_view", 13, -130, y, 0, 128, 128, "Custom/Picture", rt, Color::white);

  // texture scale/offset (DSSA: (1,-1)/(0,1) flips V) and negative V with Repeat wrap (MenuParts.PartsInfo.SetUV: v = -y) on a POT texture
  { GameObject* g = MakeQuad("st", 13, 0, y + 40, 0, 120, 40, "Custom/DssaNormal", logo, Color::white);
    g->renderer()->material()->SetTextureScale("_MainTex", Vector2(1, -1)); g->renderer()->material()->SetTextureOffset("_MainTex", Vector2(0, 1)); }
  { Texture* mt = Resources::Load<Texture>("Menu/MenuTexture");
    GameObject* g = MakeQuad("negv", 13, 0, y - 30, 0, 64, 64, "Custom/MenuParts", mt, Color::white);
    Mesh* m = g->GetComponent<MeshFilter>()->sharedMesh;
    m->uv = { Vector2(0, 0), Vector2(0.25f, 0), Vector2(0.25f, -0.25f), Vector2(0, -0.25f) }; }

  // 3D camera with a partial rect (right-bottom quarter), depth-only clear, custom projection with fixed horizontal FOV (SceneCamera style)
  Camera* c3 = MakeCamera("RT_3D", 15, 101.f, CameraClearFlags::Depth, Color::black, false);
  c3->rect = Rect(0.4f, 0.0f, 0.6f, 0.34f);
  c3->transform()->localPosition = Vector3(0, 0, -20);
  { float fov = 40.f; Matrix4x4 p = Matrix4x4::Perspective(fov, 1.f / c3->aspect(), 1.f, 100.f); float a = p.m[0]; p.m[0] = p.m[5]; p.m[5] = a; c3->setProjectionMatrix(p); }
  GameObject* spin3d = MakeQuad("rt_3dquad", 15, 0, 0, 0, 9, 9, "Custom/fld_normal", logo, Color::white);          // default cull Back: the back side must vanish while it spins
  MakeQuad("rt_3dfloor", 15, 0, -6, 5, 16, 1.5f, "Custom/DssaNormal", nullptr, Color::white, true, Color(1, 0.5f, 0, 1), Color(1, 0.5f, 0, 1));

  GameObject* d = new GameObject("RTDriver");
  RTDriver* drv = d->AddComponent<RTDriver>(); drv->rtCam = rcam; drv->spin = spin; drv->spin3d = spin3d;
  Object::DontDestroyOnLoad(d);
  l_info("RenderTest scene created (surface %.0fx%.0f)", W, H);
}

bool Flag(const char* path) { FILE* f = fopen(path, "r"); if (f) { fclose(f); return true; } return false; }

}  // namespace

void Create() {
  if (Flag(DATA_PATH "rendertest" DQIVN_SLOT_STR ".on")) { CreateRenderTest(); return; }
  if (Layout::IsLandscape()) return;                  // landscape: the world covers the surface, nothing is drawn behind it (dev aid for the pillar-boxed portrait surface)
  GameObject* camGo = new GameObject("DemoCamera");
  Camera* cam = camGo->AddComponent<Camera>();
  cam->orthographic = true; cam->orthographicSize = Screen::height * 0.5f; cam->nearClipPlane = 0.3f; cam->farClipPlane = 1000.f;
  cam->clearFlags = CameraClearFlags::Color; cam->backgroundColor = Color(0.07f, 0.10f, 0.24f, 1.f);   // dark blue: shows the 408x544 surface against the black pillars
  cam->depth = -100.f; cam->cullingMask = 1 << 12;
  Object::DontDestroyOnLoad(camGo);

  Texture* tex = Resources::Load<Texture>("boot/retro5_copyright");
  GameObject* q = GameObject::CreatePrimitive(PrimitiveType::Quad);
  q->name = "DemoCopyright";
  q->layer = 12;
  float w = (float)Screen::width;                                       // fit the logical width
  float h = tex && tex->width ? w * (float)tex->height / (float)tex->width : 36.f;
  q->transform->localPosition = Vector3(0.f, 0.f, 10.f);
  q->transform->localScale = Vector3(w, h, 1.f);
  Material* m = new Material(Shader::Find("Custom/Picture"));
  m->mainTexture = tex;
  m->color = Color::white;
  q->renderer()->setMaterial(m);
  Object::DontDestroyOnLoad(q);
  l_info("DemoOverlay: copyright quad %.0fx%.0f (texture %s)", w, h, tex ? "loaded" : "MISSING");
}

}  // namespace DemoOverlay

#endif  // DQIV_DEV_TOOLS

// game/Capture.cpp -- OWNER: WP6. Line-by-line port of Capture.cs (see Capture.hpp).
#include "game/Layout.hpp"
#include "game/Capture.hpp"

#include <cmath>
#include <cstdlib>

#include "game/Fade.hpp"
#include "game/Field.hpp"
#include "game/FldObject.hpp"
#include "game/Global.hpp"
#include "game/MainLoop.hpp"
#include "game/NativeManager.hpp"
#include "game/TouchPad.hpp"

namespace {
constexpr int m_divX = 32;
constexpr float c_dropBiasX = 0.25f;
Capture* l_self = nullptr;
RenderTexture* l_capTex = nullptr;
RenderTexture* l_blurTexs[2];
int l_blurTexNum = 0;
Material* l_capMat = nullptr;
Material* l_blurMat = nullptr;
bool l_bCapture = false, l_bBlur = false, l_bBlurFirst = false;
Mesh* l_mesh = nullptr;
Coroutine l_doFunc;
int m_divY;
const Color32 c_white(255, 255, 255, 255), c_alpha0(255, 255, 255, 0);

// Wide layouts: the effects work in clip space, where a rotation on a 16:9 surface shears the picture. The rotation is done in pixel proportions instead
// (x scaled by the surface aspect, rotate, scale back); in portrait A == 1 reproduces the original clip-space maths exactly.
float RotAspect() { return Layout::Wide() ? (float)Screen::width / (float)Screen::height : 1.f; }
int Rand(int n) { return n > 0 ? std::rand() % n : 0; }       // Utility.Rand(a) = Random.Next(a)
// the camera that draws the capture plane (clip-space quads on layer 8): the MenuCamera, or in landscape the full-surface "BG" camera (the MenuCamera is only the panel)
Camera* EffCam() { return Layout::Wide() ? Fade::FullCamera(1) : TouchPad::m_camera; }
CameraClearFlags EffCamRestoreFlags() { return Layout::Wide() ? CameraClearFlags::Nothing : CameraClearFlags::Depth; }
Camera* CaptureCamera() { return Field::m_camera ? Field::m_camera : MainLoop::m_camera; }
// shared epilogue of the effects: restore the cameras, hide the capture plane
void RestoreCameras() {
  l_mesh->Clear();
  l_self->renderer()->enabled = false;
  CaptureCamera()->enabled = true;
  EffCam()->clearFlags = EffCamRestoreFlags();
  l_self->gameObject->setActive(false);
}
void HideSceneCameras() {
  l_self->renderer()->enabled = true;
  EffCam()->backgroundColor = Color::black;
  EffCam()->clearFlags = CameraClearFlags::Color;
  CaptureCamera()->enabled = false;
}
}  // namespace

void Capture::SystemInit() { (new GameObject())->AddComponent<Capture>()->name = "CaptureCTRL"; }

void Capture::ScreenEffect(int screen_effect) {
  bool flag = (screen_effect & 0x80) != 0;
  screen_effect &= 0xF;
  Global::Flag |= 2;
  if (l_self) l_self->gameObject->setActive(true);       // SystemInit's Start has not finished (never during the game: Flag 0x10000 gates the native calls)
  switch (screen_effect) {
    case 5: l_doFunc = DarkLamp(flag); break;
    case 6: l_doFunc = Riremito(); break;
    case 3: l_doFunc = Tabi(flag); break;
    default: l_doFunc = BattleEffect(); break;
  }
}

void Capture::Snap() { l_self->SnapShot(); }
bool Capture::IsEndSnap() { return !l_bCapture; }
void Capture::SetVisible(bool bShow) { l_self->gameObject->setActive(bShow); l_self->renderer()->enabled = bShow; }
void Capture::SetBlur(bool b) { l_bBlur = (l_bBlurFirst = b); }
Coroutine Capture::DummyFunc() { while (true) co_yield 0; }
void Capture::Awake() {}

Coroutine Capture::StartCo() {
  while ((Global::Flag & 0x10000) != 0) co_yield 0;
  MeshRenderer* mr = gameObject->AddComponent<MeshRenderer>();
  Material* ma[2];
  l_mesh = gameObject->AddComponent<MeshFilter>()->mesh();
  m_divY = (int)(32.f * (1.f / Layout::Aspect(MainLoop::m_camera->aspect())));
  l_doFunc = DummyFunc();
  l_bCapture = false;
  gameObject->layer = Layout::Wide() ? Fade::FullLayer(1) : 8;
  l_capTex = new RenderTexture(Screen::width, Screen::height, 16);
  l_blurTexs[0] = new RenderTexture(Screen::width >> 1, Screen::height >> 1, 16);
  l_blurTexs[1] = new RenderTexture(Screen::width >> 1, Screen::height >> 1, 16);
  l_blurTexs[0]->filterMode = FilterMode::Bilinear;
  l_blurTexs[1]->filterMode = FilterMode::Bilinear;
  (ma[0] = (l_capMat = new Material(Shader::Find("Custom/Capture"))))->mainTexture = l_capTex;
  ma[1] = (l_blurMat = new Material(Shader::Find("Custom/CaptureBlur")));
  mr->setMaterials(std::vector<Material*>{ ma[0], ma[1] });
  mr->enabled = false;
  transform()->SetParent(TouchPad::m_camera->transform(), false);
  transform()->localRotation = Quaternion::identity;
  transform()->localPosition = Vector3::forward * 180.f;
  l_self = this;
  m_ready = true;
  Object::DontDestroyOnLoad(gameObject);
}

void Capture::SnapShot() { l_bCapture = true; }
void Capture::LateUpdate() { if (m_ready) Caputer(); }

void Capture::Caputer() {
  if (l_bCapture) {
    Camera* camera = CaptureCamera();
    camera->targetTexture = l_capTex;
    camera->Render();
    camera->targetTexture = nullptr;
    l_bCapture = false;
  }
  if (l_bBlur) {
    EffCam()->targetTexture = l_blurTexs[l_blurTexNum];
    EffCam()->Render();
    EffCam()->targetTexture = nullptr;
    l_blurMat->mainTexture = l_blurTexs[l_blurTexNum];
    l_blurTexNum = (l_blurTexNum + 1) & 1;
    if (l_bBlurFirst) {
      EffCam()->targetTexture = l_blurTexs[l_blurTexNum];
      EffCam()->Render();
      EffCam()->targetTexture = nullptr;
      l_bBlurFirst = false;
    }
  }
  if (!l_doFunc.MoveNext()) {
    Global::Flag &= -3;
    l_doFunc = DummyFunc();
  }
}

void Capture::SetMesh(std::vector<Vector3>& vertices, std::vector<Color32>& colors) {
  int num = 0;
  int num2 = 32 * m_divY + 1;
  int num3 = num2 - 1;
  std::vector<int> array(num3 * 6);
  std::vector<Vector2> array2(num2 * 4);
  vertices.assign(num2 * 4, Vector3());
  colors.assign(num2 * 4, c_white);
  float num4 = 1.f, num5 = 0.0625f, num6 = -2.f / (float)m_divY, num7 = 1.f, num8 = 1.f / 32.f, num9 = -1.f / (float)m_divY;
  int i = 0, num10 = 0;
  for (; i < m_divY; i++) {
    float num11 = -1.f, num12 = num4 + num6, num13 = 0.f, num14 = num7 + num9;
    int num15 = 0;
    while (num15 < 32) {
      int num16 = num10 * 6;
      float num17 = num13 + num8, num18 = num11 + num5;
      num = num10 << 2;
      vertices[num].Set(num11, num4, 0.f);
      vertices[num + 1].Set(num11, num12, 0.f);
      vertices[num + 2].Set(num18, num4, 0.f);
      vertices[num + 3].Set(num18, num12, 0.f);
      array2[num].Set(num13, num7);
      array2[num + 1].Set(num13, num14);
      array2[num + 2].Set(num17, num7);
      array2[num + 3].Set(num17, num14);
      array[num16] = num; array[num16 + 1] = num + 2; array[num16 + 2] = num + 1;
      array[num16 + 3] = num + 2; array[num16 + 4] = num + 3; array[num16 + 5] = num + 1;
      num11 = num18; num13 = num17;
      num15++; num10++;
    }
    num4 = num12; num7 = num14;
  }
  num = num3 << 2;
  vertices[num].Set(-1.f, 1.f, 0.1f); vertices[num + 1].Set(-1.f, -1.f, 0.1f); vertices[num + 2].Set(1.f, 1.f, 0.1f); vertices[num + 3].Set(1.f, -1.f, 0.1f);
  array2[num].Set(0.f, 1.f); array2[num + 1].Set(0.f, 0.f); array2[num + 2].Set(1.f, 1.f); array2[num + 3].Set(1.f, 0.f);
  l_mesh->Clear();
  l_mesh->vertices = vertices;
  l_mesh->uv = array2;
  l_mesh->colors32 = colors;
  l_mesh->SetSubMeshCount(2);
  l_mesh->SetTriangles(array, 0);
  num = num3 << 2;
  l_mesh->SetTriangles(std::vector<int>{ num, num + 2, num + 1, num + 2, num + 3, num + 1 }, 1);
}

Coroutine Capture::BattleEffect() {
  std::vector<Vector3> vertices; std::vector<Color32> colors;
  float fadeAlpha = 2.f / (float)m_divY;
  l_blurMat->color = Color::white;
  l_capMat->color = Color::white;
  SetBlur(true);
  Snap();
  co_yield 0;
  SetMesh(vertices, colors);
  do { co_yield 0; } while (l_bCapture);
  HideSceneCameras();
  l_blurMat->color = Color(0.8f, 0.8f, 0.8f, 1.f);
  co_yield 0;
  int counterA = 0, counterB = 0;
  while (counterA <= m_divY) {
    int py = 0, q = 0;
    for (; py < m_divY; py++) {
      int px = 0, n = py;
      while (px < 32) {
        int c, d;
        if (Rand(3) == 0) { c = counterB; d = counterA; } else { c = counterA; d = counterB; }
        bool b = c > n;
        if (b || d > n) {
          int i = q << 2;
          float ay = (float)(c - n + 32 - px) * -0.0015625f + -2.f / 1024.f;
          float dv = 0.001f * (float)std::cos((double)(float)(49152 + c * 120) / 65535.0);
          if (!b) { ay *= 0.5f; dv *= 0.5f; }
          if (colors[i].a != 0) {
            if (colors[i].a < 8) { colors[i] = c_alpha0; colors[i + 1] = c_alpha0; colors[i + 2] = c_alpha0; colors[i + 3] = c_alpha0; }
            else { colors[i].a -= 8; colors[i + 1].a -= 8; colors[i + 2].a -= 8; colors[i + 3].a -= 8; }
          }
          vertices[i].x += -2.f / 1024.f + dv; vertices[i].y += ay; vertices[i++].z += -0.01f;
          vertices[i].x += -2.f / 1024.f + dv; vertices[i].y += ay; vertices[i++].z += -0.01f;
          vertices[i].x += -2.f / 1024.f - dv; vertices[i].y += ay; vertices[i++].z += -0.01f;
          vertices[i].x += -2.f / 1024.f - dv; vertices[i].y += ay; vertices[i++].z += -0.01f;
        }
        px++; q++;
      }
    }
    l_mesh->colors32 = colors;
    l_mesh->vertices = vertices;
    int num = counterA = counterA + 1;
    if (num > 20) counterB++;
    if (counterA > m_divY / 2) {
      float c2 = l_capMat->color.a - fadeAlpha;
      if (c2 < 0.f) c2 = 0.f;
      l_capMat->color = Color(c2, c2, c2, c2);
    }
    co_yield 0;
  }
  SetBlur(false);
  Fade::SetLayer(0);
  Fade::SetFadeOutBlack(0);
  co_yield 0;
  RestoreCameras();
}

Coroutine Capture::DarkLamp(bool bMorning) {
  int h_divX = 16, h_divY = m_divY >> 2, divX = h_divX + 1, divY = h_divY + 1;
  float ay = 2.f / (float)h_divY;
  l_blurMat->color = Color::white;
  l_capMat->color = Color::white;
  SetBlur(true);
  Snap();
  co_yield 0;
  int i = 0, j = 0;
  float fv = 1.f, au = 1.f / (float)h_divX, av = -1.f / (float)h_divY, fy = 1.f, ax = 2.f / (float)h_divX;
  std::vector<Vector2> uv(divX * divY + 4);
  std::vector<int> tr(h_divX * h_divY * 6);
  std::vector<Vector3> vt(uv.size());
  std::vector<Color32> cl(uv.size(), Color32(0, 0, 0, 0));
  std::vector<float> pt(uv.size() - 4);
  for (int y = 0; y < divY; y++) {
    float fx = -1.f, fu = 0.f;
    for (int x = 0; x < divX; x++) {
      vt[i].Set(fx, pt[i] = fy, 0.255f);
      uv[i].Set(fu, fv);
      cl[i++] = c_white;
      fx += ax; fu += au;
    }
    fy -= ay; fv += av;
  }
  j = i;
  vt[i].Set(-1.f, 1.f, 0.1f); vt[i + 1].Set(-1.f, -1.f, 0.1f); vt[i + 2].Set(1.f, 1.f, 0.1f); vt[i + 3].Set(1.f, -1.f, 0.1f);
  uv[i].Set(0.f, 1.f); uv[i + 1].Set(0.f, 0.f); uv[i + 2].Set(1.f, 1.f); uv[i + 3].Set(1.f, 0.f);
  i = 0;
  int y2 = 0, u = 0, d = divX;
  while (y2 < h_divY) {
    int x2 = 0;
    while (x2 < h_divX) {
      tr[i++] = u; tr[i++] = u + 1; tr[i++] = d; tr[i++] = u + 1; tr[i++] = d + 1; tr[i++] = d;
      x2++; u++; d++;
    }
    y2++; u++; d++;
  }
  l_mesh->Clear();
  l_mesh->vertices = vt; l_mesh->uv = uv; l_mesh->colors32 = cl;
  l_mesh->SetSubMeshCount(2);
  l_mesh->SetTriangles(tr, 0);
  l_mesh->SetTriangles(std::vector<int>{ j, j + 2, j + 1, j + 2, j + 3, j + 1 }, 1);
  do { co_yield 0; } while (l_bCapture);
  HideSceneCameras();
  float movement = -125.f / 8192.f, bias = 1.f, cbias = bMorning ? 0.f : 1.f, fade = 1.f, cAlpha = 0.9125f, bAlpha = 0.f;
  int counter = 0, ye = divY - 1, xe = divX - 1, hx = divX >> 1;
  for (; counter < 60; counter++) {
    if (counter < 10) {
      float a;
      bAlpha = (a = bAlpha + 0.0875f);
      l_blurMat->color = Color(1.f, 1.f, 1.f, a);
      cAlpha = (a = cAlpha + -0.01f);
      l_capMat->color = Color(1.f, 1.f, 1.f, a);
    }
    for (int k = 1; k < ye; k++) {
      int x3 = 1, i2 = x3 + k * divX;
      while (x3 < xe) {
        float a2 = pt[i2];
        vt[i2].x *= 1.0015f;
        vt[i2].y *= 1.0015f;
        for (int l = 0; l < 8; l++) {
          float v = (vt[i2 - 1].y + vt[i2 + 1].y + vt[i2 - divX].y + vt[i2 + divX].y) * 0.25f - a2;
          a2 += v * 0.25f;
          vt[i2].y = a2;
        }
        x3++; i2++;
      }
    }
    int y3 = 0, i3 = hx;
    while (y3 < divY) {
      float a3 = pt[i3];
      float b = Mathf::Abs(a3);
      vt[i3].y = a3 * (1.f - b) + pt[i3] * bias * b;
      b -= 0.1f;
      for (int m = 1; m < hx; m++) {
        vt[i3 - m].y = a3 * (1.f - b) + pt[i3 - m] * bias * b;
        vt[i3 + m].y = a3 * (1.f - b) + pt[i3 + m] * bias * b;
        b *= b;
      }
      y3++; i3 += divX;
    }
    if (bMorning) {
      for (int n = 0; n < divX; n++) {
        float a4 = 2.f - Mathf::Abs(vt[n].x);
        a4 *= a4 * cbias;
        int y4 = 0, i4 = n;
        while (y4 < divY) {
          float c = vt[i4].z * 1000.f;
          c -= a4;
          if (c < 0.f) c = 0.f; else if (c > 255.f) c = 255.f;
          cl[i4].b = (uint8_t)c;
          vt[i4].z = c * 0.001f;
          y4++; i4 += divX;
        }
      }
    } else {
      for (int n = 0; n < divX; n++) {
        float a5 = 2.f - Mathf::Abs(vt[n].x);
        a5 *= a5 * cbias;
        int y5 = 0, i5 = n;
        while (y5 < divY) {
          float c2 = vt[i5].z * 1000.f;
          c2 -= a5;
          if (c2 < 0.f) c2 = 0.f; else if (c2 > 255.f) c2 = 255.f;
          cl[i5].r = (uint8_t)c2;
          cl[i5].g = (uint8_t)c2;
          vt[i5].z = c2 * 0.001f;
          y5++; i5 += divX;
        }
      }
    }
    if (cbias < 5.f) cbias += 0.25f;
    bias += movement;
    if (bias < 0.6f || (movement > 0.f && bias > 1.2f)) movement = 0.f - movement;
    l_mesh->vertices = vt;
    l_mesh->colors32 = cl;
    if (counter >= 30 && fade > 0.f) {
      fade -= 1.f / 30.f;
      if (fade < 0.f) fade = 0.f;
      l_capMat->color = Color(fade, fade, fade, cAlpha);
    }
    co_yield 0;
  }
  SetBlur(false);
  Fade::SetLayer(0);
  Fade::SetFadeOutBlack(0);
  co_yield 0;
  RestoreCameras();
}

Coroutine Capture::Riremito() {
  std::vector<Vector3> vt(20);
  l_blurMat->color = Color::white;
  l_capMat->color = Color::white;
  SetBlur(true);
  Snap();
  co_yield 0;
  int nm = (int)vt.size();
  std::vector<Color32> cl(nm, c_white);
  std::vector<Vector2> uv(nm);
  std::vector<int> tr(6 * (nm >> 2));
  for (int j = 0, ie = nm >> 2; j < ie; j++) {
    int b = j << 2;
    uv[b].Set(0.f, 1.f); uv[b + 1].Set(0.f, 0.f); uv[b + 2].Set(1.f, 1.f); uv[b + 3].Set(1.f, 0.f);
  }
  for (int k = 0, ie2 = (nm >> 2) - 1; k < ie2; k++) {
    int b2 = k << 2, t = k * 6;
    tr[t] = b2; tr[t + 1] = b2 + 2; tr[t + 2] = b2 + 1; tr[t + 3] = b2 + 2; tr[t + 4] = b2 + 3; tr[t + 5] = b2 + 1;
  }
  nm -= 4;
  vt[nm].Set(-1.f, 1.f, 0.5f); vt[nm + 1].Set(-1.f, -1.f, 0.5f); vt[nm + 2].Set(1.f, 1.f, 0.5f); vt[nm + 3].Set(1.f, -1.f, 0.5f);
  l_mesh->Clear();
  l_mesh->vertices = vt; l_mesh->uv = uv; l_mesh->colors32 = cl;
  l_mesh->SetSubMeshCount(2);
  l_mesh->SetTriangles(tr, 0);
  l_mesh->SetTriangles(std::vector<int>{ nm, nm + 2, nm + 1, nm + 2, nm + 3, nm + 1 }, 1);
  do { co_yield 0; } while (l_bCapture);
  HideSceneCameras();
  float cAlpha = 0.9f, bAlpha = 0.f, rot = 0.f, fade = 1.f;
  Vector2 vbs[4] = { Vector2(-1.f, 1.f), Vector2(-1.f, -1.f), Vector2(1.f, 1.f), Vector2(1.f, -1.f) };
  int counter = 0;
  Vector3 trans;
  while (true) {
    int num = counter = counter + 1;
    if (num >= 60) break;
    float z;
    rot = (z = rot + 0.11f);
    Quaternion r = Quaternion::Euler(0.f, 0.f, z);
    if (counter < 10) {
      bAlpha += 0.1f;
      l_blurMat->color = Color(bAlpha, bAlpha, bAlpha, 1.f);
      cAlpha = (z = cAlpha + -0.08f);
      l_capMat->color = Color(1.f, 1.f, 1.f, z);
    }
    for (int l = 0; l < 4; l++) {
      float baseT = (float)(10 * (l + 1));
      trans.x = (float)counter * baseT * 3.f * -5E-05f;
      trans.y = (float)counter * baseT * -5E-05f;
      trans.z = (float)l * 0.1f;
      switch (3 - l) {
        case 1: trans.y = 0.f - trans.y; break;
        case 2: trans.x = 0.f - trans.x; trans.y = 0.f - trans.y; break;
        case 3: trans.x = 0.f - trans.x; break;
      }
      int j2 = 0, b3 = l << 2;
      while (j2 < 4) {
        float A = RotAspect(), rad = z * 0.017453292f, cr = std::cos(rad), sr = std::sin(rad), px = vbs[j2].x, py = vbs[j2].y;
        vt[b3] = Vector3(px * cr - py * sr / A, px * A * sr + py * cr, 0.f) + trans;      // == r * (px, py, 0) when A == 1
        j2++; b3++;
      }
    }
    if (counter >= 30 && fade > 0.f) {
      fade -= 1.f / 30.f;
      if (fade < 0.f) fade = 0.f;
      l_capMat->color = Color(fade, fade, fade, cAlpha);
    }
    l_mesh->vertices = vt;
    co_yield 0;
  }
  SetBlur(false);
  Fade::SetLayer(0);
  Fade::SetFadeOutBlack(0);
  co_yield 0;
  RestoreCameras();
}

Coroutine Capture::Tabi(bool bReverse) {
  int h_divX = 16, h_divY = m_divY >> 1, divX = h_divX + 1, divY = h_divY + 1;
  float ay = 2.f / (float)h_divY, ax = 2.f / (float)h_divX;
  SetBlur(true);
  Snap();
  co_yield 0;
  int i = 0, j = 0;
  float fv = 1.f, au = 1.f / (float)h_divX, av = -1.f / (float)h_divY, fy = 1.f;
  std::vector<Vector2> uv(divX * divY + 4);
  std::vector<int> tr(h_divX * h_divY * 6);
  std::vector<Vector3> vt(uv.size());
  std::vector<Color32> cl(uv.size(), Color32(0, 0, 0, 0));
  if (bReverse) { ax = 0.f - ax; ay = 0.f - ay; }
  for (int y = 0; y < divY; y++) {
    float fx = -1.f, fu = 0.f;
    for (int x = 0; x < divX; x++) {
      vt[i].Set(fx, fy, 0.255f);
      uv[i].Set(fu, fv);
      cl[i++] = c_white;
      fx += ax; fu += au;
    }
    fy -= ay; fv += av;
  }
  j = i;
  vt[i].Set(-1.f, 1.f, 0.1f); vt[i + 1].Set(-1.f, -1.f, 0.1f); vt[i + 2].Set(1.f, 1.f, 0.1f); vt[i + 3].Set(1.f, -1.f, 0.1f);
  uv[i].Set(0.f, 1.f); uv[i + 1].Set(0.f, 0.f); uv[i + 2].Set(1.f, 1.f); uv[i + 3].Set(1.f, 0.f);
  i = 0;
  int y2 = 0, u = 0, d = divX;
  while (y2 < h_divY) {
    int x2 = 0;
    while (x2 < h_divX) {
      tr[i++] = u; tr[i++] = u + 1; tr[i++] = d; tr[i++] = u + 1; tr[i++] = d + 1; tr[i++] = d;
      x2++; u++; d++;
    }
    y2++; u++; d++;
  }
  l_mesh->Clear();
  l_mesh->vertices = vt; l_mesh->uv = uv; l_mesh->colors32 = cl;
  l_mesh->SetSubMeshCount(2);
  l_mesh->SetTriangles(tr, 0);
  l_mesh->SetTriangles(std::vector<int>{ j, j + 2, j + 1, j + 2, j + 3, j + 1 }, 1);
  do { co_yield 0; } while (l_bCapture);
  HideSceneCameras();
  int fAlpha = 4;
  float bAlpha = 0.f;
  float bAlphaAcc = (!bReverse ? 0.925f : 0.975f) / (float)fAlpha;
  float cAlpha = 1.f - bAlphaAcc;
  float cAlphaAcc = -0.25f / (float)fAlpha;
  float rotAcc = -1.05f, time = 0.f, timeAcc = 1.f / 60.f;
  int counter = 0;
  if (bReverse) {
    counter = 20;
    ax = 0.f - ax; ay = 0.f - ay;
    time = 1.f - timeAcc * (float)counter;
    timeAcc = 0.f - timeAcc;
    cAlpha = 0.f;
    cAlphaAcc = 0.25f / (float)fAlpha;
    fAlpha += counter;
    Fade::SetLayer(0);
    Fade::SetFadeIn(30);
  }
  for (; counter <= 60; counter++) {
    float fy2 = 1.f;
    float sbias = 1.f - 0.5f * time;
    if (counter < fAlpha) {
      float a;
      bAlpha = (a = bAlpha + bAlphaAcc);
      l_blurMat->color = Color(1.f, 1.f, 1.f, a);
      cAlpha = (a = cAlpha + cAlphaAcc);
      l_capMat->color = Color(1.f, 1.f, 1.f, a);
    }
    if (!bReverse && counter >= 30) {
      if (bAlpha > 0.f) { float a; bAlpha = (a = bAlpha - time * 0.01f); if (a < 0.f) bAlpha = 0.f; }
      if (cAlpha > 0.f) { float a; cAlpha = (a = cAlpha - time * 0.075f); if (a < 0.f) cAlpha = 0.f; }
      l_blurMat->color = Color(1.f, 1.f, 1.f, bAlpha);
      l_capMat->color = Color(1.f, 1.f, 1.f, cAlpha);
    }
    int k = 0, i2 = 0;
    for (; k < divY; k++) {
      float fx2 = -1.f, fyy = fy2 * fy2;
      int x3 = 0;
      while (x3 < divX) {
        float d2 = 1.4142f - Mathf::Sqrt(fx2 * fx2 + fyy);
        float e = d2 * d2 * time * 1.75f;
        float r = rotAcc * time + d2 * rotAcc * time;
        float a2 = (fx2 - fx2 * e) * sbias;
        float b = (fy2 - fy2 * e) * sbias;
        float s = Mathf::Sin(r), c = Mathf::Cos(r);
        if ((a2 < 0.f && fx2 > 0.f) || (a2 > 0.f && fx2 < 0.f)) a2 = fx2 * 0.0001f;
        if ((b < 0.f && fy2 > 0.f) || (b > 0.f && fy2 < 0.f)) b = fy2 * 0.0001f;
        float A = RotAspect();
        vt[i2].x = a2 * c - b * s / A;
        vt[i2].y = a2 * A * s + b * c;
        fx2 += ax; x3++; i2++;
      }
      fy2 -= ay;
    }
    time += timeAcc;
    l_mesh->vertices = vt;
    co_yield 0;
  }
  if (bReverse) {
    SetBlur(false);
    co_yield 0;
    RestoreCameras();
  } else {
    l_blurMat->color = Color::clear;
    l_capMat->color = Color::clear;
  }
}

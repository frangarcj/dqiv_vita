// game/SceneCamera.cpp -- OWNER: WP5a (written by WP2 to get the title screen: DSSA ortho camera + MainCamera projection). Port of SceneCamera.cs.

#include "game/SceneCamera.hpp"
#include "game/Boot.hpp"
#include "game/Chara.hpp"
#include "game/MainLoop.hpp"
#include "game/Layout.hpp"
#include "game/UiCameras.hpp"
#include <cmath>
#include "game/MeshChara.hpp"
#include "game/NativeManager.hpp"
#include "platform/cfg.hpp"
#include "game/Fade.hpp"
#include "utils/logger.h"

#include <algorithm>

Vector3 SceneCamera::s_camera_pos; Quaternion SceneCamera::s_billboardRot = Quaternion::identity;
float SceneCamera::s_fov = 15.f, SceneCamera::s_nearClip = 0.25f, SceneCamera::s_farClip = 200.f, SceneCamera::s_scaleW = 0.0625f, SceneCamera::s_yOffset = 0.f,
      SceneCamera::s_lookDownAdjust = 0.2f, SceneCamera::s_distanceAdjust = 4.f, SceneCamera::s_distance = 0.f;
Vector3 SceneCamera::s_angle, SceneCamera::s_target, SceneCamera::s_position, SceneCamera::s_camUp;
bool SceneCamera::s_targetCamera = true;
Quaternion SceneCamera::s_yReverse = Quaternion::identity;
GameObject* SceneCamera::s_clearPlane = nullptr;
Camera* SceneCamera::s_DSSAOrthoCamera = nullptr;
static Camera* l_camera = nullptr;
static bool l_bCalcProjection = false;
static bool s_bFixAdrendo = false;     // DQ4 SceneCamera.s_bFixAdrendo (GPU name contains ADRENO)
static float l_shiftApplied = 0.f;
static int l_battleFrames = 0; static float l_fitZoom = 1.f, l_ext1W = 0.f, l_ext1H = 0.f;   // layout "large": auto-fit zoom state
static bool l_wideBattle = false;      // landscape battle view (whole surface, zoomed about the monsters' feet)
static float l_sceneH = 1.f;          // landscape: fraction of the surface height taken by the view band (1 = whole height; battle band 0.56)

void SceneCamera::InitParam::Apply() {
  Setup();
  SetFOV2(fov);
  SetAngleDeg(GetAngleVector());
  SetDistance(dist);
  SetNear(nearClip);
  SetFar(farClip);
  SetScaleW(scaleW);
  SetTargetCamera(targetCamera);
  SetViewAndCenterY(Rect(0.f, viewY, 1.f, viewH), yOffset);
  if (bBGClear && !(Layout::Wide() && Layout::GetBattleLayout() != Layout::BattleLayout::Band)) {                // landscape: the battle view uses the whole surface, no black bars (07_LANDSCAPE.md)
    if (s_clearPlane == nullptr) {
      s_clearPlane = new GameObject();
      Mesh* mesh = s_clearPlane->AddComponent<MeshFilter>()->mesh();
      MeshRenderer* meshRenderer = s_clearPlane->AddComponent<MeshRenderer>();
      Vector3 array[8]; Color32 array2[8];
      for (Color32& c : array2) c = Color32(0, 0, 0, 0);                 // C# default(Color32)
      float num = 1.f / (float)Screen::height;
      float new_y = (l_camera->rect.y + l_camera->rect.height) * 2.f - 1.f - num;
      float new_y2 = l_camera->rect.y * 2.f - 1.f + num;
      Material* material = new Material(Shader::Find("BackColor"));
      array[0].Set(-1.05f, 1.05f, 0.f);  array[1].Set(1.05f, 1.05f, 0.f);  array[2].Set(1.05f, new_y, 0.f);  array[3].Set(-1.05f, new_y, 0.f);
      array[4].Set(-1.05f, new_y2, 0.f); array[5].Set(1.05f, new_y2, 0.f); array[6].Set(1.05f, -1.05f, 0.f); array[7].Set(-1.05f, -1.05f, 0.f);
      for (int i = 1; i < 8; i++) array2[i] = Color32(Color::black);
      mesh->vertices.assign(array, array + 8);
      mesh->colors32.assign(array2, array2 + 8);
      mesh->SetTriangles(std::vector<int>{ 0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4 }, 0);
      meshRenderer->setMaterial(material);
      s_clearPlane->layer = Layout::Wide() ? Fade::FullLayer(1) : 8;       // landscape: drawn by the full-surface camera (the MenuCamera is the panel only)
    }
  } else if (s_clearPlane != nullptr) {
    Object::Destroy(s_clearPlane);
    s_clearPlane = nullptr;
  }
}

void SceneCamera::SystemInit() {
  l_camera = MainLoop::m_camera;
  s_yReverse = Quaternion::Euler(Vector3(0.f, 180.f, 0.f));
  l_bCalcProjection = true;
  // DQ4: no DSSA ortho camera (DQ5 SceneCamera.SystemInit created s_DSSAOrthoCamera, layer 11 / cmd_mesh_ortho); s_DSSAOrthoCamera stays null.
  s_bFixAdrendo = Boot::s_gpuName.find("ADRENO") != std::string::npos;     // false on the Vita (PowerVR): TODO(dq4) the non-Adreno clear-plane path (a black Camera at depth-1) is not ported, the DQ5 mesh plane is used for both
}
static void CalcProjection() {
  if (!l_camera) return;
  if (l_bCalcProjection) {
    float num = SceneCamera::s_fov * 1.3333334f;
    float aspect = 1.f / l_camera->aspect();
    Matrix4x4 identity = Matrix4x4::identity();
    l_camera->fieldOfView = num;
    l_camera->nearClipPlane = SceneCamera::s_nearClip;
    l_camera->farClipPlane = SceneCamera::s_farClip;
    Matrix4x4 matrix4x = Matrix4x4::Perspective(num, aspect, SceneCamera::s_nearClip, SceneCamera::s_farClip);
    float value = matrix4x(0, 0);
    matrix4x(0, 0) = matrix4x(1, 1);
    matrix4x(1, 1) = value;
    if (Layout::Wide()) {
      // keep the vertical extent of the 0.75 layout (band: DS 4:3 for battle), widen the horizontal one: M11 = f * designAspect / bandHeight, M00 = M11 / realAspect
      matrix4x(1, 1) = matrix4x(0, 0) * Layout::kDesignAspect / l_sceneH;
      matrix4x(0, 0) = matrix4x(1, 1) / l_camera->aspect();
    }
    identity(1, 3) = SceneCamera::s_yOffset;
    if (l_wideBattle) {
      // battle zoom (cfg battle_zoom, percent, default 150): scale the scene about the monsters' feet (screen y -0.5 NDC, x = centre of the visible area) so they fill the picture
      Layout::BattleLayout bl = Layout::GetBattleLayout();
      float z = bl == Layout::BattleLayout::Panel ? 1.f : bl == Layout::BattleLayout::Panorama ? Layout::BattleZoomPercent() / 100.f : l_fitZoom, feet = -0.5f;
      matrix4x(0, 0) *= z; matrix4x(1, 1) *= z;
      identity(1, 3) = z * SceneCamera::s_yOffset + (1.f - z) * feet;
    }
    identity(0, 3) = l_shiftApplied = Layout::ShiftNdc();       // landscape: world slid left while a menu covers the panel (identity otherwise)
    // DQ4: the whole matrix is scaled by s_scaleW (C#: [0,0],[1,1],[2,2],[2,3] *= scaleW and [3,2] = -scaleW); the projected NDC is unchanged (precision hack of the old engine)
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) matrix4x(r, c) *= SceneCamera::s_scaleW;
    l_camera->setProjectionMatrix(identity * matrix4x);
    l_bCalcProjection = false;
  }
}

// layout "large" (DQIV-Recompiled DualScreen.cpp battleZoomToFit / zoom = (commandsTop - windowsBottom) / monster height, clamped 0.7..2, eased 0.02 per frame):
// the monsters' screen extent is measured while the battle opens (union over frames 8..90, normalised to zoom 1 about the zoom anchor = the monsters' feet line and the
// centre of the area left of the panel), then the zoom that keeps them inside the free space is eased in: horizontally between the screen edge and the panel /
// status block (x < 540), vertically between the name-plate band under the top edge (46 px) and the bottom edge. The status row and the command panel are on the right,
// so only the horizontal room matters for them; with `battle_shift=0` the room would be the whole width minus the panel.
static float g_fitX0, g_fitX1, g_fitY0, g_fitY1;
static void FitBattleZoom() {
  const float sw = (float)Layout::SurfaceW(), sh = (float)Layout::SurfaceH();
  const float cx = sw * 0.5f * (1.f + Layout::ShiftNdc()), feet = sh * 0.75f;      // anchor: x of the world centre, feet line = NDC y -0.5
  l_battleFrames++;
  if (l_battleFrames <= 90 && l_battleFrames >= 8 && MainLoop::m_camera) {
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
    for (int i = 0; i < 40; i++) {
      MeshChara* mc = NativeManager::GetMeshChara(i);
      if (!mc || !mc->m_chara || !mc->m_meshFilter || !mc->m_meshFilter->sharedMesh || !mc->m_renderer || !mc->m_renderer->enabled || mc->m_chara->layer != 0) continue;
      for (const Vector3& v : mc->m_meshFilter->sharedMesh->vertices) {
        Vector3 s = MainLoop::m_camera->WorldToScreenPoint(mc->m_chara->transform->TransformPoint(v));
        if (s.z <= 0.f) continue;
        float px = s.x, py = sh - s.y;                                               // pixels, origin top-left
        x0 = std::min(x0, px); x1 = std::max(x1, px); y0 = std::min(y0, py); y1 = std::max(y1, py);
      }
    }
    if (x1 > x0 && y1 > y0) {                                                       // normalise to zoom 1 about (cx, feet)
      float z = l_fitZoom;
      float nx0 = cx + (x0 - cx) / z, nx1 = cx + (x1 - cx) / z, ny0 = feet + (y0 - feet) / z, ny1 = feet + (y1 - feet) / z;
      if (l_ext1W == 0.f) { g_fitX0 = nx0; g_fitX1 = nx1; g_fitY0 = ny0; g_fitY1 = ny1; l_ext1W = 1.f; }
      else { g_fitX0 = std::min(g_fitX0, nx0); g_fitX1 = std::max(g_fitX1, nx1); g_fitY0 = std::min(g_fitY0, ny0); g_fitY1 = std::max(g_fitY1, ny1); }
    }
  }
  float target = 1.f;
  if (l_ext1W > 0.f) {
    const float left = 10.f, right = sw - (float)Layout::UiW() - 12.f, top = 46.f, bottom = sh - 14.f;
    float z = 2.f;
    if (cx - g_fitX0 > 1.f) z = std::min(z, (cx - left) / (cx - g_fitX0));
    if (g_fitX1 - cx > 1.f) z = std::min(z, (right - cx) / (g_fitX1 - cx));
    if (feet - g_fitY0 > 1.f) z = std::min(z, (feet - top) / (feet - g_fitY0));
    if (g_fitY1 - feet > 1.f) z = std::min(z, (bottom - feet) / (g_fitY1 - feet));
    target = std::min(2.f, std::max(0.7f, z));
  }
  float d = target - l_fitZoom;
  if (std::fabs(d) > 0.0001f) { l_fitZoom += std::fabs(d) < 0.02f ? d : (d > 0 ? 0.02f : -0.02f); l_bCalcProjection = true; }
}

// once per frame (MenuPortSystem::internalUpdate): eases the landscape world shift, re-projects the MainCamera and moves the DSSA ortho camera
void SceneCamera::LayoutUpdate() {
  Layout::SetWorldShiftMode(NativeManager::InBattleStage() && Cfg::GetInt("battle_shift", 1) != 0 ? 2 : Cfg::GetInt("world_shift", 0) != 0 ? 1 : 0);   // field: the world never moves for dialogues/menus (world_shift=1 = slide while a menu is open); battles: fixed slide
  Layout::Update();
  UiCameras::Update();
  if (l_wideBattle && Layout::GetBattleLayout() == Layout::BattleLayout::Large) FitBattleZoom();
  float sh = Layout::ShiftNdc();
  if (sh != l_shiftApplied) l_bCalcProjection = true;
  if (s_DSSAOrthoCamera && Layout::IsLandscape()) {
    Vector3 p = s_DSSAOrthoCamera->transform()->position();
    p.x = -sh * s_DSSAOrthoCamera->orthographicSize * s_DSSAOrthoCamera->aspect();
    s_DSSAOrthoCamera->transform()->setPosition(p);
  }
  if (Layout::IsLandscape()) CalcProjection();
}
void SceneCamera::SetProjectionFitScreenByWidth() { l_bCalcProjection = true; }
void SceneCamera::Setup() { SetTarget(Vector3::zero); s_camUp = Vector3::up; l_bCalcProjection = true; }
void SceneCamera::SetTargetCamera(bool bTarget) { s_targetCamera = bTarget; }

void SceneCamera::Update() {
  if (!l_camera) return;
  CalcProjection();
  Transform* t = l_camera->transform();
  if (s_targetCamera) {
    t->setPosition(s_position);
    t->LookAt(s_target, s_camUp);
    Vector3 eulerAngles = t->eulerAngles();
    eulerAngles.x -= s_lookDownAdjust;
    t->setEulerAngles(eulerAngles);
    t->setPosition(t->position() + t->rotation() * Vector3::forward * s_distanceAdjust);
  } else {
    Vector3 eulerAngles2 = s_angle;
    eulerAngles2.x = 0.f - eulerAngles2.x;
    eulerAngles2.y += 180.f;
    eulerAngles2.y = 0.f - eulerAngles2.y;
    t->setPosition(s_position);
    t->setEulerAngles(eulerAngles2);
  }
  s_camera_pos = t->position();
  s_billboardRot = t->rotation() * s_yReverse;
  Chara::CalcLieBillboard(t->rotation());
}
void SceneCamera::SetPosition(Vector3 pos) { s_position = pos; }
void SceneCamera::SetTarget(Vector3 target) { s_target = target; }
void SceneCamera::SetAngleDeg(Vector3 angleDeg) { s_angle = angleDeg; }
void SceneCamera::SetAngleYDeg(float y) { s_angle.y = y; }
void SceneCamera::SetDistance(float distance) { s_distance = distance; }
void SceneCamera::AddDistance(float distance) { s_distance += distance; }
void SceneCamera::SubDistance(float distance) { s_distance -= distance; }
void SceneCamera::SetFOV(float sin, float cos) { s_fov = (float)((double)(360.f * sin) / ((double)cos * 3.14159265358979323846)); l_bCalcProjection = true; }
void SceneCamera::SetFOV2(float fov) { s_fov = std::clamp(fov, 0.f, 78.f); l_bCalcProjection = true; }
void SceneCamera::SetScaleW(float w) { s_scaleW = w; l_bCalcProjection = true; }
void SceneCamera::SetNear(float n) { s_nearClip = n; l_bCalcProjection = true; }
void SceneCamera::SetFar(float f) { s_farClip = f; l_bCalcProjection = true; }
void SceneCamera::SetViewAndCenterY(Rect view, float yOffset) {
  l_sceneH = 1.f;
  l_wideBattle = false;
  if (view.height != 1.f && Layout::Wide() && Layout::GetBattleLayout() != Layout::BattleLayout::Band) {
    l_wideBattle = true;
    l_battleFrames = 0; l_fitZoom = 1.f; l_ext1W = l_ext1H = 0.f;   // g_fit* are re-seeded by the first measurement
    // battle camera (DS 4:3 band, black bars): in landscape the 3D view takes the whole surface at the same pixel scale (the projection continues the DS frustum
    // up/down and to the sides, like DQIV-Recompiled's pc::wideBattleFrame); the band centre is ~ the surface centre (0.275 + 0.21 = 0.485)
    float a = Layout::kDesignAspect;
    float vy = 1.f - (1.f - view.y) * a / 0.5625f, vh = view.height * a / 0.5625f;
    yOffset += 2.f * (vy + vh * 0.5f) - 1.f;           // keep the scene where the band had it (centre of the band = 0.31 of the surface height)
    view = Rect(0.f, 0.f, 1.f, 1.f);
  } else if (view.height != 1.f) {
    float a = Layout::Aspect(l_camera->aspect());
    view.y = 1.f - (1.f - view.y) * a / 0.5625f;
    view.height = view.height * a / 0.5625f;
    l_sceneH = view.height;
  }
  { Rect wr = Layout::WorldRect(); view.x = wr.x; view.width = wr.width; }       // landscape: horizontal extent = world rect (identity in portrait)
  l_camera->rect = view;
  s_yOffset = yOffset;
  l_bCalcProjection = true;
}
void SceneCamera::SetYOffset(float yOffset) { s_yOffset = yOffset; l_bCalcProjection = true; }
void SceneCamera::SetLookDownAdjust(float angle) { s_lookDownAdjust = angle; l_bCalcProjection = true; }

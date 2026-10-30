// game/Field.cpp -- OWNER: WP5c.  DQ4 port of Field.cs (world map grid + symbols).  Unlike DQ5, the DQ4 host fills the tile UVs itself (SetIndexList: 4 UVs per quad,
// row stride = the mesh width: 25 quads, 59 when native/field_patch.cpp widened FieldData::drawBlock), and UpdatePattern animates the sea/river tiles.
#include "game/Layout.hpp"
#include "native/field_patch.hpp"
#include "game/Field.hpp"

#include <algorithm>
#include <cmath>

#include "game/Boot.hpp"
#include "game/FieldSymbol.hpp"
#include "game/MainLoop.hpp"
#include "native/native_bridge.hpp"
#include "utils/logger.h"

Camera* Field::m_camera = nullptr;
Quaternion Field::m_objRot = Quaternion::Euler(90.f, 0.f, 0.f);
Color Field::m_color = Color::white;
float Field::c_uvAdjustZero = 0.f;

namespace {
struct PatternInfo { int acc, limit; };
struct PatternCheck { int end; uint8_t index; };
}  // namespace
static const float c_oneGridSize = 0.0156f;
static int m_texGrid = 1, m_patternCounter = 0, m_stride = 25;
static float m_uvone = 0.f, m_uvAdjustBase = 0.f, m_uvAdjust = 0.f;
static std::vector<int> m_indexList;
static std::vector<uint8_t> m_patternTarget;
static int m_patternAcc[5];
static PatternInfo m_patternInfo[5] = { {0, 0}, {76, 608}, {4, 96}, {64, 512}, {4, 96} };
static PatternCheck m_patternCheck[6] = { {326, 0}, {403, 1}, {940, 2}, {1195, 0}, {1260, 3}, {1713, 4} };
static Material* m_material = nullptr;

// DQ4 Field.SystemInit
void Field::SystemInit() {
  TextAsset* textAsset = Resources::Load<TextAsset>("FieldAdjustDeviceModel");
  std::string text = textAsset ? textAsset->text() : std::string();
  c_uvAdjustZero = 0.f;
  if (Boot::s_gpuName.find("POWERVR") != std::string::npos || Boot::s_gpuName.find("SGX") != std::string::npos) c_uvAdjustZero = 0.0005f;
  if (!Boot::s_deviceModel.empty()) {
    size_t p = 0;
    while (p <= text.size()) {
      size_t q = text.find('\n', p); std::string line = text.substr(p, q == std::string::npos ? std::string::npos : q - p);
      size_t t = line.find('\t');
      if (t != std::string::npos && t > 0 && line.substr(0, t) == Boot::s_deviceModel) { c_uvAdjustZero = (float)atof(line.c_str() + t + 1); break; }
      if (q == std::string::npos) break;
      p = q + 1;
    }
  }
  if (textAsset) Resources::UnloadAsset(textAsset);
  m_material = new Material(Shader::Find("Custom/Field"));
  Object::DontDestroyOnLoad(m_material);
  m_material->color = m_color;
  l_info("Field::SystemInit: c_uvAdjustZero=%f (gpu \"%s\")", c_uvAdjustZero, Boot::s_gpuName.c_str());
}

namespace {
struct FloatingObject {
  Transform* m_transform; Vector3 m_scale;
  explicit FloatingObject(Transform* t) : m_transform(t), m_scale(t->localScale) {}
  void SetScale(float scale) { m_transform->localScale = m_scale * scale; }
};
GameObject* m_field = nullptr;
Texture2D* m_texture = nullptr;
std::vector<Vector2> m_uv;
Mesh* m_mesh = nullptr;
float m_orthoOriginalSize = 0.f;
FieldSymbol* m_symbols[114];
Material* m_symbolsMaterial = nullptr;
Vector2 m_cpos;
std::vector<FloatingObject> m_floating;
const int c_sizeW = 25, c_sizeH = 40;
bool LiveSym(int c) { return c >= 0 && c < 114 && m_symbols[c] != nullptr; }
}  // namespace

void Field::Init() {
  const int cols = NativeCall::FieldCols(NativeCall::g_fieldPatched);       // 25 (original) or 59 (landscape patch, native/field_patch.cpp)
  m_stride = cols;
  std::vector<Vector3> array(cols * c_sizeH * 4);
  std::vector<int> array2(cols * c_sizeH * 6);
  m_objRot.SetEulerAngles(Vector3(90.f, 0.f, 0.f));
  m_uv.assign(cols * c_sizeH * 4, Vector2());
  if (MainLoop::m_camera) MainLoop::m_camera->enabled = false;
  GameObject* camGo = new GameObject("FieldCamera");
  m_camera = camGo->AddComponent<Camera>();
  m_camera->orthographic = true;
  m_camera->rect = Layout::WorldRect();
  m_orthoOriginalSize = 128.f / Layout::Aspect(m_camera->aspect());
  if (Layout::Wide() && !NativeCall::g_fieldPatched)      // unpatched native FieldData::drawBlock sends 25 x 38 tiles (400 x 608 units): never show more than ~300 units across
    m_orthoOriginalSize = std::min(m_orthoOriginalSize, 150.f / m_camera->aspect());
  m_camera->orthographicSize = m_orthoOriginalSize;
  m_camera->cullingMask = 512;
  m_camera->nearClipPlane = 1.f;
  m_camera->farClipPlane = 400.f;
  m_camera->depth = 0.f;
  m_camera->clearFlags = CameraClearFlags::Depth;
  m_camera->name = "FieldCamera";
  m_camera->transform()->Rotate(90.f, 0.f, 0.f);
  m_camera->gameObject->layer = 9;
  m_texture = Resources::Load<Texture2D>("Field/field");
  { int tw = m_texture && m_texture->width ? m_texture->width : 1, th = m_texture && m_texture->height ? m_texture->height : 1;
    m_uvone = 16.f / (float)tw;
    m_texGrid = tw / 16 > 0 ? tw / 16 : 1;
    m_uvAdjust = c_uvAdjustZero;
    m_uvAdjustBase = 1.f / (float)th * 0.125f * 4.f; }
  m_field = new GameObject("Field");
  m_field->layer = 9;
  m_field->transform->localScale = Vector3::one;
  MeshFilter* mf = m_field->AddComponent<MeshFilter>();
  m_mesh = new Mesh(); mf->setMesh(m_mesh); mf->meshIsInstance = true;
  int num = 0, num2 = 0, num3 = 0, num4 = 0;
  while (num < c_sizeH) {
    int num5 = 0, num6 = 0;
    while (num5 < cols) {
      array2[num3++] = num2; array2[num3++] = num2 + 1; array2[num3++] = num2 + 3;
      array2[num3++] = num2 + 3; array2[num3++] = num2 + 1; array2[num3++] = num2 + 2;
      array[num2++].Set((float)num6, 0.f, (float)num4);
      array[num2++].Set((float)(num6 + 16), 0.f, (float)num4);
      array[num2++].Set((float)(num6 + 16), 0.f, (float)(num4 - 16));
      array[num2++].Set((float)num6, 0.f, (float)(num4 - 16));
      num5++; num6 += 16;
    }
    num++; num4 -= 16;
  }
  m_mesh->vertices = array;
  m_mesh->uv = m_uv;
  m_mesh->SetSubMeshCount(1);
  m_mesh->SetTriangles(array2, 0);
  m_mesh->MarkDynamic();
  m_color = Color::white;
  m_material->color = m_color;
  m_material->mainTexture = m_texture;
  m_field->AddComponent<MeshRenderer>()->setMaterial(m_material);
  for (auto& s : m_symbols) s = nullptr;
  m_symbolsMaterial = new Material(Shader::Find("Custom/FieldSymbol"));
  m_symbolsMaterial->mainTexture = m_texture;
  m_patternCounter = 0;
  m_patternTarget.assign((size_t)cols * c_sizeH, 0);
  m_indexList.assign((size_t)cols * c_sizeH, 0);
  for (int& a : m_patternAcc) a = 0;
  l_info("Field::Init: aspect %.3f, ortho size %.1f, texture %s", m_camera->aspect(), m_orthoOriginalSize, m_texture ? "ok" : "MISSING");
}

void Field::SetSymbol(int ctrl, int picid, float x, float y, float alpha) {
  if (ctrl < 0 || ctrl >= 114 || !m_symbolsMaterial) return;
  if (!m_symbols[ctrl]) { l_info("Field::SetSymbol new ctrl %d pic %d at %.1f %.1f", ctrl, picid, x, y); m_symbols[ctrl] = new FieldSymbol(picid, x, y, m_symbolsMaterial); }
  m_symbols[ctrl]->SetAlpha(alpha);
}
void Field::SetSymbolVisible(int ctrl, bool bShow) { if (LiveSym(ctrl)) m_symbols[ctrl]->m_symbol->SetActive(bShow); }
void Field::SetSymbolScale(int ctrl, float scale) { if (LiveSym(ctrl)) m_symbols[ctrl]->m_symbol->transform->localScale = Vector3(scale, 1.f, scale); }
void Field::SetSymbolPrio(int ctrl, float prio) {
  if (!LiveSym(ctrl)) return;
  Transform* t = m_symbols[ctrl]->m_symbol->transform;
  Vector3 position = t->position(); position.y = prio; t->setPosition(position);
}

void Field::Release() {
  for (auto& s : m_symbols) if (s) { s->Release(); delete s; s = nullptr; }
  if (m_symbolsMaterial) { Object::Destroy(m_symbolsMaterial); m_symbolsMaterial = nullptr; }
  if (m_field) {
    Object::Destroy(m_field->GetComponent<MeshRenderer>());
    if (m_material) m_material->mainTexture = nullptr;
    if (m_mesh) { Object::Destroy(m_mesh); m_mesh = nullptr; }
    Object::Destroy(m_field->GetComponent<MeshFilter>());
    if (m_texture) { Resources::UnloadAsset(m_texture); m_texture = nullptr; }
    Object::Destroy(m_field); m_field = nullptr;
  }
  if (m_camera) { Object::Destroy(m_camera->gameObject); m_camera = nullptr; }
  m_floating.clear();
  m_uv.clear();
  if (MainLoop::m_camera) MainLoop::m_camera->enabled = true;
}

void Field::SetPosition(float x, float y) {
  if (!m_camera) return;
  m_cpos.Set(x, y);
  float sx = 0.f;                                       // landscape: slide the map left while a menu covers the panel (Layout::ShiftNdc <= 0)
  if (Layout::Wide() && Layout::ShiftNdc() != 0.f) sx = -Layout::ShiftNdc() * m_camera->orthographicSize * m_camera->aspect();
  m_camera->transform()->setPosition(Vector3(x + sx, 200.f, 0.f - y));
}

void Field::SetColor(Color color) {
  m_color = color;
  if (m_material) m_material->color = color;
  for (FieldSymbol* s : m_symbols) if (s) s->SetColor(color);
}

void Field::SetScale(float scale) {
  if (!m_camera) return;
  { static float last = -1.f; if (std::fabs(scale - last) > 0.02f) { last = scale; l_info("Field::SetScale %.3f", scale); } }
  m_camera->orthographicSize = m_orthoOriginalSize * scale;
  for (FloatingObject& item : m_floating) item.SetScale(scale);
  m_uvAdjust = scale > 1.f ? m_uvAdjustBase : c_uvAdjustZero;
}

void Field::SetFloatingObject(GameObject* target) { if (target) m_floating.emplace_back(target->transform); }
void Field::RemoveFloatingObject(GameObject* target) {
  for (auto it = m_floating.begin(); it != m_floating.end(); ++it)
    if (target && it->m_transform == target->transform) { it->SetScale(1.f); m_floating.erase(it); break; }
}

static uint8_t GetPatternIndex(int index) {
  for (int i = 0; i < 6; i++) if (index < m_patternCheck[i].end) return m_patternCheck[i].index;
  return 0;
}

void Field::SetIndexList(int startX, int startY, int width, const std::vector<int>& index1d) {
  if (!m_field || !m_mesh || width <= 0) return;
  int num = (int)index1d.size() / width;
  { static bool logged; if (!logged) { logged = true; l_info("Field::SetIndexList: startX %d startY %d width %d rows %d (mesh %d cols, patched=%d)", startX, startY, width, num, m_stride, (int)NativeCall::g_fieldPatched); } }
  m_field->transform->localPosition = Vector3((float)(startX * 16), -50.f, (float)(startY * -16));
  std::fill(m_indexList.begin(), m_indexList.end(), 0);
  std::fill(m_patternTarget.begin(), m_patternTarget.end(), (uint8_t)0);
  const int cap = (int)m_indexList.size();
  int num2 = num, num3 = 0, num4 = 0;
  while (num2 > 0) {
    int num5 = width, num6 = num3;
    while (num5 > 0) {
      int num7 = index1d[num4++];
      uint8_t patternIndex = GetPatternIndex(num7);
      if (num6 < cap) { m_indexList[num6] = num7; m_patternTarget[num6] = patternIndex; }
      num7 += m_patternAcc[patternIndex];
      int num8 = num6 << 2;
      float num9, num10;
      float new_x = c_oneGridSize + (num9 = m_uvone * (float)(num7 % m_texGrid));
      float new_y = c_oneGridSize + (num10 = m_uvone * (float)(num7 / m_texGrid));
      num9 += m_uvAdjust; num10 += m_uvAdjust;
      if (num8 + 3 < (int)m_uv.size()) {
        m_uv[num8++].Set(num9, num10); m_uv[num8++].Set(new_x, num10); m_uv[num8++].Set(new_x, new_y); m_uv[num8].Set(num9, new_y);
      }
      num5--; num6++;
    }
    num2--; num3 += m_stride;
  }
  m_mesh->uv = m_uv;
  m_mesh->SetDirty();
}

void Field::UpdatePattern() {
  m_patternCounter++;
  if (!(m_camera != nullptr) || m_patternCounter <= 16) return;
  m_patternCounter = 0;
  for (int i = 1; i < 5; i++) {
    int num = m_patternAcc[i];
    num += m_patternInfo[i].acc;
    if (num >= m_patternInfo[i].limit) num = 0;
    m_patternAcc[i] = num;
  }
  const int cap = (int)m_indexList.size();
  for (int j = 0; j < cap; j++) {
    int num2 = m_patternTarget[j];
    if (num2 != 0) {
      int num3 = m_indexList[j] + m_patternAcc[num2];
      int num4 = j << 2;
      float num5, num6;
      float new_x = c_oneGridSize + (num5 = m_uvone * (float)(num3 % m_texGrid));
      float new_y = c_oneGridSize + (num6 = m_uvone * (float)(num3 / m_texGrid));
      num5 += m_uvAdjust; num6 += m_uvAdjust;
      if (num4 + 3 < (int)m_uv.size()) {
        m_uv[num4++].Set(num5, num6); m_uv[num4++].Set(new_x, num6); m_uv[num4++].Set(new_x, new_y); m_uv[num4].Set(num5, new_y);
      }
    }
  }
  m_mesh->uv = m_uv;
  m_mesh->SetDirty();
}

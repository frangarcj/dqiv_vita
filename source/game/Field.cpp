// game/Field.cpp -- OWNER: WP5c.  Port of Field.cs (world map grid + symbols).  FieldSet() is the native libdq4core function (PowerVR half-texel bias chosen by the
// deviceInfo given to DevInit): it fills the 4000 UVs (4 per quad, 25 quads per row).
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

// DQ4 Field.SystemInit (the Custom/Field material creation of the C# is still done by Init(): TODO(dq4) Field port).
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
Material* m_material = nullptr;
std::vector<Vector2> m_uv;
Mesh* m_mesh = nullptr;
float m_orthoOriginalSize = 0.f;
FieldSymbol* m_symbols[114];
Material* m_symbolsMaterial = nullptr;
Vector2 m_cpos;
bool m_bScaling = false;
std::vector<FloatingObject> m_floating;
const int c_sizeW = 25, c_sizeH = 40;
bool LiveSym(int c) { return c >= 0 && c < 114 && m_symbols[c] != nullptr; }
}  // namespace

void Field::Init() {
  const int cols = NativeCall::FieldCols(NativeCall::g_fieldPatched);       // 25 (original) or 59 (landscape patch, native/field_patch.cpp)
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
  m_camera->farClipPlane = 500.f;
  m_camera->depth = 0.f;
  m_camera->clearFlags = CameraClearFlags::Depth;
  m_camera->name = "FieldCamera";
  m_camera->transform()->Rotate(90.f, 0.f, 0.f);
  m_camera->gameObject->layer = 9;
  m_texture = Resources::Load<Texture2D>("Field/field");
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
  m_material = new Material(Shader::Find("Custom/Field"));
  m_material->mainTexture = m_texture;
  m_field->AddComponent<MeshRenderer>()->setMaterial(m_material);
  for (auto& s : m_symbols) s = nullptr;
  m_symbolsMaterial = new Material(Shader::Find("Custom/FieldSymbol"));
  m_symbolsMaterial->mainTexture = m_texture;
  l_info("Field::Init: aspect %.3f, ortho size %.1f, texture %s", m_camera->aspect(), m_orthoOriginalSize, m_texture ? "ok" : "MISSING");
}

void Field::SetSymbol(int ctrl, int picid, float x, float y, float alpha) {
  if (ctrl < 0 || ctrl >= 114 || !m_symbolsMaterial) return;
  if (!m_symbols[ctrl]) { l_info("Field::SetSymbol new ctrl %d pic %d at %.1f %.1f", ctrl, picid, x, y); m_symbols[ctrl] = new FieldSymbol(picid, x, y, m_symbolsMaterial); }
  else if (ctrl == 24) m_symbols[ctrl]->SetUV(picid);
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
    if (m_material) { Object::Destroy(m_material); m_material = nullptr; }
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
  m_camera->transform()->setPosition(Vector3(x + sx, 400.f, 0.f - y));
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
  m_bScaling = scale > 1.f;
}

void Field::SetFloatingObject(GameObject* target) { if (target) m_floating.emplace_back(target->transform); }
void Field::RemoveFloatingObject(GameObject* target) {
  for (auto it = m_floating.begin(); it != m_floating.end(); ++it)
    if (target && it->m_transform == target->transform) { it->SetScale(1.f); m_floating.erase(it); break; }
}

void Field::SetIndexList(int startX, int startY, int width, const std::vector<int>& index1d) {
  if (!m_field || !m_mesh || width <= 0) return;
  int y_size = (int)index1d.size() / width;
  { static bool logged; if (!logged) { logged = true; l_info("Field::SetIndexList: startX %d startY %d width %d rows %d (mesh %d cols, patched=%d)", startX, startY, width, y_size, NativeCall::FieldCols(NativeCall::g_fieldPatched), (int)NativeCall::g_fieldPatched); } }
  if (m_bScaling) width |= 0x10000000;
  m_field->transform->localPosition = Vector3((float)(startX * 16), -50.f, (float)(startY * -16));
  if (NativeCall::FieldSet && NativeCall::IsBound()) {
    NativeCall::FieldSet(width, y_size, const_cast<int*>(index1d.data()), reinterpret_cast<float*>(m_uv.data()));
  } else {
    static bool warned; if (!warned) { warned = true; l_warn("Field::SetIndexList: native FieldSet not bound, tile UVs stay empty"); }
  }
  m_mesh->uv = m_uv;
  m_mesh->SetDirty();
}

void Field::UpdatePattern() {
  // TODO(dq4): port Field.UpdatePattern (m_patternCounter / m_patternInfo / m_patternAcc / m_patternTarget; part of the DQ4 Field delta, 249 lines)
}

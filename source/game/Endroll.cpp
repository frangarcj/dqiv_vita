// game/Endroll.cpp -- OWNER: WP6. Line-by-line port of Endroll.cs + EndrollPort.cs.
#include "game/Endroll.hpp"

#include <cfloat>
#include <cstdio>
#include <cstdlib>

#include "game/Global.hpp"
#include "game/MainLoop.hpp"
#include "game/MenuParts.hpp"
#include "game/SceneCamera.hpp"
#include "utils/logger.h"

namespace {
const int anim[62] = { 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 2, 0, 2, 1, 0, 1, 0, 1, 0, 0, 1, 1, 2, 0, 1, 0, 0,
                       1, 0, 2, 1, 2, 0, 1, 2, 0, 1, 0, 0, 1, 0, 1, 0, 2, 1, 2, 2, 0, 0 };
const int anim_start[6] = { 0, 4, 9, 13, 18, 24 };

// BinaryReader over a TextAsset (little endian)
struct Reader {
  const std::vector<uint8_t>& b; size_t p = 0;
  explicit Reader(const std::vector<uint8_t>& bytes) : b(bytes) {}
  unsigned short U16() { unsigned short v = p + 1 < b.size() ? (unsigned short)(b[p] | (b[p + 1] << 8)) : 0; p += 2; return v; }
  unsigned U32() { unsigned lo = U16(), hi = U16(); return lo | (hi << 16); }
};
char* name2(char* buf, size_t n, const char* base, int idx, const char* ext) { snprintf(buf, n, "endroll/Retro5_endroll_en_%02d%s%s", idx, base, ext); return buf; }
}  // namespace

void Endroll::Setup(MenuPort* port, int index) {
  m_port = port;
  m_index = index;
  char path[96];
  TextAsset* textAsset = Resources::Load<TextAsset>(name2(path, sizeof(path), "", index + 1, ".bin"));
  if (!textAsset) { l_error("Endroll: %s missing", path); return; }
  Reader br(textAsset->bytes);
  unsigned short num = br.U16();
  m_width = br.U16();
  m_height = br.U16();
  m_scale = 256.f / (float)(int)m_width;
  m_counter = m_scale * (float)(int)m_height - POSITIVE_H * 2.f;
  MeshFilter* meshFilter = gameObject->AddComponent<MeshFilter>();
  Mesh* mesh = meshFilter->mesh();
  MeshRenderer* meshRenderer = gameObject->AddComponent<MeshRenderer>();
  m_material = new Material(MenuParts::l_fontShader);
  m_texture = Resources::Load<Texture2D>(name2(path, sizeof(path), "", index + 1, ""));
  TextAsset* textAsset2 = nullptr;
  Reader* br2 = nullptr;
  unsigned short num2 = 0, num3 = 0;
  if (index == 5) {
    textAsset2 = Resources::Load<TextAsset>("endroll/Retro5_endroll_en_06_2.bin");
    m_texture_2 = Resources::Load<Texture2D>("endroll/Retro5_endroll_en_06_2");
    m_material_2 = new Material(MenuParts::l_fontShader);
    m_material_2->mainTexture = m_texture_2;
    m_material_2->color = Color::white;
    br2 = new Reader(textAsset2->bytes);
    num3 += (unsigned short)(br2->U32() & 0xFFFF);
    num2 += br2->U16();
    m_counter += m_scale * (float)(int)num2;
    m_uol.Adds(m_material_2);
  }
  int num4 = num + num3;
  m_petal.assign(num4, 0.f);
  std::vector<Vector3> array(num4 << 2);
  std::vector<Vector2> array2(num4 << 2);
  std::vector<Color32> array3(num4 << 2);
  int width = m_texture ? m_texture->width : 1, height = m_texture ? m_texture->height : 1;
  int num5 = m_width >> 1;
  float num7 = 1.f / (float)width, num8 = 1.f / (float)height;
  auto quad = [&](int q, unsigned short flags, unsigned short w, unsigned short h, unsigned short x, unsigned short y, unsigned short u, unsigned short v) {
    int n = q << 2;
    array[n] = Vector3((float)(x - num5), (float)-y, 0.f);
    array[n + 1] = Vector3((float)(x + w - num5), (float)-y, 0.f);
    array[n + 2] = Vector3((float)(x + w - num5), (float)-(y + h), 0.f);
    array[n + 3] = Vector3((float)(x - num5), (float)-(y + h), 0.f);
    array2[n] = Vector2((float)(int)u * num7, (float)(height - v) * num8);
    array2[n + 1] = Vector2((float)(u + w) * num7, (float)(height - v) * num8);
    array2[n + 2] = Vector2((float)(u + w) * num7, (float)(height - (v + h)) * num8);
    array2[n + 3] = Vector2((float)(int)u * num7, (float)(height - (v + h)) * num8);
    if ((flags & 1) != 0) m_petal[m_nPetal++] = m_scale * (float)(int)y - POSITIVE_H;
    array3[n] = ((flags & 2) == 0) ? Color32(255, 255, 255, 255) : Color32(94, 255, 255, 255);
    array3[n + 1] = array3[n]; array3[n + 2] = array3[n]; array3[n + 3] = array3[n];
  };
  for (unsigned short i = 0; i < num; i++) {
    unsigned short f = br.U16(), w = br.U16(), h = br.U16(), x = br.U16(), y = br.U16(), u = br.U16(), v = br.U16();
    quad(i, f, w, h, x, y, u, v);
  }
  Resources::UnloadAsset(textAsset);
  if (index == 5) {
    for (int i = num; i < num4; i++) {
      unsigned short f = br2->U16(), w = br2->U16(), h = br2->U16(), x = br2->U16();
      unsigned short y = (unsigned short)(br2->U16() + m_height);
      unsigned short u = br2->U16(), v = br2->U16();
      quad(i, f, w, h, x, y, u, v);
    }
    Resources::UnloadAsset(textAsset2);
    delete br2;
  }
  const int array4[6] = { 0, 1, 2, 0, 2, 3 };
  std::vector<int> array5(num * 6), array6(num3 * 6);
  for (int i = 0; i < num; i++) for (int j = 0; j < 6; j++) array5[i * 6 + j] = array4[j] + (i << 2);
  for (int i = 0; i < num3; i++) for (int j = 0; j < 6; j++) array6[i * 6 + j] = array4[j] + ((num + i) << 2);
  mesh->vertices = array;
  mesh->uv = array2;
  mesh->colors32 = array3;
  mesh->SetSubMeshCount(2);
  mesh->SetTriangles(array5, 0);
  mesh->SetTriangles(array6, 1);
  m_material->mainTexture = m_texture;
  m_material->color = Color::white;
  meshRenderer->setMaterials(std::vector<Material*>{ m_material, m_material_2 });
  gameObject->layer = 11;
  transform()->localScale = Vector3::one * m_scale;
  m_uol.Adds(meshFilter, mesh, meshRenderer, m_material);
}

void Endroll::OnDestroy() {
  m_uol.Destroy();
  if (m_texture) { Resources::UnloadAsset(m_texture); m_texture = nullptr; }
  if (m_texture_2) { Resources::UnloadAsset(m_texture_2); m_texture_2 = nullptr; }
}

void Endroll::internalUpdate() {
  if ((m_exclude & 0xF) != 0 || (Global::Flag & 0x10000) != 0) return;
  transform()->Translate(0.f, 1.82f, 0.f);
  m_counter -= 1.82f;
  if ((m_exclude & 0x10) == 0 && m_counter <= 0.f) {
    m_port->Send(1);
    m_exclude |= 16;
  }
  for (int i = 0; i < m_nPetal; i++) {
    m_petal[i] -= 1.82f;
    if (m_petal[i] <= 0.f) {
      if (anim_start[m_index] + i < 62) m_port->Send(0, (short)anim[anim_start[m_index] + i]);
      else { short num = (short)(std::rand() % 3); m_port->Send(0, num); }
      m_petal[i] = FLT_MAX;
      if (m_index == 5 && i == m_nPetal - 1) { m_port->Send(1); m_exclude |= 2; }
    }
  }
}

void Endroll::SetExclude(bool exclude) {
  renderer()->enabled = !exclude;
  if (exclude) m_exclude |= 1; else m_exclude &= -2;
}

// ------------------------------------------------------------------------------------------------ EndrollPort
std::map<Camera*, EndrollPort::CameraConf> EndrollPort::s_cameraConf;

EndrollPort::EndrollPort(short port, int type) : MenuPort(port) {
  SetupCamera();
  m_drawInstance = (new GameObject())->AddComponent<Endroll>();
  m_drawInstance->Setup(this, type);
  m_drawInstance->SetExclude(true);
}
void EndrollPort::Close() {
  RestoreCamera();
  Object::Destroy(m_drawInstance->gameObject);
  m_drawInstance = nullptr;
}
void EndrollPort::NoticeReceive(Param prm) {
  if (prm[0] == 0) {
    m_drawInstance->transform()->setPosition(Vector3(0.f, Endroll::POSITIVE_H, 0.f));
    m_drawInstance->SetExclude(false);
  }
}
void EndrollPort::SetupCamera() {
  PushCamera(MainLoop::m_camera);
  MainLoop::m_camera->enabled = false;
  PushCamera(SceneCamera::s_DSSAOrthoCamera);
  Camera* camera = SceneCamera::s_DSSAOrthoCamera;
  camera->backgroundColor = Color::black;
  camera->clearFlags = CameraClearFlags::Color;
}
void EndrollPort::RestoreCamera() { PopCamera(MainLoop::m_camera); PopCamera(SceneCamera::s_DSSAOrthoCamera); }
void EndrollPort::PushCamera(Camera* camera) {
  CameraConf c;
  c.depth = camera->depth; c.enabled = camera->enabled; c.cullingMask = camera->cullingMask; c.backgroundColor = camera->backgroundColor; c.clearFlags = camera->clearFlags;
  s_cameraConf[camera] = c;
}
void EndrollPort::PopCamera(Camera* camera) {
  auto it = s_cameraConf.find(camera);
  if (it != s_cameraConf.end()) {
    const CameraConf& c = it->second;
    camera->depth = c.depth; camera->enabled = c.enabled; camera->cullingMask = c.cullingMask; camera->backgroundColor = c.backgroundColor; camera->clearFlags = c.clearFlags;
    s_cameraConf.erase(it);
  }
}

MenuPort* dqiv_CreateEndrollPort(short port, short type) { return new EndrollPort(port, type); }

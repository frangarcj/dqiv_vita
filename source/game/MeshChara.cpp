// game/MeshChara.cpp -- OWNER: WP5a (written by WP2 for the title screen: DSSA quads). Port of MeshChara.cs.
#include "game/MeshChara.hpp"
#include "game/SceneCamera.hpp"
#include "game/Field.hpp"
#include "utils/logger.h"

static const char* s_shaderList[4] = { "Custom/DssaChara", "Custom/DssaMonster0", "Custom/DssaMonster1", "Custom/DssaCharaTransparent" };
static Shader* s_shaders[4];

MeshChara::MeshChara(int no, int shader0) {
  GameObject* gameObject = new GameObject();
  m_meshFilter = gameObject->AddComponent<MeshFilter>();
  m_renderer = gameObject->AddComponent<MeshRenderer>();
  m_mesh2 = new Mesh();
  m_material = new Material(s_shaders[shader0] ? s_shaders[shader0] : Shader::Find(s_shaderList[shader0]));
  m_renderer->setMaterial(m_material);
  m_chara = gameObject;
  SetTexture(no);
}
void MeshChara::Release() {
  Object::Destroy(m_chara);       // components die with the GameObject; mesh/material objects are engine-owned leaks of a few KB (the C# destroys them explicitly)
  m_chara = nullptr; m_meshFilter = nullptr; m_renderer = nullptr; m_mesh2 = nullptr; m_material = nullptr;
}
void MeshChara::SetTexture(int no) {
  char path[32]; snprintf(path, sizeof(path), "DssaTex/m%03d", no);
  m_material->mainTexture = Resources::Load<Texture>(path);
}
void MeshChara::SetTextureOffset(float u, float v) { m_material->SetTextureOffset("_MainTex", Vector2(u, v)); }
void MeshChara::SetVisible(bool bVisible) { if (m_renderer) m_renderer->enabled = bVisible; }
void MeshChara::SetColor(Color color) { m_material->color = color; }
void MeshChara::SetShader(int shaderNo) { m_material->shader = s_shaders[shaderNo] ? s_shaders[shaderNo] : Shader::Find(s_shaderList[shaderNo]); }
void MeshChara::SetPosition(Vector3 pos) { m_chara->transform->setPosition(pos); }
void MeshChara::SetBillboard() { m_chara->transform->setRotation(SceneCamera::s_billboardRot); m_billboard_request = false; }
void MeshChara::SetFieldMode() {
  m_chara->layer = 9;
  m_chara->transform->localScale = Vector3::one * 28.8f;
  m_chara->transform->localRotation = Field::m_objRot;
}
void MeshChara::SetFieldPosition(Vector3 pos) { m_chara->transform->setPosition(Vector3(pos.x, pos.z, 0.f - pos.y)); }
void MeshChara::SystemInit() {
  for (int i = 0; i < 4; i++) { s_shaders[i] = Shader::Find(s_shaderList[i]); Object::DontDestroyOnLoad(s_shaders[i]); }
}

// game/Chara.cpp -- OWNER: WP5a. Port of Chara.cs (line by line; delegates -> flags).
#include "game/Chara.hpp"
#include "game/Field.hpp"
#include "game/SceneCamera.hpp"
#include "utils/logger.h"
#include <cstdio>

static const float s_defScale = 1.8f, c_lieDefNear = 0.27f, c_lieDefOffset = 0.194f, c_FieldScale = 51.839996f;
static const float s_vtxDepthU = 0.44677734f, s_vtxDepthD = 0.2f, s_shadowVertexY = 1.f / 160.f;
static const int c_defaultPriority = 3000;

static Mesh* s_meshList[8]; static Vector2 s_uvSizeList[8];
static Mesh* s_meshLie4 = nullptr; static Mesh* s_meshLie8 = nullptr; static Mesh* s_shadowMesh = nullptr;
static Shader* s_charaShader = nullptr; static Shader* s_charaShaderEx = nullptr; static Material* s_matCharaShadow = nullptr;
static float s_lieCamNear = c_lieDefNear; static float s_lieTiltOffsets[2];
static Quaternion s_lieBillboardRot = Quaternion::identity;
static int s_priority = c_defaultPriority;
static const int c_triangles[6] = { 0, 1, 3, 3, 1, 2 };
static Vector3 c_lieOffset[4] = { Vector3(0.f, 0.f, 0.25f), Vector3(-0.05f, 0.f, 0.f), Vector3(0.f, 0.f, -0.25f), Vector3(0.05f, 0.f, 0.f) };
static Vector3 s_FieldScale(-c_FieldScale, c_FieldScale, c_FieldScale);
static Vector3 s_shadowDefScale(0.347f, 10.f, 0.206f);
static Vector3 s_shadowDepth(0.f, 0.f, 0.25f);
static Color s_workShadowColor = Color::white;

Chara::Chara(int no) {
  m_material = new Material(s_charaShader);
  m_chara = new GameObject(); m_charaT = m_chara->transform;
  m_charaT->localScale = Vector3::one * s_defScale;
  m_chara->AddComponent<MeshFilter>();
  SetTexture(no);
  m_chara->AddComponent<MeshRenderer>()->setMaterial(m_material);
  m_shadow = new GameObject();
  m_shadow->transform->localScale = s_shadowDefScale;
  m_shadow->AddComponent<MeshFilter>()->sharedMesh = s_shadowMesh;
  m_shadow->AddComponent<MeshRenderer>()->setSharedMaterial(s_matCharaShadow);
}

void Chara::SetSleepNear(int defNear, int offset) {
  float y = offset != 0 ? (float)offset / 4096.f : c_lieDefOffset;
  s_lieCamNear = defNear != 0 ? (float)defNear / 4096.f : c_lieDefNear;
  for (Vector3& o : c_lieOffset) o.y = y;
}
void Chara::CalcLieBillboard(Quaternion camRot) {
  Vector3 e = Quaternion::Inverse(camRot).eulerAngles();
  e.y = 0.f;
  s_lieBillboardRot = Quaternion::Euler(e);
  e.z = e.x = 0.256f;
  e = s_lieBillboardRot * e;
  s_lieTiltOffsets[0] = Mathf::Abs(e.x);
  s_lieTiltOffsets[1] = Mathf::Abs(e.z);
}
void Chara::SetSleepPose(bool bSleep) {
  MeshFilter* mf = m_chara->GetComponent<MeshFilter>();
  if ((l_bSleep = bSleep)) {
    l_sleepPosition = m_charaT->position();
    l_sleepDigitalDir = 0; l_sleepRotation = Quaternion::identity;
    mf->sharedMesh = l_patternYmax != 4 ? s_meshLie8 : s_meshLie4;
  } else {
    SetPosition(l_sleepPosition);                  // l_setPosition already switched back to normal
    mf->sharedMesh = s_meshList[l_sizeType];
  }
  m_billboard_request = true;
}
void Chara::SetSleepDir(int dir) {
  l_sleepDigitalDir = (dir >> 14) & 3;
  l_sleepRotation = Quaternion::Euler(0.f, (float)(dir & 0xFFFF) / -182.04445f + 180.f, 0.f);
  m_billboard_request = true;
}
void Chara::Release() {
  if (!m_chara) return;
  Object::Destroy(m_material);
  Object::Destroy(m_chara);
  Object::Destroy(m_shadow);
  if (l_shadowMat) Object::Destroy(l_shadowMat);
  m_chara = nullptr; m_charaT = nullptr; m_material = nullptr; m_shadow = nullptr; l_shadowMat = nullptr;
}
void Chara::SetTexture(int no) {
  MeshFilter* mf = m_chara->GetComponent<MeshFilter>();
  char path[40]; snprintf(path, sizeof(path), "CharaTex/h%03d", no & 0xFFF);
  Texture2D* tex = Resources::Load<Texture2D>(path);
  m_material->shader = (no & 0x800000) ? s_charaShaderEx : s_charaShader;
  m_material->mainTexture = tex;
  m_material->renderQueue = s_priority;
  l_patternYmax = (no >> 16) & 0xF;
  l_sizeType = (no >> 20) & 7;
  mf->sharedMesh = l_bSleep ? (l_patternYmax != 4 ? s_meshLie8 : s_meshLie4) : s_meshList[l_sizeType];
}
void Chara::SetPattern(int x, int y) {
  Vector2 offset = s_uvSizeList[l_sizeType];
  offset.x *= (float)x;
  offset.y *= (float)(l_patternYmax - y - 1);
  m_material->SetTextureOffset("_MainTex", offset);
}
void Chara::SetSeparateShadow(bool b) { m_bSeparateShadow = b; }
void Chara::SetVisible(bool v) {
  m_charaT->renderer()->enabled = v;
  if (!m_bSeparateShadow) m_shadow->transform->renderer()->enabled = m_bVisibleShadow & v;
}
void Chara::SetShadowVisible(bool v) { m_shadow->transform->renderer()->enabled = (m_bVisibleShadow = v); }
void Chara::SetColor(Color c) {
  if (!l_colorWithShadow && c.a != 1.f) l_colorWithShadow = true;      // SetColorDefault -> SetColorWithShadow forever after
  if (l_colorWithShadow) SetColorWithShadow(c); else m_material->color = c;
}
void Chara::SetColorWithShadow(Color c) { m_material->color = c; SetShadowAlpha(c.a); }
void Chara::SetShadowAlpha(float a) {
  if (!l_shadowMatMade) {                                               // SetShadowAlphaDefault
    l_shadowMat = new Material(*s_matCharaShadow);
    m_shadow->transform->renderer()->setMaterial(l_shadowMat);
    l_shadowMat->renderQueue = s_priority - 1;
    l_shadowMatMade = true;
  }
  SetShadowAlphaThereafter(a);
}
void Chara::SetShadowAlphaThereafter(float a) { s_workShadowColor.a = a; l_shadowMat->color = s_workShadowColor; }
void Chara::SetPosition(Vector3 pos) {
  if (l_bSleep) { l_sleepPosition = pos; m_billboard_request = true; }  // lieSetPosition
  else m_charaT->setPosition(pos);                                        // normalSetPosition
}
void Chara::SetScale(float scale) { scale *= s_defScale; m_charaT->localScale = Vector3(scale, scale, s_defScale); }
void Chara::SetShadowPosition(Vector3 pos) { m_shadow->transform->setPosition(pos + m_charaT->rotation() * s_shadowDepth); }
void Chara::normalBillboard() {
  m_charaT->setRotation(SceneCamera::s_billboardRot);
  if (!m_bSeparateShadow) m_shadow->transform->setPosition(m_charaT->position() + m_charaT->rotation() * s_shadowDepth);
  m_shadow->transform->setRotation(BillboardObject::s_shadowRot);
}
void Chara::lieBillboard() {
  float num = s_lieTiltOffsets[l_sleepDigitalDir & 1];
  m_charaT->setPosition(l_sleepPosition + c_lieOffset[l_sleepDigitalDir]);
  m_charaT->setRotation(s_lieBillboardRot * l_sleepRotation);
  m_charaT->Translate(0.f, s_lieCamNear + num, 0.f);
  if (!m_bSeparateShadow) m_shadow->transform->setPosition(m_charaT->position() + Vector3::down * 64.f);
}
void Chara::SetBillboard() {
  if (l_bSleep) lieBillboard(); else normalBillboard();
  m_billboard_request = false;
}
void Chara::SetFieldMode() {
  m_chara->layer = 9; m_shadow->layer = 9;
  m_charaT->localRotation = Field::m_objRot;
  m_shadow->transform->localRotation = Quaternion::Euler(36.f, 0.f, 0.f);
  m_charaT->localScale = s_FieldScale;
  m_shadow->transform->localScale = s_shadowDefScale * 28.8f;
}
void Chara::SetFieldPosition(Vector3 pos) {
  Vector3 p(pos.x, pos.z, 0.f - pos.y);
  m_charaT->setPosition(p);
  if (!m_bSeparateShadow) { p.y = -1.f; m_shadow->transform->setPosition(p); }
}
void Chara::SetFieldShadowPosition(Vector3 pos) { m_shadow->transform->setPosition(Vector3(pos.x, -1.f, 0.f - pos.y)); }
void Chara::UpdateBillboardShadows() { s_shadowRot = Quaternion::Euler(0.f, s_billboardTarget->eulerAngles().y, 0.f); }

static void SetSharedMesh(int index, float w, float h, float u, float v, float pu, float pv, float v_adjust = 0.f) {
  Mesh* mesh = new Mesh();
  mesh->vertices = { Vector3(-w, 0.f, s_vtxDepthD), Vector3(w, 0.f, s_vtxDepthD), Vector3(w, h, s_vtxDepthU), Vector3(-w, h, s_vtxDepthU) };
  v -= v_adjust;
  mesh->uv = { Vector2(u, 0.f), Vector2(0.f, 0.f), Vector2(0.f, v), Vector2(u, v) };
  mesh->SetSubMeshCount(1);
  mesh->SetTriangles(c_triangles, 6, 0);
  Object::DontDestroyOnLoad(s_meshList[index] = mesh);
  s_uvSizeList[index] = Vector2(pu, pv);
}
void Chara::SystemInit() {
  s_charaShader = Shader::Find("Custom/Chara");
  s_charaShaderEx = Shader::Find("Custom/CharaEx");
  s_matCharaShadow = new Material(Shader::Find("Custom/CharaShadow"));
  s_matCharaShadow->mainTexture = Resources::Load<Texture2D>("CharaTex/common");
  s_matCharaShadow->color = Color::white;
  SetSharedMesh(0, 0.32f, 0.64f, 0.25f, 0.25f, 0.25f, 0.25f, 0.002f);
  SetSharedMesh(1, 0.32f, 0.64f, 0.25f, 0.125f, 0.25f, 0.125f, 0.001f);
  SetSharedMesh(2, 0.4f, 0.64f, 0.625f, 0.125f, 0.f, 0.125f, 0.f);
  SetSharedMesh(3, 0.32f, 0.48f, 0.125f, 3.f / 32.f, 0.125f, 0.125f, 0.f);
  SetSharedMesh(4, 0.64f, 1.28f, 0.25f, 0.125f, 0.25f, 0.125f, 0.f);
  SetSharedMesh(5, 0.4f, 0.96f, 5.f / 32.f, 0.1875f, 0.25f, 0.25f, 0.f);
  SetSharedMesh(6, 0.64f, 1.28f, 1.f, 1.f, 1.f, 1.f, 0.f);
  SetSharedMesh(7, 0.64f, 1.28f, 0.5f, 0.5f, 0.5f, 0.5f, 0.f);
  s_meshLie4 = new Mesh(); s_meshLie8 = new Mesh(); s_shadowMesh = new Mesh();
  s_meshLie4->vertices = { Vector3(-0.32f, 0.f, 0.32f), Vector3(0.32f, 0.f, 0.32f), Vector3(0.32f, 0.f, -0.32f), Vector3(-0.32f, 0.f, -0.32f) };
  s_meshLie4->uv = s_meshList[0]->uv; s_meshLie4->SetSubMeshCount(1); s_meshLie4->SetTriangles(c_triangles, 6, 0);
  s_meshLie8->vertices = s_meshLie4->vertices;
  s_meshLie8->uv = s_meshList[1]->uv; s_meshLie8->SetSubMeshCount(1); s_meshLie8->SetTriangles(c_triangles, 6, 0);
  s_shadowMesh->vertices = { Vector3(-1.f, s_shadowVertexY, 1.f), Vector3(1.f, s_shadowVertexY, 1.f), Vector3(1.f, s_shadowVertexY, -1.f), Vector3(-1.f, s_shadowVertexY, -1.f) };
  s_shadowMesh->uv = { Vector2(0.f, 0.f), Vector2(1.f, 0.f), Vector2(1.f, 1.f), Vector2(0.f, 1.f) };
  s_shadowMesh->SetSubMeshCount(1); s_shadowMesh->SetTriangles(c_triangles, 6, 0);
  Object::DontDestroyOnLoad(s_charaShader); Object::DontDestroyOnLoad(s_charaShaderEx); Object::DontDestroyOnLoad(s_matCharaShadow);
  Object::DontDestroyOnLoad(s_shadowMesh); Object::DontDestroyOnLoad(s_meshLie4); Object::DontDestroyOnLoad(s_meshLie8);
  s_priority = c_defaultPriority;
}
void Chara::SetPriorityShader(int priority) { s_priority = c_defaultPriority + priority; s_matCharaShadow->renderQueue = s_priority - 1; }

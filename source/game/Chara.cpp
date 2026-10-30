// game/Chara.cpp -- OWNER: WP5a. Port of Chara.cs (line by line; delegates -> flags).
#include "game/Chara.hpp"
#include "game/Field.hpp"
#include "game/SceneCamera.hpp"
#include "utils/logger.h"
#include <cstdio>

static const float s_defScale = 1.8f, c_lieDefOffset = 0.194f, c_FieldScale = 51.839996f;
static const float s_shadowVertexY = 1.f / 160.f;

static Mesh* s_mesh4 = nullptr; static Mesh* s_mesh8 = nullptr; static Mesh* s_mesh12 = nullptr;
static Mesh* s_meshLie4 = nullptr; static Mesh* s_meshLie8 = nullptr; static Mesh* s_shadowMesh = nullptr;
static Shader* s_charaShader = nullptr; static Shader* s_charaShaderEx = nullptr; static Shader* s_charaLieShader = nullptr; static Shader* s_charaLieShaderEx = nullptr;
static Material* s_matCharaShadow = nullptr;
static Quaternion s_lieBillboardRot = Quaternion::identity;
static Vector3 s_lieOffset[4] = { Vector3(0.f, 0.f, 0.2f), Vector3(-0.1f, 0.f, 0.f), Vector3(0.f, 0.f, -0.2f), Vector3(0.1f, 0.f, 0.f) };
static Vector3 s_FieldScale(-c_FieldScale, c_FieldScale, c_FieldScale);
static Vector3 s_shadowDefScale(0.347f, 10.f, 0.206f);
static Vector3 s_shadowDepth(0.f, 0.f, 0.25f);
static Color s_workShadowColor = Color::white;
static const int s_triangles[6] = { 0, 1, 3, 3, 1, 2 };

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
  m_shadow->setActive((no != 193) & (no != 194));
}

void Chara::SetSleepNear(int defNear, int offset) {
  float y = offset != 0 ? (float)offset / 4096.f : c_lieDefOffset;
  y *= 1.6f;
  for (Vector3& o : s_lieOffset) o.y = y;
}
void Chara::CalcLieBillboard(Quaternion camRot) {
  Vector3 e = Quaternion::Inverse(camRot).eulerAngles();
  e.y = 0.f;
  s_lieBillboardRot = Quaternion::Euler(e);
}
void Chara::SetSleepPose(bool bSleep) {
  MeshFilter* component = m_chara->GetComponent<MeshFilter>();
  if (bSleep) {
    if (!l_bSleep) {                                // l_setPosition != lieSetPosition
      m_material->shader = !l_useCharaShaderEx ? s_charaLieShader : s_charaLieShaderEx;
      l_bSleep = true;                              // l_setPosition = lieSetPosition, l_setBillboard = lieBillboard
      l_sleepDigitalDir = 0;
      l_sleepRotation = Quaternion::identity;
      component->sharedMesh = l_patternYmax != 4 ? s_meshLie8 : s_meshLie4;
      SetPosition(m_charaT->position());            // l_setPosition(m_chara.position)
    }
  } else if (l_bSleep) {                            // l_setPosition != normalSetPosition
    m_material->shader = !l_useCharaShaderEx ? s_charaShader : s_charaShaderEx;
    l_bSleep = false;
    SetPosition(l_sleepPosition);
    component->sharedMesh = l_patternYmax != 4 ? s_mesh8 : s_mesh4;
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
  if (100 <= no && no <= 127) {
    l_useCharaShaderEx = true;
    m_material->shader = s_charaShaderEx;
  }
  if (no == 194) no = 193;
  char path[40]; snprintf(path, sizeof(path), "CharaTex/h%03d", no);
  Texture2D* texture2D = Resources::Load<Texture2D>(path);
  m_material->mainTexture = texture2D;
  if (no == 193) {
    MeshFilter* component = m_chara->GetComponent<MeshFilter>();
    l_patternYmax = 0;
    l_uChara = 5.f / 64.f;
    component->sharedMesh = s_mesh12;
    m_vPatternSize = 5.f / 64.f;
    l_vAdjust = -2.f / 1024.f;
    return;
  }
  int num = l_patternYmax;
  l_patternYmax = texture2D && texture2D->width ? 4 * texture2D->height / texture2D->width : 0;
  l_uChara = 0.1875f;
  if (num != l_patternYmax) {
    MeshFilter* component2 = m_chara->GetComponent<MeshFilter>();
    if (l_patternYmax == 4) {
      component2->sharedMesh = s_mesh4;
      m_vPatternSize = 0.25f;
      l_vAdjust = -1.f / 128.f;
    } else {
      component2->sharedMesh = s_mesh8;
      m_vPatternSize = 0.125f;
      l_vAdjust = -1.f / 256.f;
    }
  }
}
void Chara::SetPattern(int x, int y) {
  l_uvOffset = Vector2(l_uChara * (float)x, m_vPatternSize * (float)(l_patternYmax - y - 1) + l_vAdjust);
  m_material->SetTextureOffset("_MainTex", l_uvOffset);
}
void Chara::SetSeparateShadow(bool b) { m_bSeparateShadow = b; }
void Chara::SetVisible(bool v) {
  m_charaT->renderer()->enabled = v;
  if (!m_bSeparateShadow) m_shadow->transform->renderer()->enabled = v;
}
void Chara::SetShadowVisible(bool v) { m_shadow->transform->renderer()->enabled = v; }
void Chara::SetColor(Color c) {
  if (!l_colorWithShadow && c.a != 1.f) l_colorWithShadow = true;      // SetColorDefault -> SetColorWithShadow forever after
  if (l_colorWithShadow) SetColorWithShadow(c); else m_material->color = c;
}
void Chara::SetColorWithShadow(Color c) { m_material->color = c; SetShadowAlpha(c.a); }
void Chara::SetShadowAlpha(float a) {
  if (!l_shadowMatMade) {                                               // SetShadowAlphaDefault
    l_shadowMat = new Material(*s_matCharaShadow);
    m_shadow->transform->renderer()->setMaterial(l_shadowMat);
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
  Vector3 vector = l_sleepPosition + s_lieOffset[l_sleepDigitalDir];
  m_charaT->setPosition(vector);
  m_charaT->setRotation(s_lieBillboardRot * l_sleepRotation);
  vector.y += 0.1f;
  m_material->SetMatrix("_sleepMatrix", Matrix4x4::TRS(vector, l_sleepRotation, m_charaT->localScale));
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

static Mesh* MakeMesh() { Mesh* m = new Mesh(); return m; }
void Chara::SystemInit() {
  s_charaShader = Shader::Find("Custom/Chara");
  s_charaLieShader = Shader::Find("Custom/CharaSleep");
  s_charaShaderEx = Shader::Find("Custom/CharaEx");
  s_charaLieShaderEx = Shader::Find("Custom/CharaSleepEx");
  s_matCharaShadow = new Material(Shader::Find("Custom/CharaShadow"));
  s_matCharaShadow->mainTexture = Resources::Load<Texture2D>("CharaTex/common");
  s_matCharaShadow->color = Color::white;
  s_mesh4 = MakeMesh(); s_mesh8 = MakeMesh(); s_mesh12 = MakeMesh(); s_meshLie4 = MakeMesh(); s_meshLie8 = MakeMesh(); s_shadowMesh = MakeMesh();
  s_mesh4->vertices = { Vector3(-0.24f, 0.f, 0.185f), Vector3(0.24f, 0.f, 0.185f), Vector3(0.24f, 0.64f, 0.65f), Vector3(-0.24f, 0.64f, 0.65f) };
  s_mesh4->uv = { Vector2(0.1875f, 0.f), Vector2(0.f, 0.f), Vector2(0.f, 0.25f), Vector2(0.1875f, 0.25f) };
  s_mesh4->SetSubMeshCount(1); s_mesh4->SetTriangles(s_triangles, 6, 0);
  s_meshLie4->vertices = { Vector3(-0.24f, 0.f, 0.32f), Vector3(0.24f, 0.f, 0.32f), Vector3(0.24f, 0.f, -0.32f), Vector3(-0.24f, 0.f, -0.32f) };
  s_meshLie4->uv = s_mesh4->uv; s_meshLie4->SetSubMeshCount(1); s_meshLie4->SetTriangles(s_triangles, 6, 0);
  s_mesh8->vertices = s_mesh4->vertices;
  s_mesh8->uv = { Vector2(0.1875f, 0.f), Vector2(0.f, 0.f), Vector2(0.f, 0.125f), Vector2(0.1875f, 0.125f) };
  s_mesh8->SetSubMeshCount(1); s_mesh8->SetTriangles(s_triangles, 6, 0);
  s_meshLie8->vertices = s_meshLie4->vertices;
  s_meshLie8->uv = s_mesh8->uv; s_meshLie8->SetSubMeshCount(1); s_meshLie8->SetTriangles(s_triangles, 6, 0);
  s_shadowMesh->vertices = { Vector3(-1.f, s_shadowVertexY, 1.f), Vector3(1.f, s_shadowVertexY, 1.f), Vector3(1.f, s_shadowVertexY, -1.f), Vector3(-1.f, s_shadowVertexY, -1.f) };
  s_shadowMesh->uv = { Vector2(0.f, 0.f), Vector2(1.f, 0.f), Vector2(1.f, 1.f), Vector2(0.f, 1.f) };
  s_shadowMesh->SetSubMeshCount(1); s_shadowMesh->SetTriangles(s_triangles, 6, 0);
  s_mesh12->vertices = { Vector3(-0.4f, 0.f, 0.185f), Vector3(0.4f, 0.f, 0.185f), Vector3(0.4f, 0.8f, 0.65f), Vector3(-0.4f, 0.8f, 0.65f) };
  s_mesh12->uv = { Vector2(5.f / 64.f, 0.f), Vector2(0.f, 0.f), Vector2(0.f, 5.f / 64.f), Vector2(5.f / 64.f, 5.f / 64.f) };
  s_mesh12->SetSubMeshCount(1); s_mesh12->SetTriangles(s_triangles, 6, 0);
  Object::DontDestroyOnLoad(s_charaShader); Object::DontDestroyOnLoad(s_charaLieShader); Object::DontDestroyOnLoad(s_charaShaderEx); Object::DontDestroyOnLoad(s_charaLieShaderEx);
  Object::DontDestroyOnLoad(s_matCharaShadow);
  Object::DontDestroyOnLoad(s_mesh4); Object::DontDestroyOnLoad(s_mesh8); Object::DontDestroyOnLoad(s_shadowMesh); Object::DontDestroyOnLoad(s_mesh12);
  Object::DontDestroyOnLoad(s_meshLie4); Object::DontDestroyOnLoad(s_meshLie8);
}

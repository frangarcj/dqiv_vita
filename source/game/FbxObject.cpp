// game/FbxObject.cpp -- OWNER: WP5b.  Port of FbxObject.cs (105 lines). The prefab's SkinnedMeshRenderers are skinned on the CPU by the engine (animation.cpp).
#include "game/FbxObject.hpp"

#include <cstdio>

#include "engine/engine_internal.hpp"
#include "utils/logger.h"

FbxObject::FbxObject(int no) {
  char path[32]; snprintf(path, sizeof(path), "fbx/c%04d", no);
  GameObject* prefab = Resources::Load<GameObject>(path);
  GameObject* go = prefab ? Object::Instantiate(prefab) : nullptr;
  if (!go) { Debug::LogError("FbxObject: %s not found", path); return; }
  m_obj = go->transform;
  if (no == 10) m_defScale = 0.4f;
  MaterialScan(m_materials, m_obj);
  m_obj->gameObject->SetActiveRecursively(false);
}

void FbxObject::MaterialScan(std::vector<Material*>& list, Transform* a) {
  Renderer* r = a->GetComponent<Renderer>();
  if (r) for (Material* m : r->materials()) list.push_back(m);
  for (Transform* item : a->children()) MaterialScan(list, item);
}

void FbxObject::SetScale(Vector3 scale) { if (m_obj) m_obj->localScale = scale * m_defScale; }
void FbxObject::SetPosition(Vector3 pos) { if (m_obj) m_obj->setPosition(pos); }
void FbxObject::SetRotation(Vector3 euler) { if (m_obj) m_obj->setRotation(Quaternion::Euler(euler)); }
void FbxObject::SetVisible(bool bVisible) { if (m_obj) m_obj->gameObject->SetActiveRecursively(bVisible); }
void FbxObject::SetColor(Color color) { for (Material* m : m_materials) if (miniunity::engine::IsLive(m)) m->color = color; }

void FbxObject::PlayAnimation(int seqNo, bool bForce) {
  Animation* anim = m_obj ? m_obj->GetComponent<Animation>() : nullptr;
  if (!anim) return;
  char buf[24]; snprintf(buf, sizeof(buf), "Take %03d", 1 + seqNo);
  std::string text = buf;
  if (!bForce && anim->isPlaying()) {
    if (m_mode == WrapMode::Loop) anim->CrossFade(text, m_blend_time);
    else anim->PlayQueued(text);
  } else {
    anim->Play(text);
  }
  m_mode = (*anim)[text].wrapMode;
}

bool FbxObject::IsEndAnimation() {
  Animation* anim = m_obj ? m_obj->GetComponent<Animation>() : nullptr;
  return !anim || !anim->isPlaying();
}

void FbxObject::Release() {
  if (m_obj) Object::Destroy(m_obj->gameObject);
  m_obj = nullptr;
  m_materials.clear();
}

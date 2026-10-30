// game/FldObject.cpp -- OWNER: WP5b.  Port of re/cs_out/Assembly-CSharp/FldObject.cs (2607 lines), same member order and numeric semantics (float math in the same
// order, int16 fields kept as int16_t). Cross-checks: re/arch/05_PORTING_MAP.md (FldObject row), 04_SHADERS.md section 3.2.
//
// Differences from the C# (all of them defensive, none changes a working path):
//  * every array index / null that would throw in Mono (and abort the rest of that Update in Unity) is guarded instead,
//  * Texture2D.format == RGBA32 is tested on Texture2D::origFormat (the pack may store an RGBA32 source as RGBA4444),
//  * Material.renderQueue == 3000 is tested on the effective queue (custom queue, else the shader's default),
//  * `mesh` reads use sharedMesh (no per-model instance copy of the mesh just to look at colours / vertices),
//  * Material / Mesh pointers kept across frames are checked with engine::IsLive() before use (instances can be swept after UnloadUnusedAssets).
#include "game/Layout.hpp"
#include "game/FldObject.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "engine/engine_internal.hpp"
#include "game/ChunkFile.hpp"
#include "utils/logger.h"

using miniunity::engine::IsLive;

namespace {
inline bool Live(Object* o) { return o && IsLive(o) && !o->destroyed; }
std::string D3(int v) { char b[16]; snprintf(b, sizeof(b), "%03d", v); return b; }
Renderer* RendererOf(Transform* t) { return t ? t->GetComponent<Renderer>() : nullptr; }
std::vector<Material*> MaterialsOf(Transform* t) { Renderer* r = RendererOf(t); return r ? r->materials() : std::vector<Material*>(); }
int EffectiveQueue(Material* m) { return m->renderQueue >= 0 ? m->renderQueue : (m->shader ? m->shader->DefaultQueue() : 2000); }
void SetSampler(Texture* t, FilterMode f, TextureWrapMode w, bool setWrap) {
  if (!t) return;
  bool changed = t->filterMode != f || (setWrap && t->wrapMode != w);
  t->filterMode = f; if (setWrap) t->wrapMode = w;
  if (changed) if (Texture2D* t2 = dynamic_cast<Texture2D*>(t)) if (t2->glId) t2->ApplySampler();
}
bool TexNameHas(Texture* t, const char* s) { return t && t->name.find(s) != std::string::npos; }
}  // namespace

float FldObject::s_skyStretch = 1.f;

// ------------------------------------------------------------------------------------------------ small data types
std::shared_ptr<FldObject::FLD_MOTION> FldObject::FLD_MOTION::Clone() const {
  auto m = std::make_shared<FLD_MOTION>();
  m->flag = flag; m->nkey = nkey; m->repeat_num = repeat_num; m->key_size = key_size; m->cur_frame = cur_frame; m->end_frame = end_frame;
  m->obj_id = obj_id; m->_start_delay = _start_delay; m->_repeat_num = _repeat_num; m->key = key;
  return m;
}

FldObject::FldObject() { m_scale = Vector3::one; m_color = Color::white; }
FldObject::~FldObject() {}

Quaternion FldObject::rotateXZY(Vector3 rot) {
  Quaternion quaternion = Quaternion::AngleAxis(0.f - rot.x, Vector3::right);
  Quaternion quaternion2 = Quaternion::AngleAxis(rot.y, Vector3::up);
  Quaternion quaternion3 = Quaternion::AngleAxis(0.f - rot.z, Vector3::forward);
  return quaternion2 * quaternion3 * quaternion;
}

// ------------------------------------------------------------------------------------------------ cameras
bool FldObject::SetCameraNo(int no, int idx) {
  FLD_MOTION* mot = nullptr;
  if (!m_emv_mot || no > (int)m_emv_mot->size() || no < 0) {
    SetCameraPos(0.f, 0.f, 0.f, idx); SetCameraCent(0.f, 0.f, 1.f, idx); SetCameraUp(0.f, 1.f, 0.f, idx);
    return false;
  }
  m_camera_no[idx] = no;
  if (no > 0) mot = (*m_emv_mot)[no - 1].get();
  if (mot && mot->repeat_num != 0 && mot->nkey != 0 && mot->key && !mot->key->empty()) {
    mot->repeat_num = (int16_t)mot->_repeat_num;
    mot->start_delay = (int16_t)mot->_start_delay;
    mot->cur_frame = 0;
    s_camera_idx = idx;
    emv_func((*mot->key)[0].get(), (*mot->key)[0].get(), 0.f);
  } else {
    SetCameraPos(0.f, 0.f, 0.f, idx); SetCameraCent(0.f, 0.f, 1.f, idx); SetCameraUp(0.f, 1.f, 0.f, idx);
  }
  return true;
}
void FldObject::SetCameraPos(float x, float y, float z, int idx) { m_x32_camera_pos[idx] = Vector3(x, y, z); }
void FldObject::SetCameraCent(float x, float y, float z, int idx) { m_x32_camera_tag[idx] = Vector3(x, y, z); }
void FldObject::SetCameraUp(float x, float y, float z, int idx) { m_x32_camera_vec[idx] = Vector3(x, y, z); }
void FldObject::SetCameraPos(Vector3 pos, int idx) { m_x32_camera_pos[idx] = pos; }
void FldObject::SetCameraCent(Vector3 cent, int idx) { m_x32_camera_tag[idx] = cent; }
void FldObject::SetCameraUp(Vector3 up, int idx) { m_x32_camera_vec[idx] = up; }

// ------------------------------------------------------------------------------------------------ map objects
FldObject::FLD_MAP_OBJ* FldObject::GetMapObjPtr(int obj_id) {
  if (obj_id < 0 || obj_id >= m_map.mobj || obj_id >= (int)m_obj_tbl.size()) return nullptr;
  if (m_obj_tbl[obj_id] < 0) return nullptr;
  if (m_obj_tbl[obj_id] >= (int)m_map_objs.size()) return nullptr;
  return m_map_objs[m_obj_tbl[obj_id]].get();
}
int FldObject::GetMapObjUid(int obj_id) { FLD_MAP_OBJ* o = GetMapObjPtr(obj_id); return o ? o->uid : 0; }

void FldObject::set_obj_material_alpha(FLD_MAP_OBJ* obj, float alpha) {
  Shader* shader = Shader::Find("Custom/fld_trans");
  if (btst(obj->flag, 134217728)) shader = Shader::Find("Custom/fld_transz");
  for (Material* material : MaterialsOf(obj->model)) {
    Color color = material->GetColor("_OrgColor");
    color.a = alpha;
    if (alpha < 1.f) {
      material->shader = shader;
    } else {
      material->color = color;
      set_shader(material);
    }
    material->SetColor("_OrgColor", color);
  }
}

void FldObject::SetMapObjAlpha(int obj_id, int alpha, int keep_priority) {
  FLD_MAP_OBJ* o = GetMapObjPtr(obj_id);
  if (o && o->model && alpha >= 0 && alpha <= 31) {
    o->alpha = alpha;
    set_obj_material_alpha(o, (float)alpha / 31.f);
    if (keep_priority != 0) bset(o->flag, 67108864);
  }
}
void FldObject::SetMapObjOnOff(int obj_id, int flag) {
  FLD_MAP_OBJ* o = GetMapObjPtr(obj_id);
  if (!o) return;
  if (flag == 0) { bclr(o->flag, 33554432); obj_visible(o, true); }
  else { bset(o->flag, 33554432); obj_visible(o, false); }
}
void FldObject::SetMapObjPos(int obj_id, Vector3 pos) { FLD_MAP_OBJ* o = GetMapObjPtr(obj_id); if (o) { o->pos = pos; o->dirty |= 1; } }
void FldObject::SetMapObjRot(int obj_id, Vector3 rot) { FLD_MAP_OBJ* o = GetMapObjPtr(obj_id); if (o) { o->rot = rot; o->dirty |= 4; } }
void FldObject::SetMapObjScl(int obj_id, Vector3 scl) { FLD_MAP_OBJ* o = GetMapObjPtr(obj_id); if (o) { o->scl = scl; o->dirty |= 2; } }
Vector3 FldObject::GetMapObjPos(int obj_id) { FLD_MAP_OBJ* o = GetMapObjPtr(obj_id); return o ? o->pos : Vector3::zero; }
Vector3 FldObject::GetMapObjRot(int obj_id) { FLD_MAP_OBJ* o = GetMapObjPtr(obj_id); return o ? o->rot : Vector3::zero; }
Vector3 FldObject::GetMapObjScl(int obj_id) { FLD_MAP_OBJ* o = GetMapObjPtr(obj_id); return o ? o->scl : Vector3::zero; }
Vector3 FldObject::GetMapObjDispPos(int obj_id) { FLD_MAP_OBJ* o = GetMapObjPtr(obj_id); return o && o->model ? o->model->position() : Vector3::zero; }
Vector3 FldObject::GetMapObjDispRot(int obj_id) { FLD_MAP_OBJ* o = GetMapObjPtr(obj_id); return o && o->model ? o->model->rotation().eulerAngles() : Vector3::zero; }
Vector3 FldObject::GetMapObjDispScl(int obj_id) { FLD_MAP_OBJ* o = GetMapObjPtr(obj_id); return o && o->model ? o->model->localScale : Vector3::zero; }

int FldObject::GetMapObjFrame(int obj_id) {
  FLD_MAP_OBJ* o = GetMapObjPtr(obj_id);
  int result = 0;
  if (o) {
    if (m_pmv_mot && o->pmv_id != -1 && o->pmv_id < (int)m_pmv_mot->size()) result = (*m_pmv_mot)[o->pmv_id]->cur_frame;
    else if (m_has_seq_obj && o->seq_obj_id != -1 && o->seq_obj_id < (int)m_seq_obj.size()) result = m_seq_obj[o->seq_obj_id].cur_frame;
  }
  return result;
}
int FldObject::GetMapObjEndFrame(int obj_id) {
  FLD_MAP_OBJ* o = GetMapObjPtr(obj_id);
  int result = 0;
  if (o) {
    if (m_pmv_mot && o->pmv_id != -1 && o->pmv_id < (int)m_pmv_mot->size()) result = (*m_pmv_mot)[o->pmv_id]->end_frame;
    else if (m_has_seq_obj && o->seq_obj_id != -1 && o->seq_obj_id < (int)m_seq_obj.size()) {
      const FLD_SEQ_OBJ& so = m_seq_obj[o->seq_obj_id];
      if (so.seq_id >= 0 && so.seq_id < (int)m_seq_data.size() && so.grp_id >= 0 && so.grp_id < (int)m_seq_data[so.seq_id]->grp.size())
        result = m_seq_data[so.seq_id]->grp[so.grp_id]->end_frame;
    }
  }
  return result;
}
void FldObject::AddMapObjPos(int obj_id, Vector3 pos) { FLD_MAP_OBJ* o = GetMapObjPtr(obj_id); if (o) { o->pos = o->pos + pos; o->dirty |= 1; } }
void FldObject::AddMapObjRot(int obj_id, Vector3 rot) { FLD_MAP_OBJ* o = GetMapObjPtr(obj_id); if (o) { o->rot = o->rot + rot; o->dirty |= 4; } }
void FldObject::AddMapObjScl(int obj_id, Vector3 scl) { FLD_MAP_OBJ* o = GetMapObjPtr(obj_id); if (o) { o->scl = o->scl + scl; o->dirty |= 2; } }

#define FOR_UID(uid_id, call) \
  for (int i = 0; i < m_map.mobj && i < (int)m_obj_tbl.size(); i++) if (m_obj_tbl[i] >= 0 && m_obj_tbl[i] < (int)m_map_objs.size() && m_map_objs[m_obj_tbl[i]]->uid == (uid_id)) { call; }
void FldObject::SetMapUidAlpha(int uid_id, int alpha, int keep_priority) { FOR_UID(uid_id, SetMapObjAlpha(i, alpha, keep_priority)) }
void FldObject::SetMapUidOnOff(int uid_id, int flag) { FOR_UID(uid_id, SetMapObjOnOff(i, flag)) }
void FldObject::SetMapUidPos(int uid_id, Vector3 pos) { FOR_UID(uid_id, SetMapObjPos(i, pos)) }
void FldObject::SetMapUidRot(int uid_id, Vector3 rot) { FOR_UID(uid_id, SetMapObjRot(i, rot)) }
void FldObject::SetMapUidScl(int uid_id, Vector3 scl) { FOR_UID(uid_id, SetMapObjScl(i, scl)) }
void FldObject::AddMapUidPos(int uid_id, Vector3 pos) { FOR_UID(uid_id, AddMapObjPos(i, pos)) }
void FldObject::AddMapUidRot(int uid_id, Vector3 rot) { FOR_UID(uid_id, AddMapObjRot(i, rot)) }
void FldObject::AddMapUidScl(int uid_id, Vector3 scl) { FOR_UID(uid_id, AddMapObjScl(i, scl)) }
#undef FOR_UID

void FldObject::SetVanishFlag(int flag) { m_vanish_flag = flag; }

// ------------------------------------------------------------------------------------------------ model replacement shared by SetCommonAnimation
void FldObject::DestroySeqMeshes(FLD_MAP_OBJ* o) {
  if (Live(o->seq_mesh1)) Object::Destroy(o->seq_mesh1);
  if (Live(o->seq_mesh2)) Object::Destroy(o->seq_mesh2);
  o->seq_mesh1 = o->seq_mesh2 = nullptr;
}
void FldObject::MakeSeqMeshes(FLD_MAP_OBJ* o) {
  MeshFilter* mf = o->model ? o->model->GetComponent<MeshFilter>() : nullptr;
  if (!mf || !mf->sharedMesh) return;
  o->seq_mesh1 = Object::Instantiate(mf->sharedMesh); o->seq_mesh2 = Object::Instantiate(mf->sharedMesh);
  if (o->seq_mesh1) o->seq_mesh1->engineOwned = false;           // referenced from FldObject only: must not be swept (flip-flop leaves one unreferenced by the scene)
  if (o->seq_mesh2) o->seq_mesh2->engineOwned = false;
}
// Instantiate the `.fld` child `mdl_id` at the object's position/rotation in place of the current model (same parent and name).
void FldObject::ReplaceModel(FLD_MAP_OBJ* o, int mdl_id) {
  if (!m_fld_model || mdl_id < 0 || mdl_id >= m_fld_model->GetChildCount()) return;
  Transform* child = m_fld_model->GetChild(mdl_id);
  Quaternion rotation = rotateXZY(o->rot);
  Transform* transform = Object::Instantiate(child, o->pos, rotation);
  if (!transform) return;
  if (o->model) {
    transform->SetParent(o->model->parent());
    transform->name = o->model->name;
    Object::Destroy(o->model->gameObject);
  }
  o->model = transform;
  for (Transform* item : o->model->children()) item->gameObject->setActive(false);
}

int FldObject::SetEventAnimation(int no) {
  if (!m_has_ean || no < 0 || no > (int)m_ean.size()) return 0;
  m_event_anim_id = no;
  if (no == 0) {
    int num = 0;
    int nobj = m_map.nobj;
    int i;
    for (i = num; i < nobj; i++) ChangeMapTable((int16_t)i, (int16_t)i);
    if (m_map.merge_flg) ChangeMapTable((int16_t)(i - 1), -1);
    for (; i < m_map.mobj; i++) ChangeMapTable((int16_t)i, -1);
    if (m_map.merge_flg) ChangeMapTable((int16_t)(i - 1), (int16_t)(m_map.mobj - 1));
  } else {
    int start = m_ean[no - 1].start;
    int end = m_ean[no - 1].end;
    int num2 = m_map.nobj;
    if (m_map.merge_flg) num2--;
    for (int i = start; i < end; i++) {
      if (i < 0 || i >= (int)m_map_objs.size()) continue;
      ChangeMapTable((int16_t)i, (int16_t)i);
      MapObjAnimReset(i);
      if (m_pmv_mot) update_pmv_one(i);
      for (int j = 0; j < num2 && j < (int)m_map_objs.size(); j++)
        if (m_map_objs[i]->uid != 0 && m_map_objs[i]->uid == m_map_objs[j]->uid) ChangeMapTable((int16_t)j, -1);
    }
    if (m_sra_mot) {
      for (size_t i = 0; i < m_sra_mot->size(); i++) {
        FLD_MOTION* mot = (*m_sra_mot)[i].get();
        if (mot->obj_id > 0 && mot->obj_id == m_event_anim_id) {
          mot->repeat_num = (int16_t)mot->_repeat_num;
          mot->start_delay = (int16_t)mot->_start_delay;
          mot->cur_frame = 0;
          fld_motion_update(mot, SRA, nullptr);
        }
      }
    }
    if (m_has_plp && m_has_pam) {
      for (size_t i = 0; i < m_plp_data.size(); i++) {
        FLD_PLP_DATA& plp = m_plp_data[i];
        if (plp.pam_id < 0 || plp.pam_id >= (int)m_pam_data.size()) continue;
        FLD_PAM_DATA& pam = m_pam_data[plp.pam_id];
        if (plp.event_id > 0 && plp.event_id == m_event_anim_id) { pam._cur_key = 0; pam._cur_frame = 0; }
      }
    }
  }
  return 1;
}

int FldObject::SetCommonAnimation(int obj_id, int no) {
  if (!m_has_canim) return 0;
  if (no < 0) return 0;
  if (obj_id <= -1 || obj_id >= (int)m_map_objs.size() || obj_id >= (int)m_obj_tbl.size()) return 0;
  int num = m_obj_tbl[obj_id];
  if (num <= -1 || num >= (int)m_map_objs.size()) return 0;
  FLD_MAP_OBJ* o = m_map_objs[num].get();
  if (o->seq_obj_id == -1 || o->seq_obj_id >= (int)m_seq_obj.size()) return 0;
  if (no == 0) {
    if (o->mdl_id != o->org_mdl_id) {
      ReplaceModel(o, o->org_mdl_id);
      DestroySeqMeshes(o); MakeSeqMeshes(o);
    }
    o->mdl_id = o->org_mdl_id;
    o->pmv_id = o->org_pmv_id;
    o->dirty = 7;
    fld_set_obj_matrix(o);
    if (o->alpha != 31) set_obj_material_alpha(o, (float)o->alpha / 31.f);
    update_material_color(o->model->gameObject);
    m_seq_obj[o->seq_obj_id].seq_id = o->org_seq_id;
  } else {
    if (o->canim_id < 0 || o->canim_id >= (int)m_canim.size()) return 0;
    if (no > m_canim[o->canim_id].ndata) return 0;
    const FLD_COMMON_ANIM_DATA& cad = m_canim[o->canim_id].data[no - 1];
    if (o->mdl_id != cad.mdl_id) {
      ReplaceModel(o, cad.mdl_id);
      o->dirty = 7;
      fld_set_obj_matrix(o);
      DestroySeqMeshes(o); MakeSeqMeshes(o);
    }
    o->mdl_id = cad.mdl_id;
    o->pmv_id = cad.pmv_id;
    if (o->alpha != 31) set_obj_material_alpha(o, (float)o->alpha / 31.f);
    update_material_color(o->model->gameObject);
    if (o->pmv_id != -1 && m_pmv_mot && o->pmv_id < (int)m_pmv_mot->size()) o->m_motion = (*m_pmv_mot)[o->pmv_id]->Clone();
    FLD_SEQ_OBJ& so = m_seq_obj[o->seq_obj_id];
    if (cad.seq_obj_id == -1 || cad.seq_obj_id >= (int)m_seq_obj.size()) {
      so.seq_id = -1;
    } else {
      const FLD_SEQ_OBJ& so2 = m_seq_obj[cad.seq_obj_id];
      so.seq_id = so2.seq_id; so.grp_id = so2.grp_id; so.start_delay = so2.start_delay; so.repeat_delay = so2.repeat_delay;
      so.repeat_num = so2.repeat_num; so._repeat_num = so2._repeat_num; so.cur_frame = 0;
    }
  }
  o->canim_no = (int16_t)no;
  return 1;
}

int FldObject::GetEventAnimationNum() { return m_has_ean ? (int)m_ean.size() : 0; }
int FldObject::GetCommonAnimationNo(int obj_id) { FLD_MAP_OBJ* o = GetMapObjPtr(obj_id); return o ? o->canim_no : -1; }

int FldObject::IsCommonAnimationEnd(int obj_id) {
  FLD_MAP_OBJ* o = GetMapObjPtr(obj_id);
  if (!m_has_canim || !o) return 1;
  if (o->canim_id == -1) return 1;
  if (o->pmv_id != -1 && o->m_motion) { if (o->m_motion->repeat_num != 0) return 0; }
  if (o->seq_obj_id != -1 && o->seq_obj_id < (int)m_seq_obj.size()) {
    const FLD_SEQ_OBJ& so = m_seq_obj[o->seq_obj_id];
    if (so.seq_id != -1 && so.repeat_num != 0) return 0;
  }
  return 1;
}

void FldObject::MapObjAnimReset(int obj_id) {
  if (obj_id <= -1 || obj_id >= m_map.mobj || obj_id >= (int)m_obj_tbl.size()) return;
  int num = m_obj_tbl[obj_id];
  if (num <= -1 || num >= m_map.mobj || num >= (int)m_map_objs.size()) return;
  FLD_MAP_OBJ* o = m_map_objs[num].get();
  if (m_has_seq_obj && o->seq_obj_id != -1 && o->seq_obj_id < (int)m_seq_obj.size()) {
    FLD_SEQ_OBJ& so = m_seq_obj[o->seq_obj_id];
    if (so.seq_id != -1) { so.repeat_num = so._repeat_num; so.start_delay = so._start_delay; so.cur_frame = 0; }
  }
  if (m_pmv_mot && o->pmv_id != -1 && o->pmv_id < (int)m_pmv_mot->size()) {
    FLD_MOTION* mot = (*m_pmv_mot)[o->pmv_id].get();
    if (!btst(mot->flag, 4)) { mot->repeat_num = (int16_t)mot->_repeat_num; mot->start_delay = (int16_t)mot->_start_delay; mot->cur_frame = 0; }
  }
}
int FldObject::GetMapObjCommonId(int obj_id) { FLD_MAP_OBJ* o = GetMapObjPtr(obj_id); return o ? o->common_id : -1; }

void FldObject::Pause(int sw) { if (sw != 0) m_flags |= 4; else m_flags &= -5; }
void FldObject::SetDispScale(float sx, float sy, float sz) { m_scale.x = sx; m_scale.y = sy; m_scale.z = sz; }

// ------------------------------------------------------------------------------------------------ materials
void FldObject::set_priority(FLD_MAP_OBJ* obj, int priority) {
  for (Material* material : MaterialsOf(obj->model)) if (EffectiveQueue(material) == 3000) material->renderQueue = 3000 + priority;
}
void FldObject::set_priority2(FLD_MAP_OBJ* obj, int priority) {
  for (Material* material : MaterialsOf(obj->model)) material->renderQueue = priority;
}

std::vector<Material*> FldObject::get_material_list(const std::string& name) {
  std::vector<Material*> list;
  for (size_t i = 0; i < m_map_objs.size(); i++) {
    FLD_MAP_OBJ* o = m_map_objs[i].get();
    for (Material* material : MaterialsOf(o->model)) {
      size_t at = material->name.find(name);
      if (at != std::string::npos && at > 0) {                 // IndexOf(name) > 0
        list.push_back(material);
        material->shader = Shader::Find(btst(o->flag, 134217728) ? "Custom/fld_transz" : "Custom/fld_trans");
      }
    }
  }
  return list;
}

void FldObject::save_org_color() {
  Color white = Color::white;
  if (!m_root_obj) return;
  std::vector<Renderer*> rs; m_root_obj->GetComponentsInChildren<Renderer>(rs);
  for (Renderer* renderer : rs)
    for (Material* material : renderer->materials()) { white.a = material->color.a; material->SetColor("_OrgColor", white); }
}

void FldObject::set_shader(Material* mtl) {
  Texture* tex = mtl->mainTexture;
  if (!tex) return;
  if (tex->name.find(".tim") != std::string::npos) {
    if (mtl->color.a != 1.f) {
      mtl->shader = Shader::Find("Custom/fld_trans");
    } else {
      Texture2D* t2 = dynamic_cast<Texture2D*>(tex);
      bool rgba32 = t2 ? (t2->origFormat == (int)TextureFormat::RGBA32) : false;
      if (!rgba32) { mtl->shader = Shader::Find("Custom/fld_normal"); mtl->SetVector("_TexScale", 1.f, 1.f, 0.f, 0.f); }
      else mtl->shader = Shader::Find("Custom/fld_atest");
    }
  } else {
    mtl->shader = Shader::Find("Custom/fld_trans");
  }
}

void FldObject::init_model(Transform* model) {
  std::vector<Renderer*> rs; model->GetComponentsInChildren<Renderer>(rs);
  for (Renderer* renderer : rs) {
    for (Material* material : renderer->materials()) {
      if (material->mainTexture) {
        set_shader(material);
        SetSampler(material->mainTexture, FilterMode::Point, TextureWrapMode::Clamp, true);
      }
    }
  }
}

void FldObject::disable_game_object(Transform* obj, bool sw) {
  obj->gameObject->setActive(sw);
  for (Transform* item : obj->children()) disable_game_object(item, sw);
}

void FldObject::init_fld() {
  m_scale = Vector3::one;
  m_color = Color::white;
  disable_game_object(m_fld_model, false);
  m_root_obj = new GameObject("fld_root");
  for (size_t i = 0; i < m_map_objs.size(); i++) {
    FLD_MAP_OBJ* o = m_map_objs[i].get();
    std::string text = D3(o->mdl_id);
    Transform* transform = m_fld_model;
    if (m_fld_model->GetChildCount() > 0) transform = (o->mdl_id >= 0 && o->mdl_id < m_fld_model->GetChildCount()) ? m_fld_model->GetChild(o->mdl_id) : nullptr;
    if (transform) {
      Quaternion rotation = rotateXZY(o->rot);
      o->model = Object::Instantiate(transform, o->pos, rotation);
      o->model->localScale = o->scl;
      o->model->SetParent(m_root_obj->transform);
      o->model->name = D3((int)i) + "_" + text;
      disable_game_object(o->model, false);
      o->model->gameObject->setActive(true);
      if (o->model->GetChildCount() > 0) {
        MeshFilter* mf = o->model->GetComponent<MeshFilter>();
        if (mf && mf->sharedMesh && (!mf->sharedMesh->colors.empty() || !mf->sharedMesh->colors32.empty())) bset(o->flag, 268435456);
      }
    } else {
      Debug::Log("model not found %s", text.c_str());
    }
  }
  if (m_has_plp && m_has_pam) {
    std::string text2 = "pam/";
    for (size_t j = 0; j < m_plp_data.size(); j++) {
      FLD_PLP_DATA& plp = m_plp_data[j];
      if (plp.pam_id < 0 || plp.pam_id >= (int)m_pam_data.size()) continue;
      FLD_PAM_DATA& pam = m_pam_data[plp.pam_id];
      for (size_t k = 0; k < pam.clut.size(); k++) {
        FLD_PAM& p = pam.clut[k];
        if (!p.texture) {
          std::string path = text2 + plp.pal_name + "." + D3((int)k);
          p.texture = Resources::Load<Texture>(path);
          if (p.texture) SetSampler(p.texture, FilterMode::Point, TextureWrapMode::Clamp, true);
          else Debug::Log("pam texture not found %s", path.c_str());
        }
      }
    }
  }
  if (m_data_path.find("ev02") != std::string::npos && m_map_objs.size() > 37) {
    bset(m_map_objs[1]->flag, 134217728);
    bset(m_map_objs[4]->flag, 134217728);
    bset(m_map_objs[37]->flag, 134217728);
    set_priority2(m_map_objs[4].get(), 2999);
  }
  if (s_skyStretch != 1.f && m_data_path.find("ev/ev01/") != std::string::npos && !m_map_objs.empty() && m_map_objs[0]->model) {   // landscape title: the cloud plane is only 34 units wide
    m_map_objs[0]->scl.x *= s_skyStretch;
    m_map_objs[0]->model->localScale = m_map_objs[0]->scl;
  }
  if (m_sra_mot) for (size_t l = 0; l < m_sra_mot->size(); l++) (*m_sra_mot)[l]->mtl_list = get_material_list((*m_sra_mot)[l]->mtl_name);
  for (size_t i = 0; i < m_map_objs.size(); i++) {
    FLD_MAP_OBJ* o = m_map_objs[i].get();
    if (!o->model) continue;
    set_priority(o, o->priority);
  }
  save_org_color();
  init_seq();
}

void FldObject::Term() {
  if (m_fld_model) { Object::Destroy(m_fld_model->gameObject); m_fld_model = nullptr; }
  if (m_root_obj) { Object::Destroy(m_root_obj); m_root_obj = nullptr; }
  for (auto& o : m_map_objs) if (o) DestroySeqMeshes(o.get());
  m_flags = 0;
  m_map_objs.clear(); m_obj_tbl.clear(); m_map = FLD_MAP();
  m_model_data.clear(); m_seq_data.clear(); m_has_seq_data = false; m_seq_obj.clear(); m_has_seq_obj = false;
  m_emv_mot.reset(); m_pmv_mot.reset(); m_sra_mot.reset();
  m_pam_data.clear(); m_has_pam = false; m_plp_data.clear(); m_has_plp = false; m_canim.clear(); m_has_canim = false; m_ean.clear(); m_has_ean = false;
  m_event_anim_id = 0;
}

// ------------------------------------------------------------------------------------------------ sequences (vertex animation) / keyframes
void FldObject::init_seq_one(int idx) {
  FLD_MAP_OBJ* o = m_map_objs[idx].get();
  if (o->seq_obj_id >= 0 && o->seq_obj_id < (int)m_seq_obj.size() && o->model) {
    if (m_seq_obj[o->seq_obj_id].seq_id >= 0) MakeSeqMeshes(o);
  }
}
void FldObject::init_seq() {
  if (m_has_seq_obj) for (int i = 0; i < m_map.mobj && i < (int)m_map_objs.size(); i++) init_seq_one(i);
}

int FldObject::fld_motion_update(FLD_MOTION* mot, KEY_TYPE key_type, FLD_MAP_OBJ* obj) {
  int result = -1;
  std::shared_ptr<KeyArray> keyp = mot->key;
  if (btst(mot->flag, 4) && obj && obj->m_motion) keyp = obj->m_motion->key;
  if (!keyp) return result;
  KeyArray& key = *keyp;
  if (mot->nkey != 0 && mot->_repeat_num != 0 && (int)key.size() >= mot->nkey) {
    auto call = [&](FLD_KEY* k1, FLD_KEY* k2, float t) -> int {
      switch (key_type) {
      case EMV: return emv_func(k1, k2, t);
      case PMV: return pmv_func(k1, k2, t, obj);
      case CPMV: return c_pmv_func(k1, k2, t, obj);
      case SEQ: return seq_func(k1, k2, t, obj);
      case SEQ_SRT: return seq_srt_func(k1, k2, t, obj);
      case SRA: return sra_func(k1, k2, t, mot);
      }
      return 0;
    };
    if (mot->start_delay > 0) {
      result = call(key[0].get(), key[0].get(), 0.f);
      mot->start_delay -= (int16_t)s_seq_frame_rate_step;
    } else {
      FLD_KEY* k1; FLD_KEY* k2;
      if (mot->cur_frame == mot->end_frame) {
        k1 = k2 = key[mot->nkey - 1].get();
      } else {
        int num = 0, num2 = mot->nkey;
        while (num < num2) {
          int num3 = (num + num2) >> 1;
          if (key[num3]->frame < mot->cur_frame) num = num3 + 1; else num2 = num3;
        }
        if (num >= mot->nkey) num = mot->nkey - 1;               // (C#: IndexOutOfRange)
        k1 = k2 = key[num].get();
        if (k1->frame > mot->cur_frame) k1 = key[num > 0 ? num - 1 : 0].get();
        else if (k1->frame < mot->cur_frame) k2 = key[num + 1 < mot->nkey ? num + 1 : num].get();
      }
      float num4;
      if (k1 == k2) {
        num4 = 0.f;
      } else {
        num4 = (float)(mot->cur_frame - k1->frame) / (float)(k2->frame - k1->frame);
        if (num4 == 1.f) { num4 = 0.f; k1 = k2; }
        else if (num4 == 0.f) { k2 = k1; }
      }
      result = call(k1, k2, num4);
      if (!btst(mot->flag, 2)) mot->cur_frame += s_seq_frame_rate_step;
      if (mot->cur_frame > mot->end_frame) {
        if (mot->repeat_num > 0) mot->repeat_num--;
        if (mot->repeat_num == 0) mot->cur_frame = mot->end_frame;
        else mot->cur_frame -= mot->end_frame;
        mot->start_delay = mot->repeat_delay;
      }
    }
  }
  return result;
}

int FldObject::emv_func(FLD_KEY* _k1, FLD_KEY* _k2, float t) {
  FLD_EMV_KEY* a = static_cast<FLD_EMV_KEY*>(_k1); FLD_EMV_KEY* b = static_cast<FLD_EMV_KEY*>(_k2);
  int idx = s_camera_idx;
  SetCameraPos(Vector3::Lerp(a->pos, b->pos, t), idx);
  SetCameraCent(Vector3::Lerp(a->tag, b->tag, t), idx);
  SetCameraUp(Vector3::Lerp(a->vec, b->vec, t), idx);
  return 0;
}

void FldObject::seq_build_mesh(FLD_KEY* _k1, FLD_KEY* _k2, float t, FLD_MAP_OBJ* obj) {
  FLD_SEQ_KEY* k1 = static_cast<FLD_SEQ_KEY*>(_k1); FLD_SEQ_KEY* k2 = static_cast<FLD_SEQ_KEY*>(_k2);
  Transform* model = obj->model;
  if (!model) return;
  Mesh* mesh = m_frame_flip_flop != 0 ? obj->seq_mesh2 : obj->seq_mesh1;
  if (!Live(mesh)) return;
  if (k1->shape_id < 0 || k1->shape_id >= model->GetChildCount() || k2->shape_id < 0 || k2->shape_id >= model->GetChildCount()) return;
  MeshFilter* f2 = model->GetChild(k1->shape_id)->GetComponent<MeshFilter>();
  MeshFilter* f3 = model->GetChild(k2->shape_id)->GetComponent<MeshFilter>();
  if (!f2 || !f3 || !f2->sharedMesh || !f3->sharedMesh) return;
  Mesh* mesh2 = f2->sharedMesh; Mesh* mesh3 = f3->sharedMesh;
  {
    if (mesh2->vertices.size() != mesh->vertices.size() || mesh3->vertices.size() != mesh->vertices.size()) return;
    for (size_t i = 0; i < mesh->vertices.size(); i++) mesh->vertices[i] = Vector3::Lerp(mesh2->vertices[i], mesh3->vertices[i], t);
  }
  {
    size_t n = std::min(mesh->uv.size(), std::min(mesh2->uv.size(), mesh3->uv.size()));
    for (size_t j = 0; j < n; j++) mesh->uv[j] = Vector2::Lerp(mesh2->uv[j], mesh3->uv[j], t);
  }
  if (btst(obj->flag, 268435456)) {
    size_t n = mesh->colors.size();
    if (n && mesh2->colors.size() >= n && mesh3->colors.size() >= n) {
      for (size_t k = 0; k < n; k++) mesh->colors[k] = Color::Lerp(mesh2->colors[k], mesh3->colors[k], t);
    } else {
      n = std::min(mesh->colors32.size(), std::min(mesh2->colors32.size(), mesh3->colors32.size()));
      for (size_t k = 0; k < n; k++) mesh->colors32[k] = Color32(Color::Lerp(Color(mesh2->colors32[k].r / 255.f, mesh2->colors32[k].g / 255.f, mesh2->colors32[k].b / 255.f, mesh2->colors32[k].a / 255.f),
                                                                           Color(mesh3->colors32[k].r / 255.f, mesh3->colors32[k].g / 255.f, mesh3->colors32[k].b / 255.f, mesh3->colors32[k].a / 255.f), t));
    }
  }
  mesh->SetDirty();
  MeshFilter* mf = model->GetComponent<MeshFilter>();
  if (mf) mf->setMesh(mesh);
}

int FldObject::seq_func(FLD_KEY* k1, FLD_KEY* k2, float t, FLD_MAP_OBJ* obj) { seq_build_mesh(k1, k2, t, obj); return 0; }

int FldObject::seq_srt_func(FLD_KEY* _k1, FLD_KEY* _k2, float t, FLD_MAP_OBJ* obj) {
  FLD_SEQ_KEY_SRT* a = static_cast<FLD_SEQ_KEY_SRT*>(_k1); FLD_SEQ_KEY_SRT* b = static_cast<FLD_SEQ_KEY_SRT*>(_k2);
  if (!obj->model) return 0;
  obj->model->localPosition = obj->pos + Vector3::Lerp(a->pos, b->pos, t);
  obj->model->localScale = Vector3::Scale(obj->scl, Vector3::Lerp(a->scl, b->scl, t));
  obj->model->localRotation = rotateXZY(obj->rot + Vector3::Lerp(a->rot, b->rot, t));
  seq_build_mesh(a, b, t, obj);
  obj->dirty = 0;
  return 0;
}

int FldObject::sra_func(FLD_KEY* _k1, FLD_KEY* _k2, float t, FLD_MOTION* mot) {
  FLD_SRA_KEY* a = static_cast<FLD_SRA_KEY*>(_k1); FLD_SRA_KEY* b = static_cast<FLD_SRA_KEY*>(_k2);
  Color color = Color::Lerp(a->diffuse, b->diffuse, t);
  Color color2 = color * m_color;
  for (Material* material : mot->mtl_list) {
    if (!Live(material)) continue;
    if (material->mainTexture && !TexNameHas(material->mainTexture, "tz_")) material->color = color2;
    else material->color = color;
  }
  return 0;
}

int FldObject::pmv_func(FLD_KEY* _k1, FLD_KEY* _k2, float t, FLD_MAP_OBJ* obj) {
  FLD_PMV_KEY* a = static_cast<FLD_PMV_KEY*>(_k1); FLD_PMV_KEY* b = static_cast<FLD_PMV_KEY*>(_k2);
  if (!obj->model) return 0;
  obj->model->localPosition = obj->pos + Vector3::Lerp(a->pos, b->pos, t);
  obj->model->localScale = Vector3::Scale(obj->scl, Vector3::Lerp(a->scl, b->scl, t));
  obj->model->localRotation = rotateXZY(obj->rot + Vector3::Lerp(a->rot, b->rot, t));
  obj->dirty = 0;
  return 0;
}

int FldObject::c_pmv_func(FLD_KEY* _k1, FLD_KEY* _k2, float t, FLD_MAP_OBJ* obj) {
  FLD_PMV_KEY* a = static_cast<FLD_PMV_KEY*>(_k1); FLD_PMV_KEY* b = static_cast<FLD_PMV_KEY*>(_k2);
  if (!obj->model) return 0;
  Matrix4x4 m = Matrix4x4::TRS(obj->pos, rotateXZY(obj->rot), obj->scl);
  obj->model->localPosition = m.MultiplyPoint(Vector3::Lerp(a->pos, b->pos, t));
  obj->model->localScale = Vector3::Scale(obj->scl, Vector3::Lerp(a->scl, b->scl, t));
  obj->model->localRotation = rotateXZY(obj->rot + Vector3::Lerp(a->rot, b->rot, t));
  obj->dirty = 0;
  return 0;
}

void FldObject::update_pmv_one(int no) {
  if (no < 0 || no >= (int)m_map_objs.size() || no >= (int)m_obj_tbl.size()) return;
  FLD_MAP_OBJ* o = m_map_objs[no].get();
  if (m_obj_tbl[no] == -1 || btst(o->flag, 16777216) || o->pmv_id < 0 || !m_pmv_mot || o->pmv_id >= (int)m_pmv_mot->size()) return;
  FLD_MOTION* mot = (*m_pmv_mot)[o->pmv_id].get();
  if (btst(mot->flag, 4)) { if (!o->m_motion) return; mot = o->m_motion.get(); }
  if (btst(o->flag, 4194304)) {
    bset(mot->flag, 2);
    if (btst(mot->flag, 4)) fld_motion_update(mot, CPMV, o); else fld_motion_update(mot, PMV, o);
    bclr(mot->flag, 2);
  } else if (btst(mot->flag, 4)) {
    fld_motion_update(mot, CPMV, o);
  } else {
    fld_motion_update(mot, PMV, o);
  }
}

void FldObject::update_emv(int idx) {
  int num = m_camera_no[idx];
  if (m_emv_mot && num != 0 && num <= (int)m_emv_mot->size()) {
    FLD_MOTION* mot = (*m_emv_mot)[num - 1].get();
    s_camera_idx = idx;
    fld_motion_update(mot, EMV, nullptr);
    Camera* cam = Camera::main();
    if (cam) {
      cam->transform()->setPosition(m_x32_camera_pos[0]);
      cam->transform()->LookAt(m_x32_camera_tag[0], m_x32_camera_vec[0]);
    }
  }
}

void FldObject::update_pmv() { if (m_pmv_mot) for (int i = 0; i < m_map.mobj; i++) update_pmv_one(i); }

void FldObject::update_seq_one(int idx) {
  if (idx < 0 || idx >= (int)m_map_objs.size() || idx >= (int)m_obj_tbl.size()) return;
  FLD_MAP_OBJ* o = m_map_objs[idx].get();
  if (o->seq_obj_id < 0 || o->seq_obj_id >= (int)m_seq_obj.size()) return;
  FLD_SEQ_OBJ& so = m_seq_obj[o->seq_obj_id];
  if (so.seq_id < 0 || m_obj_tbl[idx] == -1) return;
  if (so.seq_id >= (int)m_seq_data.size() || so.grp_id < 0 || so.grp_id >= (int)m_seq_data[so.seq_id]->grp.size()) return;
  FLD_MOTION* mot = m_seq_data[so.seq_id]->grp[so.grp_id].get();
  if (!mot->key || (int)mot->key->size() < mot->nkey || mot->nkey < 1) return;
  mot->start_delay = so.start_delay;
  mot->repeat_delay = so.repeat_delay;
  mot->repeat_num = so.repeat_num;
  mot->cur_frame = so.cur_frame;
  mot->_repeat_num = so._repeat_num;
  int nkey = mot->nkey;
  KeyArray& key = *mot->key;
  if (btst(mot->flag, 1)) {
    if (btst(mot->flag, 8)) {
      if (mot->nkey >= 2) {
        int frame = key[mot->nkey - 1]->frame;
        if (btst(o->flag, 2)) key[mot->nkey - 1] = key[0];       // NB: aliases key[0] (the C# does the same), so key[0].frame becomes `frame` too
        else key[mot->nkey - 1] = key[mot->nkey - 2];
        key[mot->nkey - 1]->frame = frame;
      }
    } else if (mot->nkey >= 2) {
      int shape_id = !btst(o->flag, 2) ? static_cast<FLD_SEQ_KEY*>(key[mot->nkey - 2].get())->shape_id : static_cast<FLD_SEQ_KEY*>(key[0].get())->shape_id;
      static_cast<FLD_SEQ_KEY*>(key[mot->nkey - 1].get())->shape_id = shape_id;
    }
  } else {
    mot->nkey--;
  }
  KEY_TYPE key_type = !btst(mot->flag, 8) ? SEQ : SEQ_SRT;
  if (btst(o->flag, 4194304)) { bset(mot->flag, 2); fld_motion_update(mot, key_type, o); bclr(mot->flag, 2); }
  else fld_motion_update(mot, key_type, o);
  so.start_delay = mot->start_delay;
  so.repeat_delay = mot->repeat_delay;
  so.repeat_num = mot->repeat_num;
  so.cur_frame = (int16_t)mot->cur_frame;
  so._repeat_num = (int16_t)mot->_repeat_num;
  mot->nkey = (int16_t)nkey;
}
void FldObject::update_seq() { if (m_has_seq_obj) for (int i = 0; i < m_map.mobj; i++) update_seq_one(i); }

// ------------------------------------------------------------------------------------------------ palette animation (PAM / PLP) and material colour animation (SRA)
void FldObject::set_palette_data(FLD_PLP_DATA* plp, FLD_PAM_DATA* pam, int key) {
  if (plp->obj_id < 0 || plp->obj_id >= (int)m_map_objs.size()) return;
  FLD_MAP_OBJ* o = m_map_objs[plp->obj_id].get();
  if (key < 0 || key >= (int)pam->clut.size()) return;
  for (Material* material : MaterialsOf(o->model)) {
    if (material->mainTexture) {
      std::string text = material->mainTexture->name;
      size_t num = text.rfind('.');
      if (num != std::string::npos && num > 0) text = text.substr(0, num);
      if (text.find(plp->pal_name) != std::string::npos) material->mainTexture = pam->clut[key].texture;
    }
  }
}

void FldObject::update_plp() {
  if (!m_has_plp || !m_has_pam) return;
  for (size_t i = 0; i < m_plp_data.size(); i++) {
    FLD_PLP_DATA* plp = &m_plp_data[i];
    if (plp->pam_id < 0 || plp->pam_id >= (int)m_pam_data.size()) continue;
    FLD_PAM_DATA* pam = &m_pam_data[plp->pam_id];
    int key = pam->_cur_key;
    if (plp->event_id > 0) {
      if (plp->event_id == m_event_anim_id) continue;
      key = 0;
    }
    set_palette_data(plp, pam, key);
  }
  if (m_event_anim_id > 0) {
    for (size_t j = 0; j < m_plp_data.size(); j++) {
      FLD_PLP_DATA* plp = &m_plp_data[j];
      if (plp->event_id == m_event_anim_id && plp->pam_id >= 0 && plp->pam_id < (int)m_pam_data.size()) {
        FLD_PAM_DATA* pam = &m_pam_data[plp->pam_id];
        set_palette_data(plp, pam, pam->_cur_key);
      }
    }
  }
  for (size_t k = 0; k < m_pam_data.size(); k++) {
    FLD_PAM_DATA* pam = &m_pam_data[k];
    if (pam->_cur_key < 0 || pam->_cur_key >= (int)pam->clut.size()) continue;
    int16_t frame = pam->clut[pam->_cur_key].frame;
    pam->_cur_frame += (int16_t)s_seq_frame_rate_step;
    if (pam->_cur_frame >= frame) {
      pam->_cur_frame -= frame;
      pam->_cur_key++;
      if (pam->_cur_key >= pam->nclut) pam->_cur_key = 0;
    }
  }
  bset(m_flags, 128);
}

void FldObject::update_sra() {
  if (!m_sra_mot) return;
  for (size_t i = 0; i < m_sra_mot->size(); i++) {
    FLD_MOTION* mot = (*m_sra_mot)[i].get();
    if (mot->obj_id <= 0 || mot->obj_id == m_event_anim_id) fld_motion_update(mot, SRA, nullptr);
  }
}

void FldObject::update_material_color(GameObject* obj) {
  if (!Live(obj)) return;
  std::vector<Renderer*> rs; obj->GetComponentsInChildren<Renderer>(rs);
  for (Renderer* renderer : rs) {
    for (Material* material : renderer->materials()) {
      Color color = material->GetColor("_OrgColor");
      if (material->mainTexture && !TexNameHas(material->mainTexture, "tz_")) material->color = color * m_color;
      else material->color = color;
    }
  }
}

void FldObject::obj_visible(FLD_MAP_OBJ* obj, bool sw) { if (obj->model && Live(obj->model->gameObject)) obj->model->gameObject->setActive(sw); }

// ------------------------------------------------------------------------------------------------ per-frame
void FldObject::Update() {
  Camera* main = Camera::main();
  if (!main || btst(m_flags, 4) || !m_root_obj) return;
  static int s_logged; if (s_logged < 3) { s_logged++; l_info("FldObject::Update #%d (cam no %d)", s_logged, m_camera_no[0]); }
  m_frame_flip_flop ^= 1;
  if (btst(m_flags, 2)) m_root_obj->transform->localScale = Vector3::zero;
  else m_root_obj->transform->localScale = m_scale;
  s_seq_frame_rate_step = 2;
  update_emv(0);
  if (m_camera_no[0] != m_camera_no[1]) {
    update_emv(1);
  } else {
    m_x32_camera_pos[1] = m_x32_camera_pos[0];
    m_x32_camera_tag[1] = m_x32_camera_tag[0];
    m_x32_camera_vec[1] = m_x32_camera_vec[0];
  }
  update_pmv();
  update_seq();
  for (size_t i = 0; i < m_map_objs.size() && i < m_obj_tbl.size(); i++) {
    FLD_MAP_OBJ* o = m_map_objs[i].get();
    if (!o->model) continue;
    obj_visible(o, m_obj_tbl[i] != -1);
    if (btst(o->flag, 33554432)) obj_visible(o, false);
    if (o->dirty != 0) fld_set_obj_matrix(o);
    if (btst(o->flag, 32)) {
      o->model->LookAt(main->transform()->position());
      o->model->Rotate(0.f, 180.f, 0.f);
    } else if (btst(o->flag, 4)) {
      o->model->localRotation = Quaternion::AngleAxis(main->transform()->eulerAngles().y, Vector3::up);
    }
  }
  update_material_color(m_root_obj);
  update_plp();
  update_sra();
}

// ------------------------------------------------------------------------------------------------ loading
std::shared_ptr<FldObject::MotionArray> FldObject::ReadMotion(ChunkFile& file, int n, KEY_TYPE key_type) {
  auto array = std::make_shared<MotionArray>();
  if (n < 0 || n > 100000) return array;
  std::vector<int> offs(n);
  for (int i = 0; i < n; i++) { offs[i] = file.ReadInt32(); array->push_back(std::make_shared<FLD_MOTION>()); }
  for (int j = 0; j < n; j++) {
    file.fseek(offs[j]);
    FLD_MOTION* m = (*array)[j].get();
    if (key_type == SRA) { m->mtl_name = file.getString(-1); file.skip(32); }
    m->flag = file.ReadInt16(); m->nkey = file.ReadInt16(); m->repeat_num = file.ReadInt16(); m->start_delay = file.ReadInt16(); m->repeat_delay = file.ReadInt16(); m->key_size = file.ReadInt16();
    m->cur_frame = file.ReadInt32(); m->end_frame = file.ReadInt32(); m->obj_id = file.ReadInt32(); m->_start_delay = file.ReadInt32(); m->_repeat_num = file.ReadInt32();
    int nkey = m->nkey < 0 ? 0 : m->nkey;
    m->key = std::make_shared<KeyArray>(nkey);
    KeyArray& keys = *m->key;
    switch (key_type) {
    case EMV:
      for (int k = 0; k < nkey; k++) { auto key = std::make_shared<FLD_EMV_KEY>(); key->frame = file.ReadInt32(); key->pos = file.readPosition(); key->tag = file.readPosition(); key->vec = file.readPosition(); keys[k] = key; }
      break;
    case PMV:
      for (int k = 0; k < nkey; k++) { auto key = std::make_shared<FLD_PMV_KEY>(); key->frame = file.ReadInt32(); key->pos = file.readPosition(); key->rot = file.readRotation(); key->scl = file.readVector3(); keys[k] = key; }
      break;
    case SEQ:
      if (btst(m->flag, 8)) {
        for (int k = 0; k < nkey; k++) { auto key = std::make_shared<FLD_SEQ_KEY_SRT>(); key->frame = file.ReadInt32(); key->shape_id = file.ReadInt32(); key->pos = file.readPosition(); key->rot = file.readRotation(); key->scl = file.readVector3(); keys[k] = key; }
      } else {
        for (int k = 0; k < nkey; k++) { auto key = std::make_shared<FLD_SEQ_KEY>(); key->frame = file.ReadInt32(); key->shape_id = file.ReadInt32(); keys[k] = key; }
      }
      break;
    case SRA:
      for (int k = 0; k < nkey; k++) { auto key = std::make_shared<FLD_SRA_KEY>(); key->frame = file.ReadInt32(); key->amb = file.readColorf(); key->diffuse = file.readColorf(); key->spec = file.readColorf(); key->emissive = file.readColorf(); keys[k] = key; }
      break;
    default: break;
    }
  }
  return array;
}

void FldObject::SetBilinearFilter(bool sw) {
  FilterMode filterMode = sw ? FilterMode::Bilinear : FilterMode::Point;
  for (auto& op : m_map_objs)
    for (Material* material : MaterialsOf(op->model)) if (material->mainTexture) SetSampler(material->mainTexture, filterMode, TextureWrapMode::Clamp, false);
  if (!m_has_plp || !m_has_pam) return;
  for (size_t k = 0; k < m_plp_data.size(); k++) {
    FLD_PLP_DATA& plp = m_plp_data[k];
    if (plp.pam_id < 0 || plp.pam_id >= (int)m_pam_data.size()) continue;
    for (FLD_PAM& p : m_pam_data[plp.pam_id].clut) if (p.texture) SetSampler(p.texture, filterMode, TextureWrapMode::Clamp, false);
  }
}

bool FldObject::Load(const std::string& fname) {
  Term();
  m_data_path = fname;
  Transform* prefab = Resources::Load<Transform>(fname + ".fld");
  if (!prefab) { Debug::LogError("%s.fld not found", fname.c_str()); return false; }
  m_fld_model = Object::Instantiate(prefab);
  init_model(m_fld_model);
  TextAsset* textAsset = Resources::Load<TextAsset>(fname + ".map");
  if (!textAsset) { Debug::LogError("%s not found", fname.c_str()); return false; }
  ChunkFile chunkFile(textAsset->bytes.data(), textAsset->bytes.size());
  while (chunkFile.nextChunk()) {
    std::string id = chunkFile.getIDString();
    if (id == "FMAP") {
      m_map = FLD_MAP();
      m_map.nobj = chunkFile.ReadInt32();
      m_map.mobj = chunkFile.ReadInt32();
      m_map.flag = chunkFile.ReadInt32();
      chunkFile.skip(8);
      m_map.merge_flg = chunkFile.ReadInt32() != 0;
      if (m_map.mobj < 0 || m_map.mobj > 100000) { Debug::LogError("FMAP: bad object count %d", m_map.mobj); m_map.mobj = 0; }
      m_map_objs.clear(); m_map_objs.reserve(m_map.mobj);
      for (int l = 0; l < m_map.mobj; l++) {
        auto o = std::make_shared<FLD_MAP_OBJ>();
        m_map_objs.push_back(o);
        o->pos = chunkFile.readPosition();
        o->id = chunkFile.ReadInt32();
        o->rot = chunkFile.readRotation();
        o->mdl_id = chunkFile.ReadInt16();
        o->common_id = chunkFile.ReadInt16();
        o->scl = chunkFile.readVector3();
        o->uid = chunkFile.ReadInt32();
        o->flag = chunkFile.ReadInt32();
        o->priority = chunkFile.ReadInt32();
        chunkFile.skip(4);
        chunkFile.skip(4);
        o->seq_obj_id = chunkFile.ReadInt16();
        o->pmv_id = chunkFile.ReadInt16();
        o->org_mdl_id = chunkFile.ReadInt16();
        o->org_seq_id = chunkFile.ReadInt16();
        o->org_pmv_id = chunkFile.ReadInt16();
        o->canim_id = chunkFile.ReadInt16();
        o->canim_no = chunkFile.ReadInt16();
        o->btest = chunkFile.ReadInt16();
        o->alpha = 31;
      }
      chunkFile.skip(4 * m_map.mobj);
      m_obj_tbl.assign(m_map.mobj, 0);
      for (int m = 0; m < m_map.mobj; m++) m_obj_tbl[m] = chunkFile.ReadInt16();
    } else if (id == "FMDL") {
      m_model_num = chunkFile.ReadInt32();
      chunkFile.skip(4);
      if (m_model_num < 0 || m_model_num > 100000) m_model_num = 0;
      m_model_data.assign(m_model_num, FLD_MDL_DATA());
      for (int i = 0; i < m_model_num; i++) {
        FLD_MDL_DATA& d = m_model_data[i];
        chunkFile.skip(12);
        d.flag = chunkFile.ReadInt32();
        chunkFile.skip(4);
        d.common_id = chunkFile.ReadInt32();
        int num7 = chunkFile.ReadInt32();
        d.name = chunkFile.getString(num7);
        chunkFile.skip(4);
        d.display_size = chunkFile.ReadInt32();
        d.shape_id1 = chunkFile.ReadInt32();
        d.shape_id2 = chunkFile.ReadInt32();
        d.time = chunkFile.ReadSingle();
      }
    } else if (id == "FETC") {
      m_etc = FLD_MAP_ETC();
      m_etc.bg_color = chunkFile.readColor();
      m_etc.fov = chunkFile.readFX32();
      m_etc.height = chunkFile.readFX32();
      m_etc.dist = chunkFile.readFX32();
      m_etc.limit_r = chunkFile.readFX32();
      m_etc.limit_l = chunkFile.readFX32();
      m_etc.limit_u = chunkFile.readFX32();
      m_etc.limit_d = chunkFile.readFX32();
    } else if (id == "FSEQ") {
      int num12 = chunkFile.ReadInt32();
      if (num12 < 0 || num12 > 100000) num12 = 0;
      m_seq_obj.assign(num12, FLD_SEQ_OBJ()); m_has_seq_obj = true;
      for (int i = 0; i < num12; i++) {
        FLD_SEQ_OBJ& s = m_seq_obj[i];
        s.obj_id = chunkFile.ReadInt16(); s.seq_id = chunkFile.ReadInt16(); s.grp_id = chunkFile.ReadInt16(); s.start_delay = chunkFile.ReadInt16();
        s.repeat_delay = chunkFile.ReadInt16(); s.repeat_num = chunkFile.ReadInt16(); s._start_delay = chunkFile.ReadInt16(); s._repeat_num = chunkFile.ReadInt16();
        s.cur_frame = chunkFile.ReadInt16();
        chunkFile.skip(2);
      }
    } else if (id == "FSQD") {
      auto d = std::make_shared<FLD_SEQ_DATA>();
      d->ngrp = chunkFile.ReadInt32();
      d->grp = *ReadMotion(chunkFile, d->ngrp, SEQ);
      m_seq_data.push_back(d); m_has_seq_data = true;
    } else if (id == "FPMV") {
      int n3 = chunkFile.ReadInt32();
      m_pmv_mot = ReadMotion(chunkFile, n3, PMV);
    } else if (id == "FEMV") {
      int n2 = chunkFile.ReadInt32();
      m_emv_mot = ReadMotion(chunkFile, n2, EMV);
    } else if (id == "FPAM") {
      int num8 = chunkFile.ReadInt32();
      if (num8 < 0 || num8 > 10000) num8 = 0;
      std::vector<int> array(num8);
      m_pam_data.assign(num8, FLD_PAM_DATA()); m_has_pam = true;
      for (int i = 0; i < num8; i++) array[i] = chunkFile.ReadInt32();
      for (int i = 0; i < num8; i++) {
        chunkFile.fseek(array[i]);
        FLD_PAM_DATA& p = m_pam_data[i];
        p._cur_frame = chunkFile.ReadInt16(); p._cur_key = chunkFile.ReadInt16(); p.nclut = chunkFile.ReadInt16(); p.flag = chunkFile.ReadInt16();
        p.clut.assign(p.nclut > 0 ? p.nclut : 0, FLD_PAM());
        for (int k = 0; k < (int)p.clut.size(); k++) {
          p.clut[k].frame = chunkFile.ReadInt16();
          if (btst(p.flag, 1)) chunkFile.skip(512); else chunkFile.skip(32);
        }
      }
    } else if (id == "FPLP") {
      int num4 = chunkFile.ReadInt32();
      if (num4 < 0 || num4 > 10000) num4 = 0;
      m_plp_data.assign(num4, FLD_PLP_DATA()); m_has_plp = true;
      for (int i = 0; i < num4; i++) {
        FLD_PLP_DATA& p = m_plp_data[i];
        p.pal_name = chunkFile.getString(-1);
        chunkFile.skip(16);
        p.pam_id = chunkFile.ReadByte();
        p.event_id = chunkFile.ReadByte();
        p.obj_id = chunkFile.ReadInt16();
        chunkFile.skip(4);
      }
    } else if (id == "FEAN") {
      int num2 = chunkFile.ReadInt32();
      chunkFile.skip(4);
      if (num2 < 0 || num2 > 100000) num2 = 0;
      m_ean.assign(num2, FLD_EVENT_ANIM_DATA()); m_has_ean = true;
      for (int i = 0; i < num2; i++) { m_ean[i].start = chunkFile.ReadInt16(); m_ean[i].end = chunkFile.ReadInt16(); }
    } else if (id == "FCAN") {
      int num = chunkFile.ReadInt32();
      if (num < 0 || num > 100000) num = 0;
      m_canim.assign(num, FLD_COMMON_ANIM_TBL()); m_has_canim = true;
      for (int i = 0; i < num; i++) { m_canim[i].ndata = chunkFile.ReadInt32(); chunkFile.skip(4); }
      for (int j = 0; j < num; j++) {
        FLD_COMMON_ANIM_TBL& t = m_canim[j];
        if (t.ndata < 0 || t.ndata > 10000) t.ndata = 0;
        t.data.assign(t.ndata, FLD_COMMON_ANIM_DATA());
        for (int k = 0; k < t.ndata; k++) {
          t.data[k].mdl_id = chunkFile.ReadInt16(); t.data[k].seq_obj_id = chunkFile.ReadInt16(); t.data[k].pmv_id = chunkFile.ReadInt16(); t.data[k].pad = chunkFile.ReadInt16();
        }
      }
    } else if (id == "FSRA") {
      int n = chunkFile.ReadInt32();
      m_sra_mot = ReadMotion(chunkFile, n, SRA);
    } else if (id == "FHDR" || id == "FLGT" || id == "FFOG" || id == "FPTX" || id == "FCRT" || id == "FCRS" || id == "FTEX" || id == "FTTL" || id == "FATB" || id == "FNAM") {
      // ignored chunks
    } else {
      Debug::LogError("ChunkError:%s", id.c_str());
    }
  }
  l_info("FldObject::Load %s: %d map objects, %d models, cameras %d, events %d", fname.c_str(), (int)m_map_objs.size(), m_model_num, GetCameraNoLength(), GetEventAnimationNum());
  init_fld();
  l_info("FldObject::Load %s: init done", fname.c_str());
  return true;
}

void FldObject::ChangeMapTable(int16_t tbl_id, int16_t obj_id) {
  if (tbl_id >= 0 && tbl_id < (int)m_map_objs.size() && tbl_id < (int)m_obj_tbl.size()) m_obj_tbl[tbl_id] = obj_id;
}

void FldObject::fld_set_obj_matrix(FLD_MAP_OBJ* obj) {
  Transform* model = obj->model;
  if (!model) { obj->dirty = 0; return; }
  if ((obj->dirty & 8) != 0) obj->pos = model->position();
  else if ((obj->dirty & 1) != 0) model->localPosition = obj->pos;
  if ((obj->dirty & 0x10) != 0) obj->scl = model->localScale;
  else if ((obj->dirty & 2) != 0) model->localScale = obj->scl;
  if ((obj->dirty & 0x20) != 0) obj->rot = model->rotation().eulerAngles();
  else if ((obj->dirty & 4) != 0) model->localRotation = rotateXZY(obj->rot);
  obj->dirty = 0;
}

void FldObject::SetMaterialColor(Color color) { m_color = color; }

void FldObject::SetVertexColor(Color color) {
  if (!m_root_obj) return;
  std::vector<MeshFilter*> fs; m_root_obj->GetComponentsInChildren<MeshFilter>(fs);
  for (MeshFilter* mf : fs) {
    Mesh* mesh = mf->mesh();
    if (!mesh) continue;
    for (Color& c : mesh->colors) c = color;
    for (Color32& c : mesh->colors32) c = Color32(color);
    mesh->SetDirty();
  }
}

void FldObject::calculateExecSepia() {
  for (auto& op : m_map_objs) {
    for (Material* material : MaterialsOf(op->model)) {
      if (!material->shader) continue;
      std::string name = material->shader->name;
      if (name.size() >= 6 && name.compare(name.size() - 6, 6, "_sepia") == 0) continue;     // already swapped (the C# would build "..._sepia_sepia")
      name += "_sepia";
      Shader* s = Shader::Find(name);
      if (s && s->id >= 0) material->shader = s;                                                // no sepia variant (e.g. fld_transz): keep the shader
    }
  }
  save_org_color();
}

void FldObject::SetRootVisible(bool sw) { if (m_root_obj) m_root_obj->SetActive(sw); }

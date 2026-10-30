// game/FldObject.hpp -- OWNER: WP5b.  Port of re/cs_out/Assembly-CSharp/FldObject.cs (town / dungeon / battle-stage map models: `.fld` prefab + `.map` chunk file).
// Names, field layout and semantics follow the C#; arrays -> std::vector, reference-typed classes -> shared_ptr (the C# aliases motion keys in a few places and the port
// keeps that: see update_seq_one). Public API = what NativeManager / Chara / Cleaner call.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "miniunity.hpp"
using namespace miniunity;

class ChunkFile;

class FldObject {
public:
  // ---------------------------------------------------------------------------------------------------- data (public nested types of the C#)
  struct FLD_KEY { int frame = 0; virtual ~FLD_KEY() {} };
  struct FLD_EMV_KEY : FLD_KEY { Vector3 pos, tag, vec; };
  struct FLD_PMV_KEY : FLD_KEY { Vector3 pos, rot, scl; };
  struct FLD_SEQ_KEY : FLD_KEY { int shape_id = 0; };
  struct FLD_SEQ_KEY_SRT : FLD_SEQ_KEY { Vector3 pos, rot, scl; };       // C#: derives FLD_KEY only (its (FLD_SEQ_KEY) casts would throw); sharing shape_id is the safe reading
  struct FLD_SRA_KEY : FLD_KEY { Color amb, diffuse, spec, emissive; };
  typedef std::vector<std::shared_ptr<FLD_KEY>> KeyArray;

  struct FLD_MOTION {
    std::string mtl_name; std::vector<Material*> mtl_list;
    int16_t flag = 0, nkey = 0, repeat_num = 0, start_delay = 0, repeat_delay = 0, key_size = 0;
    int cur_frame = 0, end_frame = 0, obj_id = 0, _start_delay = 0, _repeat_num = 0;
    std::shared_ptr<KeyArray> key;                                       // shared by Clone() like the C# array reference
    std::shared_ptr<FLD_MOTION> Clone() const;
  };
  struct FLD_MAP_OBJ {
    Vector3 pos; int id = 0; Vector3 rot; int16_t mdl_id = 0, common_id = 0; Vector3 scl; int uid = 0, flag = 0, priority = 0;
    int16_t seq_obj_id = -1, pmv_id = -1, org_mdl_id = 0, org_seq_id = 0, org_pmv_id = 0, canim_id = -1, canim_no = 0, btest = 0;
    Transform* model = nullptr; Mesh* seq_mesh1 = nullptr; Mesh* seq_mesh2 = nullptr; int dirty = 0;
    std::shared_ptr<FLD_MOTION> m_motion; int alpha = 31;
    std::vector<Material*> mat_normal, mat_transz;
  };
  struct FLD_SEQ_OBJ { int16_t obj_id = 0, seq_id = -1, grp_id = 0, start_delay = 0, repeat_delay = 0, repeat_num = 0, _start_delay = 0, _repeat_num = 0, cur_frame = 0; };
  struct FLD_MDL_DATA { int size = 0, flag = 0, common_id = 0; std::string name; int display_size = 0, shape_id1 = 0, shape_id2 = 0; float time = 0; };
  struct FLD_COMMON_ANIM_DATA { int16_t mdl_id = 0, seq_obj_id = -1, pmv_id = -1, pad = 0; };
  struct FLD_COMMON_ANIM_TBL { int ndata = 0; std::vector<FLD_COMMON_ANIM_DATA> data; };
  struct FLD_MAP_ETC { Color bg_color = Color(0.f, 0.f, 0.f, 1.f); float fov = 20.f, height = 10.f, dist = 20.f, limit_r = 0, limit_l = 0, limit_u = 0, limit_d = 0; };
  typedef FLD_MAP_ETC MapEtc;                                            // name used by the header-first stub
  struct FLD_SEQ_DATA { int ngrp = 0; std::vector<std::shared_ptr<FLD_MOTION>> grp; };
  struct FLD_MAP { int nobj = 0, mobj = 0, flag = 0; bool merge_flg = false; };

  enum { FLD_OBJ_MODEL_DIRTY_POS_ORG = 1, FLD_OBJ_MODEL_DIRTY_SCL_ORG = 2, FLD_OBJ_MODEL_DIRTY_ROT_ORG = 4, FLD_OBJ_MODEL_DIRTY_ALL_ORG = 7,
         FLD_OBJ_MODEL_DIRTY_POS_FX32 = 8, FLD_OBJ_MODEL_DIRTY_SCL_FX32 = 16, FLD_OBJ_MODEL_DIRTY_ROT_FX32 = 32, FLD_OBJ_MODEL_DIRTY_ALL_FX32 = 56 };
  enum { FLG_FLD_INIT = 1, FLG_FLD_HIDE = 2, FLG_FLD_PAUSE = 4, FLG_FLD_HAS_LIGHT = 8, FLG_FLD_HAS_FOG = 16, FLG_FLD_SUB = 32, FLG_FLD_CALC_PALETTE = 64,
         FLG_FLD_NEED_PALETTE_SEND = 128, FLG_FLD_DISABLE_PALETTE_SEND = 256 };
  enum { FLD_OBJ_FLAG_GROUND = 1, FLD_OBJ_FLAG_LOOP = 2, FLD_OBJ_FLAG_BILLBOARD_Y = 4, FLD_OBJ_FLAG_BILLBOARD_XYZ = 32, FLD_OBJ_FLAG_FARM = 65536, FLD_OBJ_FLAG_HIDE = 131072,
         FLD_OBJ_FLAG_COMMON = 262144, FLD_OBJ_FLAG_UV_ANIME = 524288, FLD_OBJ_FLAG_VANISH = 1048576, FLD_OBJ_FLAG_VANISH_C = 2097152, FLD_OBJ_FLAG_PAUSE = 4194304,
         FLD_OBJ_FLAG_DOOR_PAIR = 8388608, FLD_OBJ_FLAG_SKIP = 16777216, FLD_OBJ_FLAG_ONOFF = 33554432, FLD_OBJ_FLAG_KEEP_PRIORITY = 67108864, FLD_OBJ_FLAG_ALPHA_DRAW = 134217728,
         FLD_OBJ_FLAG_SEQ_COLOR = 268435456, FLD_OBJ_FLAG_SEQ_VTX = 536870912, FLD_OBJ_FLAG_SEQ_ST = 1073741824, FLD_OBJ_FLAG_BILLBOARD = 36 };
  enum { FLG_PAM_256 = 1, FLG_PAM_CALC_END = 2 };
  enum { FLD_MOTION_LOOP = 1, FLD_MOTION_PAUSE = 2, FLD_MOTION_COMMON = 4, FLD_MOTION_EX_SEQ = 8 };

  FldObject();
  ~FldObject();

  // ---------------------------------------------------------------------------------------------------- lifecycle (NativeManager op 0 / 33 / frame loop)
  bool Load(const std::string& fname);          // "map/c/ca/in/ca2f1a": loads fname + ".fld" (prefab) and fname + ".map" (chunk file)
  void Term();
  void Update();
  void Pause(int sw);
  void SetDispScale(float sx, float sy, float sz);
  void SetBilinearFilter(bool sw);
  void SetRootVisible(bool sw);
  void SetMaterialColor(Color c);
  void SetVertexColor(Color c);
  void calculateExecSepia();
  void FixProblems(int mapId);
  void update_material_color_force();
  bool IsLoaded() const { return m_root_obj != nullptr; }
  GameObject* GetRootObject() const { return m_root_obj; }

  // ---------------------------------------------------------------------------------------------------- map info / objects
  FLD_MAP_ETC GetMapEtc() const { return m_etc; }
  const FLD_MAP& GetMap() const { return m_map; }
  int GetMapObjNum() const { return (int)m_map_objs.size(); }
  FLD_MAP_OBJ* GetMapObjPtr(int obj_id);
  int GetMapObjUid(int obj_id);
  int GetMapObjCommonId(int obj_id);
  void SetMapObjAlpha(int obj_id, int alpha, int keep_priority = 0);
  void SetMapObjOnOff(int obj_id, int flag);
  void SetMapObjPos(int obj_id, Vector3 pos);
  void SetMapObjRot(int obj_id, Vector3 rot);
  void SetMapObjScl(int obj_id, Vector3 scl);
  Vector3 GetMapObjPos(int obj_id); Vector3 GetMapObjRot(int obj_id); Vector3 GetMapObjScl(int obj_id);
  Vector3 GetMapObjDispPos(int obj_id); Vector3 GetMapObjDispRot(int obj_id); Vector3 GetMapObjDispScl(int obj_id);
  int GetMapObjFrame(int obj_id); int GetMapObjEndFrame(int obj_id);
  void AddMapObjPos(int obj_id, Vector3 pos); void AddMapObjRot(int obj_id, Vector3 rot); void AddMapObjScl(int obj_id, Vector3 scl);
  void SetMapUidAlpha(int uid_id, int alpha, int keep_priority = 0); void SetMapUidOnOff(int uid_id, int flag);
  void SetMapUidPos(int uid_id, Vector3 pos); void SetMapUidRot(int uid_id, Vector3 rot); void SetMapUidScl(int uid_id, Vector3 scl);
  void AddMapUidPos(int uid_id, Vector3 pos); void AddMapUidRot(int uid_id, Vector3 rot); void AddMapUidScl(int uid_id, Vector3 scl);
  void SetVanishFlag(int flag);
  void MapObjAnimReset(int obj_id);

  // ---------------------------------------------------------------------------------------------------- animations
  int SetEventAnimation(int no);
  int GetEventAnimationNum();
  int SetCommonAnimation(int obj_id, int no);
  int GetCommonAnimationNo(int obj_id);
  int IsCommonAnimationEnd(int obj_id);

  // ---------------------------------------------------------------------------------------------------- map cameras (EMV motions)
  int GetCameraNoLength() const { return m_emv_mot ? (int)m_emv_mot->size() : 0; }
  int GetCameraNo(int idx = 0) const { return m_camera_no[idx]; }
  bool SetCameraNo(int no, int idx);
  void SetCameraPos(float x, float y, float z, int idx); void SetCameraCent(float x, float y, float z, int idx); void SetCameraUp(float x, float y, float z, int idx);
  void SetCameraPos(Vector3 pos, int idx); void SetCameraCent(Vector3 cent, int idx); void SetCameraUp(Vector3 up, int idx);
  Vector3 GetCameraPos(int idx = 0) const { return m_x32_camera_pos[idx]; }
  Vector3 GetCameraCent(int idx = 0) const { return m_x32_camera_tag[idx]; }
  Vector3 GetCameraUp(int idx = 0) const { return m_x32_camera_vec[idx]; }
  int s_camera_idx = 0;

  static bool btst(int x, int mask) { return (x & mask) != 0; }
  static void bset(int& x, int mask) { x |= mask; }
  static void bset(int16_t& x, int mask) { x |= (int16_t)mask; }
  static void bclr(int& x, int mask) { x &= ~mask; }
  static void bclr(int16_t& x, int mask) { x &= (int16_t)(~mask); }
  static float FX_Mul(float a, float b) { return a * b; }

  std::vector<std::shared_ptr<FLD_MAP_OBJ>> m_map_objs;                  // C#: FLD_MAP_OBJ[] (NativeManager reads m_map_objs[38]->model)

private:
  enum KEY_TYPE { EMV, PMV, SEQ, SEQ_SRT, CPMV, SRA };
  struct FLD_EVENT_ANIM_DATA { int16_t start = 0, end = 0; };
  struct FLD_PAM { int16_t frame = 0; Texture* texture = nullptr; };
  struct FLD_PAM_DATA { int16_t _cur_frame = 0, _cur_key = 0, nclut = 0, flag = 0; std::vector<FLD_PAM> clut; };
  struct FLD_PLP_DATA { std::string pal_name; int16_t pam_id = 0, event_id = 0, obj_id = 0; };
  typedef std::vector<std::shared_ptr<FLD_MOTION>> MotionArray;

  Quaternion rotateXZY(Vector3 rot);
  void set_obj_material_alpha(FLD_MAP_OBJ* obj, float alpha);
  void update_obj_material_color(FLD_MAP_OBJ* obj);
  void set_priority(FLD_MAP_OBJ* obj, int priority);
  void set_priority2(FLD_MAP_OBJ* obj, int priority);
  std::vector<Material*> get_material_list(const std::string& name);
  void save_org_color();
  void set_shader(Material* mtl);
  void init_model(Transform* model);
  void disable_game_object(Transform* obj, bool sw);
  void init_fld();
  void ReplaseShader(FLD_MAP_OBJ* obj, Shader* src, Shader* dst);
  void init_seq_one(int idx);
  void init_seq();
  int fld_motion_update(FLD_MOTION* mot, KEY_TYPE key_type, FLD_MAP_OBJ* obj);
  int emv_func(FLD_KEY* k1, FLD_KEY* k2, float t);
  void seq_build_mesh(FLD_KEY* k1, FLD_KEY* k2, float t, FLD_MAP_OBJ* obj);
  int seq_func(FLD_KEY* k1, FLD_KEY* k2, float t, FLD_MAP_OBJ* obj);
  int seq_srt_func(FLD_KEY* k1, FLD_KEY* k2, float t, FLD_MAP_OBJ* obj);
  int sra_func(FLD_KEY* k1, FLD_KEY* k2, float t, FLD_MOTION* mot);
  int pmv_func(FLD_KEY* k1, FLD_KEY* k2, float t, FLD_MAP_OBJ* obj);
  int c_pmv_func(FLD_KEY* k1, FLD_KEY* k2, float t, FLD_MAP_OBJ* obj);
  void update_pmv_one(int no);
  void update_emv(int idx);
  void update_pmv();
  void update_seq_one(int idx);
  void update_seq();
  void set_palette_data(FLD_PLP_DATA* plp, FLD_PAM_DATA* pam, int key);
  void update_plp();
  void update_sra();
  void update_material_color();
  void obj_visible(FLD_MAP_OBJ* obj, bool sw);
  std::shared_ptr<MotionArray> ReadMotion(ChunkFile& file, int n, KEY_TYPE key_type);
  void ChangeMapTable(int16_t tbl_id, int16_t obj_id);
  void fld_set_obj_matrix(FLD_MAP_OBJ* obj);
  void ReplaceModel(FLD_MAP_OBJ* o, int mdl_id);
  void DestroySeqMeshes(FLD_MAP_OBJ* o);
  void MakeSeqMeshes(FLD_MAP_OBJ* o);

  Transform* m_fld_model = nullptr; GameObject* m_root_obj = nullptr;
  int m_flags = 0, m_vanish_flag = 0, m_frame_flip_flop = 0, s_seq_frame_rate_step = 0, m_event_anim_id = 0;
  FLD_MAP m_map;
  std::shared_ptr<MotionArray> m_emv_mot, m_pmv_mot, m_sra_mot;
  std::vector<FLD_MDL_DATA> m_model_data; std::vector<FLD_SEQ_OBJ> m_seq_obj; bool m_has_seq_obj = false;
  std::vector<FLD_PLP_DATA> m_plp_data; std::vector<FLD_PAM_DATA> m_pam_data; bool m_has_plp = false, m_has_pam = false;
  std::vector<FLD_COMMON_ANIM_TBL> m_canim; bool m_has_canim = false;
  std::vector<std::shared_ptr<FLD_SEQ_DATA>> m_seq_data; bool m_has_seq_data = false;
  std::vector<FLD_EVENT_ANIM_DATA> m_ean; bool m_has_ean = false;
  std::vector<int16_t> m_obj_tbl;
  int m_model_num = 0;
  FLD_MAP_ETC m_etc;
  int m_camera_no[2] = { 0, 0 };
  Vector3 m_x32_camera_pos[2], m_x32_camera_tag[2], m_x32_camera_vec[2];
  bool m_updateColor = true; Color m_color; Vector3 m_scale; std::string m_data_path;
};

// game/NativeManager.hpp -- OWNER: WP3.  Port of NativeManager.cs: the ONE per-frame exchange with libdq4core (01 §1.2, 02 §B/§C).
//   Update(): write the input stream, GameUpdate(pad), then execute the native->host command stream through m_cmd_tbl[41].
// The operand consumption of every cmd_* is complete (it follows the C# exactly); the effects are calls into the WP5/WP6 classes,
// which are header-first stubs at this stage.
#pragma once
#include <string>
#include <cstdint>
#include <list>
#include <vector>
#include "miniunity.hpp"
#include "game/FldObject.hpp"
#include "game/Chara.hpp"
#include "game/Riseup.hpp"
#include "game/MeshChara.hpp"
#include "game/FbxObject.hpp"

class NativeManager {
public:
  static constexpr int NUM_CMDS = 41;
  static FldObject m_fld;
  static void Init();                       // NativeCall.GameInit(m_input, m_output, m_output_float, m_string)
  static void ResetNativePacket();
  static void Update();                     // uses TouchPad.m_press/m_toggle
  static void Update(int padPress, int padToggle);
  static MeshChara* GetMeshChara(int index) { return m_dssa[index]; }
  static bool InBattleStage() { return m_camera_mode == 1; }          // landscape/battle_speed helpers (game/GameLoop.cpp, SceneCamera.cpp)
  static void ReleaseStage();
  static int GetIntEx() { return m_output[m_output_offset++]; }
  static float GetFloatEx() { return m_output_float[m_float_offset++]; }
  static void SetInt(int i) { m_input[m_input_offset++] = i; }

  // ---- statistics for the packet dumper (per frame) ----
  struct FrameStats { int frame = 0; int nCmds = 0; int cmdCount[NUM_CMDS] = {}; int nInts = 0, nFloats = 0; int flagOut = 0; bool desync = false; };
  static const FrameStats& LastFrame() { return s_stats; }
  static std::string s_sceneName;           // last map / field loaded (perf log: GameLoop prints avg frame time per scene)
  static const char* OpName(int op);        // packet opcode names (02 §C), for the logs

private:
  static int m_input[1024]; static int m_output[8192]; static float m_output_float[8192]; static const char* m_string[32];
  static int m_input_offset, m_output_offset, m_float_offset;
  static int m_camera_mode, m_camera_flag, m_fld_etc; static bool m_fld_load;
  static std::list<int> m_anim_list;
  static Chara* m_chara[40]; static Riseup* m_rise[16]; static MeshChara* m_dssa[40]; static FbxObject* m_model[16];
  static FrameStats s_stats;
  static int GetInt() { return m_output[m_output_offset++]; }
  static float GetFloat() { return m_output_float[m_float_offset++]; }
  static Vector3 GetVector3() { float x = GetFloat(), y = GetFloat(), z = GetFloat(); return Vector3(x, y, z); }
  static const char* GetString(int i) { return m_string[i]; }
  static bool btst(int a, int b) { return (a & b) != 0; }
  static void CharaTerm();
  typedef void (*NativeCmd)();
  static NativeCmd m_cmd_tbl[NUM_CMDS];
  static void cmd_fld_load(); static void cmd_fld_color(); static void cmd_fld_alpha(); static void cmd_fld_pos(); static void cmd_fld_rot(); static void cmd_fld_common_anim();
  static void cmd_fld_event_anim(); static void cmd_fld_on_off(); static void cmd_fld_etc(); static void cmd_back_color(); static void cmd_fade(); static void cmd_camera();
  static void cmd_camera_sub(); static void cmd_chara_load(); static void cmd_chara(); static void cmd_chara_on_off(); static void cmd_chara_shadow(); static void cmd_chara_sleep_pose();
  static void cmd_chara_sleep_near(); static void cmd_chara_pose(); static void cmd_rise_icon_load(); static void cmd_rise_icon(); static void cmd_mesh_load(); static void cmd_mesh();
  static void cmd_mesh_on_off(); static void cmd_mesh_shader(); static void cmd_mesh_color(); static void cmd_mesh_tex_offset(); static void cmd_mesh_ortho(); static void cmd_field_load();
  static void cmd_field(); static void cmd_field_symbol(); static void cmd_field_scale(); static void cmd_fbx_load(); static void cmd_fbx(); static void cmd_map_color_rate();
  static void cmd_screen_effect(); static void cmd_se_play(); static void cmd_se_stop(); static void cmd_bgm(); static void cmd_particle();
};

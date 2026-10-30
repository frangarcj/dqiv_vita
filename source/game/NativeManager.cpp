// game/NativeManager.cpp -- OWNER: WP3.  Line-for-line port of NativeManager.cs (see NativeManager.hpp). C# source: re/cs_out/Assembly-CSharp/NativeManager.cs
//
// Deviations from the C# (all deliberate):
//   * `chara.Foo(GetFloat())` on a null slot would NRE in C#; here the operands are consumed and the call skipped.
//   * per-frame statistics + packet recorder hooks (native/packet_recorder.*) + opcode range guard (an unknown opcode ends the frame and flags `desync`).
#include "game/NativeManager.hpp"

#include <cstring>

#include "game/BackColor.hpp"
#include "game/Capture.hpp"
#include "game/Cleaner.hpp"
#include "game/Fade.hpp"
#include "game/Field.hpp"
#include "game/Global.hpp"
#include "game/PadFocus.hpp"
#include "game/DevCheats.hpp"
#include "game/SceneCamera.hpp"
#include "game/Sepia.hpp"
#include "game/Sound.hpp"
#include "game/Spotlight.hpp"
#include "game/TouchPad.hpp"
#include "native/native_bridge.hpp"
#include "native/packet_recorder.hpp"
#include "native/packet_replay.hpp"
#include "platform/perf.hpp"
#include "utils/logger.h"

using namespace miniunity;

// Out-of-range slot ids from the native side would be an array overrun (the C# would throw): read as null, writes are rejected in the *_load commands.
template <class T, size_t N> static T* Slot(T* (&a)[N], int i) { return (unsigned)i < N ? a[i] : nullptr; }

int NativeManager::m_input[1024];
int NativeManager::m_output[8192];
float NativeManager::m_output_float[8192];
const char* NativeManager::m_string[32];
int NativeManager::m_input_offset = 0, NativeManager::m_output_offset = 0, NativeManager::m_float_offset = 0;
int NativeManager::m_camera_mode = 0, NativeManager::m_camera_flag = 0;
bool NativeManager::m_fld_load = false;
std::string NativeManager::s_sceneName = "(title)";
std::list<int> NativeManager::m_anim_list;
FldObject NativeManager::m_fld;
Chara* NativeManager::m_chara[40];
Riseup* NativeManager::m_rise[16];
MeshChara* NativeManager::m_dssa[24];
FbxObject* NativeManager::m_model[16];
NativeManager::FrameStats NativeManager::s_stats;
static int s_frame = 0;

NativeManager::NativeCmd NativeManager::m_cmd_tbl[NUM_CMDS] = {
  cmd_fld_load, cmd_fld_color, cmd_fld_alpha, cmd_fld_pos, cmd_fld_rot, cmd_fld_common_anim, cmd_fld_event_anim, cmd_fld_on_off, cmd_back_color, cmd_fade,
  cmd_camera, cmd_camera_sub, cmd_chara_load, cmd_chara, cmd_chara_on_off, cmd_chara_shadow, cmd_chara_sleep_pose, cmd_chara_sleep_near, cmd_chara_pose, cmd_rise_icon_load,
  cmd_rise_icon, cmd_mesh_load, cmd_mesh, cmd_mesh_on_off, cmd_mesh_shader, cmd_mesh_color, cmd_mesh_tex_offset, cmd_field_load, cmd_field, cmd_field_symbol,
  cmd_field_scale, cmd_fbx_load, cmd_fbx, cmd_map_color_rate, cmd_screen_effect, cmd_se_play, cmd_se_stop, cmd_bgm
};

const char* NativeManager::OpName(int op) {
  static const char* n[NUM_CMDS] = { "fld_load", "fld_color", "fld_alpha", "fld_pos", "fld_rot", "fld_common_anim", "fld_event_anim", "fld_on_off", "back_color", "fade",
    "camera", "camera_sub", "chara_load", "chara", "chara_on_off", "chara_shadow", "chara_sleep_pose", "chara_sleep_near", "chara_pose", "rise_icon_load",
    "rise_icon", "mesh_load", "mesh", "mesh_on_off", "mesh_shader", "mesh_color", "mesh_tex_offset", "field_load", "field", "field_symbol",
    "field_scale", "fbx_load", "fbx", "map_color_rate", "screen_effect", "se_play", "se_stop", "bgm" };
  return (op >= 0 && op < NUM_CMDS) ? n[op] : "?";
}

void NativeManager::Init() { NativeCall::GameInit(m_input, m_output, m_output_float, m_string); }
void NativeManager::ResetNativePacket() { if (NativeCall::ResetNativePacket) NativeCall::ResetNativePacket(m_input, m_output, m_output_float, m_string); }

void NativeManager::CharaTerm() {
  for (int i = 0; i < 40; i++)
    if (m_chara[i]) { m_chara[i]->Release(); delete m_chara[i]; m_chara[i] = nullptr; }
}

// ---------------------------------------------------------------------------------------------------- commands (02 §C)
void NativeManager::cmd_fld_load() {
  int num = GetInt();
  int num2 = (m_camera_mode = num >> 8);
  Global::InitCamera(num2 & 1);
  if (num2 == 2) SceneCamera::SetTargetCamera(false);
  CharaTerm();
  std::string text = std::string("map/") + (GetString(num & 0xFF) ? GetString(num & 0xFF) : "");
  s_sceneName = text;
  if (m_fld_load) {
    if (m_fld.GetCameraNo() > 0) m_fld.SetCameraNo(0, 0);       // DQ4: done here (DQ5's FldObject.Term did it)
    m_fld.Term();
  }
  m_anim_list.clear();
  m_fld.Load(text);
  m_fld_load = true;
  if (num2 == 1) m_fld.SetDispScale(6.f, 6.f, 6.f);
  BackColor::SetColor(m_fld.GetMapEtc().bg_color);
  Chara::SetSleepNear(0, 0);
  Cleaner::Cleaning();
}
void NativeManager::cmd_field_load() {
  int num = GetInt();
  s_sceneName = num > 0 ? "worldmap" : s_sceneName;
  if (num > 0) {
    if (m_fld_load) { m_fld_load = false; m_fld.Term(); }
    if (m_camera_mode != 0) { m_camera_mode = 0; Global::InitCamera(0); }
    Field::Init();
  } else {
    Field::Release();
  }
  CharaTerm();
  Cleaner::Cleaning();
}
void NativeManager::cmd_field() {
  int startX = GetInt(), startY = GetInt(), width = GetInt();
  int len = GetInt();
  std::vector<int> array(m_output + m_output_offset, m_output + m_output_offset + len);
  m_output_offset += len;
  float x = GetFloat(), y = GetFloat();
  Field::SetPosition(x, y);
  Field::SetIndexList(startX, startY, width, array);
}
void NativeManager::cmd_field_symbol() {
  int num = GetInt();
  if (num < 32768) {
    float x = GetFloat(), y = GetFloat(), alpha = GetFloat();
    Field::SetSymbol(num & 0xFF, num >> 8, x, y, alpha);
    Field::SetSymbolVisible(num & 0xFF, true);
  } else if (btst(num, 4096)) {
    float s = GetFloat();
    Field::SetSymbolScale(num & 0xFF, s);
    float p = GetFloat();
    Field::SetSymbolPrio(num & 0xFF, p);
  } else {
    Field::SetSymbolVisible(num & 0xFF, btst(num, 16384));
  }
}
void NativeManager::cmd_field_scale() {
  int num = GetInt();
  if (num >= 0) { if (Slot(m_chara, num)) Field::SetFloatingObject(m_chara[num]->m_chara); }
  else Field::SetScale(GetFloat());
}
void NativeManager::cmd_fld_common_anim() {
  int num = GetInt();
  m_fld.SetCommonAnimation(num & 0xFFFF, num >> 16);
  m_anim_list.push_back(num & 0xFFFF);
}
void NativeManager::cmd_fld_event_anim() {
  int num = GetInt();
  if (num < 16384) m_fld.SetEventAnimation(num);
  else m_fld.SetCameraNo(num & 0x3FFF, 0);
}
void NativeManager::cmd_fld_alpha() { int num = GetInt(); m_fld.SetMapObjAlpha(num & 0xFFFF, (num >> 16) & 0xFF, num >> 24); }
void NativeManager::cmd_fld_pos() { int obj_id = GetInt(); m_fld.SetMapObjPos(obj_id, GetVector3()); }
void NativeManager::cmd_fld_rot() {
  int obj_id = GetInt();
  Vector3 rot; rot.x = GetFloat(); rot.y = GetFloat(); rot.z = 0.f;
  m_fld.SetMapObjRot(obj_id, rot);
}
void NativeManager::cmd_fld_on_off() { int num = GetInt(); m_fld.SetMapObjOnOff(num & 0xFFFF, num >> 16); }
void NativeManager::cmd_fld_color() { m_fld.calculateExecSepia(); }

void NativeManager::cmd_chara_load() {
  int num = GetInt(), num2 = GetInt();
  if ((unsigned)num >= 40) { l_error("cmd_chara_load: bad slot %d", num); return; }
  if (num2 >= 0) {
    Chara* chara = new Chara(num2 & 0xFFF);
    if (btst(num2, 4096)) { chara->m_billboard_request = false; chara->SetFieldMode(); chara->SetVisible(false); }
    if (m_chara[num]) { m_chara[num]->Release(); delete m_chara[num]; }
    m_chara[num] = chara;
  } else if (m_chara[num]) {
    m_chara[num]->Release(); delete m_chara[num]; m_chara[num] = nullptr;
  }
}
void NativeManager::cmd_chara() {
  int num = GetInt();
  Chara* chara = Slot(m_chara, num & 0xFF);
  if (btst(num, 2048)) { float s = GetFloat(); if (chara) chara->SetScale(s); }
  if (btst(num, 4096)) {
    Color c; c.r = GetFloat(); c.g = GetFloat(); c.b = GetFloat(); c.a = GetFloat();
    if (chara) chara->SetColor(c);
  }
  if (btst(num, 8192)) {
    Vector3 v = GetVector3();
    if (chara) { chara->SetPosition(v); chara->m_billboard_request = true; }
    if ((num & 0xFF) == 0 && DevCheats::HeroPosLog()) { static Vector3 s_last; if ((v - s_last).magnitude() > 0.001f) { l_info("CHARA0 pos f%d (%.3f, %.3f, %.3f)", s_frame, v.x, v.y, v.z); s_last = v; } }   // hero position as sent by the native (walk tests)
  }
  if (btst(num, 16384)) { int n2 = GetInt(); if (chara) chara->SetPattern(n2 & 0xFF, n2 >> 8); }
  if (btst(num, 32768)) { Vector3 v = GetVector3(); if (chara) chara->SetFieldPosition(v); }
}
void NativeManager::cmd_chara_on_off() {
  int slot = GetInt(); int vis = GetInt();
  if (Slot(m_chara, slot)) Slot(m_chara, slot)->SetVisible(vis != 0);
}
void NativeManager::cmd_chara_shadow() {
  int num = GetInt();
  Chara* chara = Slot(m_chara, num & 0xFF);
  if (btst(num, 768)) { if (chara) chara->SetShadowVisible(btst(num, 256)); }
  if (btst(num, 3072)) { if (chara) chara->SetSeparateShadow(btst(num, 1024)); }
  if (btst(num, 4096)) { Vector3 v = GetVector3(); if (chara) chara->SetShadowPosition(v); }
  if (btst(num, 8192)) { float a = GetFloat(); if (chara) chara->SetShadowAlpha(a); }
  if (btst(num, 16384)) { Vector3 v = GetVector3(); if (chara) chara->SetFieldShadowPosition(v); }
}
void NativeManager::cmd_chara_sleep_pose() {
  int num = GetInt();
  Chara* chara = Slot(m_chara, num & 0xFF);
  if (btst(num, 768)) { if (chara) chara->SetSleepPose(btst(num, 256)); }
  if (btst(num, 1024)) { if (chara) chara->SetSleepDir(num >> 12); }
}
void NativeManager::cmd_chara_sleep_near() { int a = GetInt(); int b = GetInt(); Chara::SetSleepNear(a, b); }
void NativeManager::cmd_chara_pose() { int num = GetInt(); if (Slot(m_chara, num & 0xFF)) Slot(m_chara, num & 0xFF)->SetTexture(num >> 8); }

void NativeManager::cmd_fbx_load() {
  int num = GetInt(), num2 = GetInt();
  if ((unsigned)num >= 16) { l_error("cmd_fbx_load: bad slot %d", num); return; }
  if (num2 >= 0) {
    if (m_model[num]) { m_model[num]->Release(); delete m_model[num]; }
    m_model[num] = new FbxObject(num2);
    m_model[num]->SetRotation(Vector3(0.f, 180.f, 0.f));
  } else if (m_model[num]) {
    m_model[num]->Release(); delete m_model[num]; m_model[num] = nullptr;
  }
}
void NativeManager::cmd_fbx() {
  int num = GetInt();
  FbxObject* f = Slot(m_model, num & 0xFF);
  if (btst(num, 768)) { if (f) f->SetVisible(btst(num, 256)); }
  if (btst(num, 2048)) { Vector3 v = GetVector3(); if (f) f->SetScale(v); }
  if (btst(num, 4096)) { Color c; c.r = GetFloat(); c.g = GetFloat(); c.b = GetFloat(); c.a = GetFloat(); if (f) f->SetColor(c); }
  if (btst(num, 8192)) { Vector3 v = GetVector3(); if (f) f->SetPosition(v); }
  if (btst(num, 16384)) { Vector3 v = GetVector3(); if (f) f->SetRotation(v); }
  if (btst(num, 32768)) { int seq = GetInt(); if (f) f->PlayAnimation(seq, btst(num, 1024)); m_anim_list.push_back((num & 0xFF) | 0x1000); }
}
void NativeManager::cmd_rise_icon_load() {
  int num = GetInt(), num2 = GetInt();
  if ((unsigned)num < 16) {
    if (num2 >= 0) { if (m_rise[num]) { m_rise[num]->Release(); delete m_rise[num]; } m_rise[num] = new Riseup(num2); }
    else if (m_rise[num]) { m_rise[num]->Release(); delete m_rise[num]; m_rise[num] = nullptr; }
  }
  if (num >= 15) TouchPad::SetPadBlink(num2 >= 0);
}
void NativeManager::cmd_rise_icon() {
  Riseup* r = Slot(m_rise, GetInt());
  Vector3 p = GetVector3();
  float a = GetFloat();
  if (r) { r->SetPosition(p); if (r->m_material) r->m_material->color = Color(1.f, 1.f, 1.f, a); r->m_billboard_request = true; }
}
void NativeManager::cmd_mesh_load() {
  int num = GetInt(), num2 = GetInt();
  if ((unsigned)num >= 24) { l_error("cmd_mesh_load: bad slot %d", num); return; }
  if (num2 >= 0) {
    if (m_dssa[num]) { m_dssa[num]->Release(); delete m_dssa[num]; }
    m_dssa[num] = new MeshChara(num2, num2 < 1000 ? 1 : 3);
    if (m_dssa[num]->m_material) m_dssa[num]->m_material->color = Color(1.f, 1.f, 1.f, 1.f);
  } else if (m_dssa[num]) {
    m_dssa[num]->Release(); delete m_dssa[num]; m_dssa[num] = nullptr;
  }
}
void NativeManager::cmd_mesh() {
  MeshChara* mc = Slot(m_dssa, GetInt());
  Vector3 pos = GetVector3();
  int n = GetInt();
  std::vector<Vector3> vtx(n); std::vector<Vector2> uv(n); std::vector<Color32> col(n);
  Color32 color(255, 255, 255, 255);
  for (int i = 0; i < n; i++) {
    uv[i].x = GetFloat(); uv[i].y = GetFloat();
    vtx[i].x = GetFloat(); vtx[i].y = GetFloat(); vtx[i].z = GetFloat();
    if ((i & 3) == 0) color.a = (uint8_t)GetInt();
    col[i] = color;
  }
  int ni = n * 6 / 4;
  std::vector<int> tri(m_output + m_output_offset, m_output + m_output_offset + ni);
  m_output_offset += ni;
  if (mc) {
    mc->SetPosition(pos);
    Mesh* mesh = mc->m_mesh2;
    if (mesh) {
      mesh->Clear(); mesh->vertices = vtx; mesh->uv = uv; mesh->colors32 = col; mesh->SetTriangles(tri);
      // C#: m_mesh2 = m_meshFilter.mesh; m_meshFilter.mesh = mesh;   (two meshes alternate; direct pointer swap = no clone of the instance getter)
      Mesh* prev = mc->m_meshFilter->sharedMesh ? mc->m_meshFilter->sharedMesh : new Mesh();
      mc->m_meshFilter->sharedMesh = mesh; mc->m_meshFilter->meshIsInstance = true;
      mc->m_mesh2 = prev;
    }
    mc->SetVisible(true);
  }
}
void NativeManager::cmd_mesh_on_off() { int slot = GetInt(); int vis = GetInt(); if (Slot(m_dssa, slot)) m_dssa[slot]->SetVisible(vis != 0); }
void NativeManager::cmd_mesh_shader() { int num = GetInt(); if (Slot(m_dssa, num & 0xFF)) m_dssa[num & 0xFF]->SetShader(num >> 8); }
void NativeManager::cmd_mesh_color() {
  MeshChara* mc = Slot(m_dssa, GetInt());
  Color c; c.r = GetFloat(); c.g = GetFloat(); c.b = GetFloat(); c.a = 1.f;
  if (mc) mc->SetColor(c);
}
void NativeManager::cmd_mesh_tex_offset() {
  float u = GetFloat(), v = GetFloat();
  int slot = GetInt();
  if (Slot(m_dssa, slot)) m_dssa[slot]->SetTextureOffset(u, v);
}
void NativeManager::cmd_camera() {
  int a = GetInt();
  if (btst(a, 1)) SceneCamera::SetPosition(GetVector3());
  if (btst(a, 2)) SceneCamera::SetTarget(GetVector3());
  if (btst(a, 4)) SceneCamera::SetAngleDeg(GetVector3());
  m_camera_flag = 1;
}
void NativeManager::cmd_camera_sub() {
  int a = GetInt();
  if (btst(a, 1)) SceneCamera::SetNear(GetFloat());
  if (btst(a, 2)) SceneCamera::SetFar(GetFloat());
  if (btst(a, 4)) SceneCamera::SetFOV2(GetFloat());
  if (btst(a, 16)) SceneCamera::SetYOffset(GetFloat());
  if (btst(a, 32)) SceneCamera::SetLookDownAdjust(GetFloat());
  if (btst(a, 128)) SceneCamera::SetTargetCamera(false);
}
void NativeManager::cmd_fade() {
  int num = GetInt();
  Fade::SetLayer(btst(num, 16384) ? 1 : 0);
  int frames = num & 0x7FF;
  if (btst(num, 4096)) {
    if (btst(num, 2048)) { Color c; c.r = GetFloat(); c.g = GetFloat(); c.b = GetFloat(); c.a = 1.f; Fade::SetFadeOutColor(frames, c); }
    else if (btst(num, 8192)) Fade::SetFadeOutWhite(frames);
    else Fade::SetFadeOutBlack(frames);
  } else {
    Fade::SetFadeIn(frames);
  }
}
void NativeManager::cmd_map_color_rate() {
  Color c; c.r = GetFloat(); c.g = GetFloat(); c.b = GetFloat(); c.a = 1.f;
  if (m_fld_load) m_fld.SetMaterialColor(c); else Field::SetColor(c);
}
void NativeManager::cmd_back_color() {
  Color up, down;
  up.r = GetFloat(); up.g = GetFloat(); up.b = GetFloat(); up.a = 1.f;
  down.r = GetFloat(); down.g = GetFloat(); down.b = GetFloat(); down.a = 1.f;
  BackColor::SetColor(up, down);
}
void NativeManager::cmd_screen_effect() {
  int v = GetInt();
  Capture::ScreenEffect(v);
}
void NativeManager::cmd_se_play() { Sound::PlaySE(GetInt()); }
void NativeManager::cmd_se_stop() { Sound::StopSE(GetInt()); }
void NativeManager::cmd_bgm() { int num = GetInt(); if (num < 16384) Sound::PlayBGM(num); else Sound::StopBGM(num & 0x3FFF); }

// ---------------------------------------------------------------------------------------------------- the frame (01 §1.2)
void NativeManager::Update() { Update(TouchPad::m_press, TouchPad::m_toggle); }

void NativeManager::Update(int padPress, int padToggle) {
  PadFocus::Mask(padPress, padToggle);        // [WP3] a MenuPortItem screen has the focus: its d-pad / A / B must not also reach the field logic
  m_input_offset = 0;
  if (m_camera_flag > 0) {
    m_camera_flag = 0;
    SetInt(0);
    SetInt((int)(SceneCamera::s_camera_pos.x * 4096.f));
    SetInt((int)(SceneCamera::s_camera_pos.y * 4096.f));
    SetInt((int)(SceneCamera::s_camera_pos.z * -4096.f));
  }
  if (m_fld_load) {
    std::vector<int> ended;
    for (int item : m_anim_list) {
      bool flag = false;
      if (item < 4096) { if (m_fld.IsCommonAnimationEnd(item) != 0) flag = true; }
      else if (Slot(m_model, item & 0xFF)) flag = Slot(m_model, item & 0xFF)->IsEndAnimation();
      if (flag) { ended.push_back(item); SetInt(1); SetInt(item); }
    }
    for (int item : ended) m_anim_list.remove(item);
  }
  SetInt(-1);
  const int nIn = m_input_offset;
  PacketRecorder::PrepareFrame(m_output, 8192, m_output_float, 8192);
  m_output[0] = Global::Flag;
  const int flagIn = Global::Flag;

  if (PacketReplay::Active()) {
    // --replay: the recorded frame replaces the native side (out[], outF[], m_string[0] and the MenuPort native->host mailbox)
    static char s_replayStr[256];
    extern short* dqiv_menu_native_buffer();
    if (PacketReplay::LoadFrame(s_frame, m_output, m_output_float, s_replayStr, sizeof(s_replayStr), dqiv_menu_native_buffer())) {
      m_string[0] = s_replayStr[0] ? s_replayStr : nullptr;
    } else {
      m_output[0] = Global::Flag; m_output[1] = -1;                               // end of trace: empty frames
      static bool s_ended; if (!s_ended) { s_ended = true; l_info("PacketReplay: end of trace after %d frames", PacketReplay::FramesPlayed()); if (PacketReplay::QuitAtEnd()) Application::Quit(); }
    }
  } else {
    perf::Begin(perf::S_NATIVE);
  NativeCall::GameUpdate(padPress, padToggle);
  perf::End(perf::S_NATIVE);
  }

  m_output_offset = 0; m_float_offset = 0; m_input_offset = 0;
  s_stats = FrameStats();
  s_stats.frame = s_frame;
  Global::Flag = GetInt();
  s_stats.flagOut = Global::Flag;
  for (int i = 0; i < 24; i++)
    if (m_dssa[i]) m_dssa[i]->SetVisible(false);
  while (true) {
    int op = GetInt();
    if (op < 0) break;
    if (op >= NUM_CMDS || m_output_offset > 8000) {
      l_error("NativeManager: bad opcode %d at out[%d] (frame %d) - packet stream desynchronised", op, m_output_offset - 1, s_frame);
      s_stats.desync = true;
      break;
    }
    s_stats.cmdCount[op]++; s_stats.nCmds++;
    m_cmd_tbl[op]();
  }
  s_stats.nInts = m_output_offset; s_stats.nFloats = m_float_offset;

  if (PacketRecorder::Enabled() && !PacketReplay::Active()) {
    PacketRecorder::FrameView fv = {};
    fv.frame = s_frame; fv.padPress = padPress; fv.padToggle = padToggle; fv.flagIn = flagIn;
    fv.in = m_input; fv.nIn = nIn;
    // the native side also scribbles ahead of its cursors: record the PARSED stream length (high-water mark only when the parse desynchronised)
    fv.out = m_output; fv.nOut = s_stats.desync ? PacketRecorder::UsedInts(m_output, 8192) : s_stats.nInts;
    fv.outF = m_output_float; fv.nOutF = s_stats.desync ? PacketRecorder::UsedFloats(m_output_float, 8192) : s_stats.nFloats;
    fv.str0 = m_string[0];
    extern short* dqiv_menu_unity_buffer(); extern short* dqiv_menu_native_buffer();
    fv.menuUnity = dqiv_menu_unity_buffer(); fv.nMenuUnity = 129;
    short* nb = dqiv_menu_native_buffer(); fv.menuNative = nb; fv.nMenuNative = nb[64] > 65 && nb[64] <= 1089 ? nb[64] : 65;
    PacketRecorder::Record(fv);
  }
  s_frame++;

  if (m_camera_flag > 0) {
    SceneCamera::Update();
    Chara::UpdateBillboardShadows();
    for (Chara* c : m_chara) if (c) c->SetBillboard();
    for (Riseup* r : m_rise) if (r) r->SetBillboard();
  } else {
    for (Chara* c : m_chara) if (c && c->m_billboard_request) c->SetBillboard();
    for (Riseup* r : m_rise) if (r && r->m_billboard_request) r->SetBillboard();
  }
  for (int n = 0; n < 24; n++)
    if (m_dssa[n]) m_dssa[n]->SetBillboard();
  if (m_fld_load) {
    m_fld.Update();
  } else {
    Field::UpdatePattern();
  }
  if (Sound::s_bgmState != 0) Sound::SystemUpdate();
}

const char* DqvCurrentScene() { return NativeManager::s_sceneName.c_str(); }       // input script `sync` command (platform/vita_input.cpp)

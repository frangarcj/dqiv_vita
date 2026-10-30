// game/SlimeTouch.cpp -- OWNER: WP6. Line-by-line ports of CgController.cs, SlimeTouchBehaviour.cs, SlimeTouchPort.cs.
#include "game/SlimeTouch.hpp"

#include <cfloat>
#include <cstdio>

#include "game/MainLoop.hpp"
#include "game/NativeManager.hpp"
#include "game/SceneCamera.hpp"
#include "utils/logger.h"

namespace {
struct CgInfo { const char* filename; WrapMode wrapMode; };
const CgInfo cgInfo[8] = { { nullptr, WrapMode::Once }, { "start_p", WrapMode::Once }, { "gameover_p", WrapMode::Once }, { "ase_p", WrapMode::Loop },
                           { "board_p", WrapMode::Once }, { "board_p", WrapMode::Once }, { "board_p", WrapMode::Once }, { "board_p", WrapMode::Once } };
const char* DATA_PATH_SLIME = "SlimeTouch/";
struct BoxInfo { Vector3 center, size; };
const BoxInfo boxInfo[8] = {
  { Vector3(-68.f, -75.f, 0.f), Vector3(40.f, 30.f, 0.f) }, { Vector3(1.f, -75.f, 0.f), Vector3(40.f, 30.f, 0.f) }, { Vector3(72.f, -75.f, 0.f), Vector3(40.f, 30.f, 0.f) },
  { Vector3(-28.f, -114.f, 0.f), Vector3(40.f, 30.f, 0.f) }, { Vector3(31.f, -114.f, 0.f), Vector3(40.f, 30.f, 0.f) },
  { Vector3(-68.f, -155.f, 0.f), Vector3(40.f, 30.f, 0.f) }, { Vector3(1.f, -155.f, 0.f), Vector3(40.f, 30.f, 0.f) }, { Vector3(68.f, -155.f, 0.f), Vector3(40.f, 30.f, 0.f) } };
}  // namespace

MenuPort* SlimeTouchPort::s_port = nullptr;

// ------------------------------------------------------------------------------------------------ CgController
void CgController::Setup() {
  for (int i = 1; i < 8; i++) {
    cg[i] = dynamic_cast<GameObject*>(Object::Instantiate(Resources::Load<GameObject>(std::string(DATA_PATH_SLIME) + cgInfo[i].filename)));
    if (!cg[i]) { l_error("CgController: %s%s missing", DATA_PATH_SLIME, cgInfo[i].filename); continue; }
    anim[i] = cg[i]->animation();
    anim[i]->wrapMode = cgInfo[i].wrapMode;
    anim[i]->Play();
    stat[i] = &(*anim[i])[anim[i]->clip->name];
    stat[i]->speed = 0.f;
    cg[i]->GetComponentsInChildren<Renderer>(render[i]);
    if (4 <= i && i <= 7) {
      Renderer* r = cg[i]->GetComponentInChildren<Renderer>();
      mat[i] = r->materials()[1];
      Vector3 position = cg[i]->transform->position();
      position.x += (float)(i - 4) * 42.f;
      cg[i]->transform->setPosition(position);
    }
  }
  tex[0] = Resources::Load<Texture2D>(std::string(DATA_PATH_SLIME) + "board/slime");
  tex[1] = Resources::Load<Texture2D>(std::string(DATA_PATH_SLIME) + "board/bes");
  tex[2] = Resources::Load<Texture2D>(std::string(DATA_PATH_SLIME) + "board/metal");
}

void CgController::Clean() {
  for (int i = 1; i < 8; i++) { if (cg[i]) Object::Destroy(cg[i]); cg[i] = nullptr; anim[i] = nullptr; stat[i] = nullptr; }
}

void CgController::Update() {
  int num = 0;
  for (int i = 1; i < 8; i++) {
    if (!cg[i]) continue;
    int num2 = 1 << i;
    bool flag = (drawBit & num2) != 0;
    if ((playBit & num2) != 0 && !anim[i]->isPlaying()) num |= 1 << (i & 0x1F);
    for (Renderer* r : render[i]) {
      r->enabled = flag;
    }
    stat[i]->speed = flag ? 1.f : 0.f;
  }
  drawBit = 0;
  if (playBit != _playBit) {
    playBit &= ~num & 0x7FFFFFFF;
    SlimeTouchPort::s_port->Send(1, (short)playBit);
    _playBit = playBit;
  } else if (num != 0) {
    playBit &= ~num & 0x7FFFFFFF;
    SlimeTouchPort::s_port->Send(2, (short)playBit);
    _playBit = playBit;
  }
}

void CgController::Draw(int i_cg) { if (0 < i_cg && i_cg < 8) drawBit |= 1 << i_cg; }

void CgController::NormaSet(int id, int i_cg, int track) {
  if (id < 8 || 12 <= id) {
    if (!anim[i_cg]) return;
    anim[i_cg]->Play();
    stat[i_cg]->speed = 0.f;
    playBit |= 1 << i_cg;
  } else if (i_cg >= 4 && 6 >= i_cg && track >= 0 && 5 >= track) {
    id = id - 8 + 4;
    i_cg -= 4;
    if (!anim[id]) return;
    mat[id]->mainTexture = tex[i_cg];
    char name[16]; snprintf(name, sizeof(name), "Take %03d", track + 1);
    stat[id] = &(*anim[id])[name];
    anim[id]->clip = stat[id]->clip;
    anim[id]->Play();
    stat[id]->speed = 0.f;
    playBit |= (1 << id) | (int)0x80000000;
  }
}

// ------------------------------------------------------------------------------------------------ SlimeTouchBehaviour
void SlimeTouchBehaviour::Awake() {
  for (int i = 0; i < 8; i++) {
    BoxCollider* boxCollider = gameObject->AddComponent<BoxCollider>();
    boxCollider->center = boxInfo[i].center;
    boxCollider->size = boxInfo[i].size;
    boxCollider->isTrigger = true;
    boxCollider->enabled = false;
    box[i] = boxCollider;
  }
  controller = new CgController();
  controller->Setup();
}

void SlimeTouchBehaviour::OnDestroy() {
  for (int i = 0; i < 8; i++) { Object::Destroy(box[i]); box[i] = nullptr; }
  controller->Clean();
  delete controller;
  controller = nullptr;
}

void SlimeTouchBehaviour::Update() {
  if ((updateFlag & 1) != 0 && Input::touchCount != 0 && Input::touches[0].phase == TouchPhase::Began) {
    Vector2 position = Input::touches[0].position;
    Ray ray = SceneCamera::s_DSSAOrthoCamera->ScreenPointToRay(Vector3(position.x, position.y, 0.f));
    for (int i = 0; i < 8; i++) {
      RaycastHit hit;
      if (box[i]->Raycast(ray, &hit, FLT_MAX)) { SlimeTouchPort::s_port->Send(0, (short)i); break; }
    }
  }
  controller->Update();
}

void SlimeTouchBehaviour::SetTouchState(bool state) {
  for (int i = 0; i < 8; i++) box[i]->enabled = state;
  if (state) updateFlag |= 1; else updateFlag &= -2;
}

// ------------------------------------------------------------------------------------------------ SlimeTouchPort
SlimeTouchPort::SlimeTouchPort(short port, int type) : MenuPort(port) {
  (void)type;
  behaviour = (new GameObject("SlimeTouchBehaviour"))->AddComponent<SlimeTouchBehaviour>();
  s_port = this;
  Camera* cam = SceneCamera::s_DSSAOrthoCamera;
  cam->clearFlags = CameraClearFlags::Color;
  cam->backgroundColor = Color::black;
  camera_pos = cam->transform()->position();
  if (2.f / 3.f < cam->aspect()) {
    Vector3 position = camera_pos;
    position.y -= 192.f - cam->orthographicSize;
    cam->transform()->setPosition(position);
  }
  MainLoop::m_camera->enabled = false;
  NativeManager::ReleaseStage();
}

void SlimeTouchPort::Close() {
  Object::Destroy(behaviour->gameObject);
  s_port = nullptr;
  Camera* cam = SceneCamera::s_DSSAOrthoCamera;
  cam->clearFlags = CameraClearFlags::Depth;
  cam->transform()->setPosition(camera_pos);
  MainLoop::m_camera->enabled = true;
}

void SlimeTouchPort::NoticeReceive(Param prm) {
  switch (prm[0]) {
    case 0: behaviour->SetTouchState(prm[1] != 0); break;
    case 1: behaviour->controller->Draw(prm[1]); break;
    case 2: behaviour->controller->NormaSet(prm[1], prm[2], prm[3]); break;
  }
}

MenuPort* dqiv_CreateSlimeTouchPort(short port, short type) { return new SlimeTouchPort(port, type); }

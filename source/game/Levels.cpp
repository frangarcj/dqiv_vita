// game/Levels.cpp -- OWNER: WP1/WP3. Numbers: 01_FRAME_LOOP.md §6 (dump of level0 / mainData).
#include "game/Levels.hpp"

#include <cstring>

#include "game/Boot.hpp"
#include "game/MainLoop.hpp"
#include "game/Layout.hpp"
#include "game/TouchPad.hpp"
#include "miniunity.hpp"
#include "utils/logger.h"

using namespace miniunity;

namespace Levels {

void BuildMainData() {
  GameObject* go = new GameObject("Main Camera");
  go->tag = "MainCamera";
  Camera* c = go->AddComponent<Camera>();
  c->orthographic = true; c->orthographicSize = 400.f; c->nearClipPlane = 0.3f; c->farClipPlane = 1000.f;
  c->clearFlags = CameraClearFlags::Color; c->backgroundColor = Color::black; c->cullingMask = -1; c->depth = -1.f;
  go->AddComponent<Boot>();
}

void BuildLevel0() {
  // MenuCamera first: TouchPad.Update must run before MainLoop.Update so this frame's pad reaches GameUpdate (01 §3)
  GameObject* menu = new GameObject("MenuCamera");
  menu->layer = 8;
  menu->transform->localPosition = Vector3(0.f, 100.f, 0.f);
  menu->transform->localRotation = Quaternion::Euler(90.f, 0.f, 0.f);
  Camera* mc = menu->AddComponent<Camera>();
  mc->orthographic = true; mc->orthographicSize = 400.f; mc->nearClipPlane = 1.f; mc->farClipPlane = 200.f;
  mc->clearFlags = CameraClearFlags::Depth; mc->depth = 1.f; mc->cullingMask = 256;
  mc->rect = Layout::UiRect();
  menu->AddComponent<TouchPad>();

  GameObject* main = new GameObject("MainCamera");
  main->tag = "MainCamera";
  main->transform->localPosition = Vector3(0.f, 28.925f, -26.973f);
  Quaternion q; q.x = 0.39875f; q.y = 0.f; q.z = 0.f; q.w = 0.91706f;
  main->transform->localRotation = q;
  Camera* c = main->AddComponent<Camera>();
  c->orthographic = false; c->fieldOfView = 20.f; c->nearClipPlane = 5.f; c->farClipPlane = 100.f;
  c->clearFlags = CameraClearFlags::Depth; c->backgroundColor = Color(0.192f, 0.302f, 0.475f, 0.0196f); c->depth = -1.f; c->cullingMask = 1;
  c->rect = Layout::WorldRect();
  main->AddComponent<MainLoop>();
  l_info("Levels: level0 built (MenuCamera + TouchPad, MainCamera + MainLoop)");
}

void Build(const char* name) {
  if (!strcmp(name, "mainData")) BuildMainData();
  else if (!strcmp(name, "dq4sp")) BuildLevel0();
  else l_error("Levels::Build(%s): unknown level", name);
}

}  // namespace Levels

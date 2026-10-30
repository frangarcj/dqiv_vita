// miniunity/engine.hpp -- engine lifecycle entry points.         OWNER: WP1 (RunUpdate/LateUpdate/LoadLevel), WP2 (Init/Render/Present)
#pragma once
#include <vector>
#include "miniunity/coroutine.hpp"
#include "miniunity/object.hpp"

namespace miniunity {

void StartCoroutine(Coroutine c);          // free-standing runner (no owner): resumes once per frame after Update(); honours WaitForSeconds using Time.deltaTime

// Prefab MonoBehaviours: Resources.Load builds `MonoBehaviour` property lists into the component registered under the script class name (fields are NOT applied,
// the owner of the class reads what it needs). Unregistered scripts are skipped.
void RegisterScriptFactory(const char* className, Component* (*factory)());

namespace Engine {
  // vitaGL set-up, logical screen = viewW x viewH placed at (viewX0,viewY0) on the 960x544 display (06 §1).
  bool Init(int viewW, int viewH, int viewX0, int viewY0, bool rotated90);
  void BeginFrame();                       // Time update; (input is polled by platform/vita_input before this)
  void RunUpdate();                        // Start() queue, Update of all enabled components, then coroutine resume
  void RunLateUpdate();
  void Render();                           // cameras by depth into the logical-surface FBO (03 §4)
  void Present();                          // blit the logical surface centred to the 960x544 backbuffer + swap (interval 2 => 30 Hz)
  void EndFrame();                         // deferred Destroy flush, Time.frameCount++
  void LoadLevel(const char* name);        // destroys non-DontDestroyOnLoad objects, then runs the registered level builder ("dq4sp" = level0, "mainData")
  using LevelBuilder = void (*)(const char* name);
  void SetLevelBuilder(LevelBuilder b);    // game/Levels.cpp registers Level0::Build / MainData::Build here
  void Shutdown();
  // ---- WP1 additions
  // Instantiates a pack scene prefab ("scene/level0", "scene/maindata": several roots) and returns the root GameObjects in order (scene objects, not assets).
  std::vector<GameObject*> InstantiateScene(const char* resPath);
  // Debug aid: runs the engine self tests (pack prefab/mesh/material/anim/skin/leak checks, results in the log) when the file ux0:data/dqiv/enginetest.on exists.
  // Returns true if tests ran. Call once after Resources::Init().
  bool RunSelfTestsIfRequested();
}

}  // namespace miniunity

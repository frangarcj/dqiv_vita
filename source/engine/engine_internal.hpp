// engine/engine_internal.hpp -- private glue between the engine/*.cpp files (WP1 + WP2). Not part of the game-facing API.
#pragma once
#include <functional>
#include <vector>
#include "miniunity.hpp"

namespace miniunity { namespace engine {

extern std::vector<GameObject*> g_gameObjects;      // all live SCENE GameObjects, creation order (prefab asset nodes are not in here)
extern std::vector<Component*> g_startQueue;        // components awaiting Start()
extern bool g_assetBuild;                            // true while a prefab asset tree is being built (GameObjects become inert asset nodes)
extern uint32_t g_delVersion;                        // bumped on every GameObject/Component deletion
bool IsLive(Object* o);
void ForEachLive(const std::function<void(Object*)>& f);   // snapshot iteration over every live Object (assets, GameObjects, components)
size_t LiveCount();
void FlushDestroy();                                 // end of frame
void DestroyAllExceptDontDestroy();                  // LoadLevel
void PurgeReferences(Object* o);                     // null every pointer to `o` held by live materials / renderers / filters / animations / fonts

void StartCoroutineOwned(Coroutine c, Component* owner);   // owner may be null
void StepCoroutines(float dt);                       // after Update()
void ClearCoroutinesOfDestroyed();

void ProcessStartQueue();

// ---- animation + skinning (animation.cpp), driven by Engine::RunUpdate / RunLateUpdate
void TickAnimations(float dt);                       // after Update() + coroutines, before LateUpdate()
void SkinAll();                                      // end of LateUpdate(): CPU skinning of every active SkinnedMeshRenderer

// ---- pack glue (pack_glue.cpp)
void ResourcesSweep();                               // Resources::UnloadUnusedAssets implementation
void ResourcesOnObjectDeleted(Object* o);            // drop cache entries pointing to o

// ---- self tests (selftest.cpp); returns the number of failed checks
int RunSelfTests(const char* fbx, const char* fld, const char* fld2);

// ---- render side (WP2), see engine/renderer.cpp ----
void RenderAllCameras();

}}  // namespace miniunity::engine

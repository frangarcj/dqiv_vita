// miniunity.hpp -- umbrella header of the "mini-Unity" API the C# presentation layer is ported against.
// Goal: translated code stays line-for-line close to the C# (same member names, same call shapes).
// Everything single-threaded (game thread). Contract text: re/arch/03_MINI_UNITY.md. Split by owner:
//   miniunity/math.hpp        value types                         (WP1)
//   miniunity/coroutine.hpp   IEnumerator on C++20 coroutines     (WP1)
//   miniunity/object.hpp      Object, Component, Transform, GameObject (WP1)
//   miniunity/assets.hpp      Texture, Material, Mesh, Shader, Font, Resources  (WP1 data, WP2 GL/shader side)
//   miniunity/components.hpp  Renderer, MeshFilter/Renderer, TextMesh, Camera, Collider, Animation (WP1/WP2)
//   miniunity/statics.hpp     Time, Screen, Application, Input, Debug (WP1; Input filled by WP3)
//   miniunity/engine.hpp      Engine::Init/RunUpdate/Render/Present...   (WP1/WP2)
// Changes to these headers: note them in the header comment (PR-style) and tell the owning WP.
#pragma once
#include "miniunity/math.hpp"
#include "miniunity/coroutine.hpp"
#include "miniunity/object.hpp"
#include "miniunity/assets.hpp"
#include "miniunity/components.hpp"
#include "miniunity/statics.hpp"
#include "miniunity/engine.hpp"

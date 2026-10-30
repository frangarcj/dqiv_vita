// miniunity/components.hpp -- built-in components.
// OWNER: WP1: MeshFilter, colliders, Animation.   WP2: Renderer, MeshRenderer, TextMesh, Camera (and the render loop that consumes them).
//
// WP1 additions (additive, 2026-10-27): SkinnedMeshRenderer (+SkinnedMeshFilter helper, CPU skinning feeds the same MeshFilter/MeshRenderer path WP2 already draws),
// full legacy Animation (Play/Stop/Rewind/CrossFade/PlayQueued/AddClip, states with weights, WrapMode), colliders implemented (engine/colliders.cpp).
// Renderer::material()/materials() and MeshFilter::mesh() moved to engine/components.cpp (WP1) with Unity instancing semantics (instances are tagged
// Material::instanceOwner / engineOwned and freed by Resources::UnloadUnusedAssets()); materials()/material()/mesh() now live in components.cpp, not renderer.cpp.
#pragma once
#include "miniunity/assets.hpp"

namespace miniunity {

class Renderer : public Component { public: int RenderKind() const override { return 2; } bool castShadows=false, receiveShadows=false;      // `enabled` comes from Component
  Material* material();                       // returns instance (clones shared material on first access, like Unity)
  void setMaterial(Material*); std::vector<Material*> materials(); void setMaterials(const std::vector<Material*>&);
  Material* sharedMaterial = nullptr; void setSharedMaterial(Material*);
  std::vector<Material*> mats;                // all materials (index = submesh); mats[0] == sharedMaterial / the instance
  bool materialIsInstance = false; };
class MeshFilter : public Component { public: int RenderKind() const override { return 1; } Mesh* mesh(); /* instance copy on first access */ void setMesh(Mesh*); Mesh* sharedMesh = nullptr; bool meshIsInstance = false; };
class MeshRenderer : public Renderer {};
// Skinned mesh (49 fbx prefabs): `sharedMesh` is the bind-pose mesh (shared pack asset); every frame (end of LateUpdate) Skin() computes the posed vertices in this
// renderer's LOCAL space into `skinned` and publishes it through a helper MeshFilter on the same GameObject, so the renderer draws it like any MeshRenderer.
class SkinnedMeshFilter : public MeshFilter {};
class SkinnedMeshRenderer : public MeshRenderer { public:
  Mesh* sharedMesh = nullptr; std::vector<Transform*> bones; Transform* rootBone = nullptr; bool updateWhenOffscreen = false;
  Mesh* skinned = nullptr;                                  // runtime mesh (engine-owned), same uv/colors/triangles as sharedMesh
  SkinnedMeshRenderer(); SkinnedMeshRenderer(const SkinnedMeshRenderer&); ~SkinnedMeshRenderer();
  void Skin();                                              // CPU skinning now (also called by the engine each frame)
  void RemapReferences(const std::unordered_map<Transform*, Transform*>&) override; };
class TextMesh : public Component { public: int RenderKind() const override { return 3; }   // see 03_MINI_UNITY.md "TextMesh"
  std::string text; Font* font = nullptr; float characterSize=1; float lineSpacing=1; int fontSize=0; int anchor/*TextAnchor*/=0; int alignment/*TextAlignment*/=0; bool richText=true; int fontStyle=0; Color color=Color::white; float offsetZ=0; float tabSize=4;
  void Awake() override;                       // WP2: like Unity's RequireComponent, adds the MeshRenderer (engine/textmesh.cpp)
  ~TextMesh();                                 // WP2: generated glyph mesh + the parameters it was built from (copies start with an empty cache)
  struct Cache { void* p = nullptr; Cache() {} Cache(const Cache&) {} Cache& operator=(const Cache&) { return *this; } } cache;
};
enum class CameraClearFlags { Skybox=1, Color=2, Depth=3, Nothing=4 };
class Camera : public Component { public: int RenderKind() const override { return 4; }
  bool orthographic=false; float orthographicSize=100, fieldOfView=60, nearClipPlane=0.3f, farClipPlane=1000; float depth=0; int cullingMask=-1;
  CameraClearFlags clearFlags=CameraClearFlags::Skybox; Color backgroundColor; Rect rect=Rect(0,0,1,1); RenderTexture* targetTexture=nullptr;
  float aspect() const;                       // = (pixelWidth/pixelHeight) of the camera rect unless overridden (UNVERIFIED rect-aware; see doc)
  void setProjectionMatrix(const Matrix4x4&); Matrix4x4 projectionMatrix();       // custom projection set by SceneCamera.Update
  void ResetProjectionMatrix() { hasCustomProjection = false; }
  Rect pixelRect() const; void setPixelRect(Rect);
  Ray ScreenPointToRay(Vector3 screenPos) const;      // origin = point on near plane (ortho: z irrelevant, origin.z taken from screenPos.z in RawPads), direction = forward
  Vector3 WorldToScreenPoint(Vector3) const; Vector3 WorldToViewportPoint(Vector3) const; Vector3 ScreenToWorldPoint(Vector3) const;
  void Render();                                // immediate render of this camera's layers into targetTexture (or screen) - used by Capture
  static Camera* main(); static Camera* mainCamera(){return main();}       // first enabled camera tagged "MainCamera"
  Matrix4x4 customProjection{}; bool hasCustomProjection = false; };
class Collider : public Component { public: virtual bool Raycast(const Ray&, RaycastHit* hit, float maxDist) const = 0; };
class BoxCollider : public Collider { public: Vector3 center, size; bool isTrigger=false; bool Raycast(const Ray&, RaycastHit*, float) const override; };
class SphereCollider : public Collider { public: Vector3 center; float radius=0.5f; bool Raycast(const Ray&, RaycastHit*, float) const override; };
class MeshCollider : public Collider { public: Mesh* sharedMesh=nullptr; /* triangle raycast; UI frames are axis-aligned quads so a bounds test is acceptable */ bool Raycast(const Ray&, RaycastHit*, float) const override; };
// WP2: Graphics.Blit for OnRenderImage (Sepia): draws the full-screen quad of `mat`'s shader over the current camera target (Blit(src,dst,mat) with
// dst == the camera target). `src` is the engine's screen-copy RenderTexture handed to OnRenderImage; it is filled from the target on demand.
struct Graphics { static void Blit(Texture* src, Texture* dst, Material* mat); };
// Unity 4 legacy Animation. States are created from `clips` (name = clip name, "Take 001"...). Semantics (engine/animation.cpp):
//  Play(): stops every other state at once, (re)starts this one at weight 1 (a state that is already playing is not rewound). CrossFade(): target ramps 0->1, the others ->0
//  over t seconds, then are disabled. PlayQueued(): starts when no other state is playing. isPlaying: any state enabled (ClampForever never ends).
//  Effective wrap mode: state.wrapMode, else Animation.wrapMode, else clip.wrapMode, else Once. Ticked once per frame after Update()/coroutines, before LateUpdate();
//  inactive GameObjects / disabled Animations do not advance. States addresses are stable (CgController keeps AnimationState*).
class Animation : public Component { public:
  AnimationClip* clip = nullptr;                     // default clip (Play() with no name)
  WrapMode wrapMode = WrapMode::Default; bool playAutomatically = true;
  std::vector<AnimationClip*> clips;                 // serialized clip list (m_Animations)
  Animation(); Animation(const Animation&); ~Animation();
  bool isPlaying() const; bool IsPlaying(const std::string& name) const;
  bool Play(); bool Play(const std::string& name);
  void Stop(); void Stop(const std::string& name); void Rewind(); void Rewind(const std::string& name);
  void CrossFade(const std::string&, float t = 0.3f); void PlayQueued(const std::string&);
  void AddClip(AnimationClip* c, const std::string& name); int GetClipCount() const { return (int)states.size(); }
  AnimationState& operator[](const std::string&);    // missing name: returns a detached dummy state (Unity returns null)
  std::unordered_map<std::string, AnimationState> states;
  std::vector<std::string> queue;                    // PlayQueued
  void Awake() override;                             // playAutomatically
  void Tick(float dt);                               // engine-internal
  void SyncStates(); };                              // creates states for `clips`/`clip`
// MonoBehaviour-like game scripts derive from Component and are created with AddComponent<T>()

}  // namespace miniunity

// miniunity/object.hpp -- Object / Component / Transform / GameObject.     OWNER: WP1 (engine core)
//
// Translation conventions (C# -> C++), all porters follow them:
//   null                    -> nullptr                       `X x = go.GetComponent<X>()` -> `X* x = go->GetComponent<X>()`
//   obj.transform.parent=p  -> obj->transform->SetParent(p)  (keepWorld=true like Unity 4's setter: world TRS preserved)
//   properties              -> public fields where trivially stored (localPosition, localScale, localRotation, color, enabled, layer, name, ...)
//                              and get/set method pairs only where Unity computes (position(), setPosition(), rotation(), eulerAngles(), ...)
//   string                  -> std::string ; arrays -> std::vector<T> ; List<T> -> std::vector<T> ; Dictionary -> std::unordered_map
//   delegates               -> std::function ; IEnumerator -> miniunity::Coroutine (C++20, co_yield 0 / co_yield WaitForSeconds(x))
//   Object.Destroy(o)       -> Object::Destroy(o) deferred to end of frame; DestroyImmediate immediate.
//   Debug.Log               -> Debug::Log(fmt,...)
//
// WP1 additions (additive, 2026-10-27): Object ctor/dtor + registry (every Object is tracked: asset sweeps, dangling-safe Destroy), engineOwned flag,
// Instantiate(o,pos,rot), Component::{GetComponent,AddComponent,GetComponentsInChildren,GetComponentInChildren} forwarders (C# calls them on Transforms too),
// Component::RemapReferences (Instantiate), Transform::{Find,root,TransformPoint,InverseTransformPoint,TransformDirection,forward,right,up},
// GameObject::{isAsset,GetComponents,GetComponentInChildren,FindWithTag}, AttachComponent(c, callAwake).
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <typeinfo>
#include "miniunity/math.hpp"
#include "miniunity/coroutine.hpp"

namespace miniunity {

class GameObject; class Transform; class Renderer; class MeshRenderer; class MeshFilter; class Camera; class Collider; class Animation; class TextMesh;
class Material; class Mesh; class Texture; class Texture2D; class Shader; class Font;

struct Object { std::string name;
  Object(); Object(const Object& o); Object& operator=(const Object& o) { name = o.name; return *this; } virtual ~Object();   // registry: every Object is tracked (engine/object.cpp)
  static void Destroy(Object*, float t=0); static void DestroyImmediate(Object*); static void DontDestroyOnLoad(Object*);
  // Instantiate: GameObject -> GameObject*, Transform -> Transform* (the clone's root, named "<name>(Clone)", no parent), Material/Mesh -> copies.
  // Deep clone of the hierarchy incl. components (typeid clone registry, engine/object.cpp); Materials/Meshes of renderers stay SHARED until accessed through .material/.mesh like Unity.
  static Object* Instantiate(Object* original);
  static Object* Instantiate(Object* original, Vector3 position, Quaternion rotation);     // root placed at world position/rotation
  template<class T> static T* Instantiate(T* original) { return dynamic_cast<T*>(Instantiate(static_cast<Object*>(original))); }
  template<class T> static T* Instantiate(T* original, Vector3 p, Quaternion r) { return dynamic_cast<T*>(Instantiate(static_cast<Object*>(original), p, r)); }
  uint32_t instanceId=0; bool destroyed=false; bool dontDestroyOnLoad=false;
  /* assets only: true = created by the engine (pack loads, renderer.material / meshFilter.mesh instances) and freed by Resources::UnloadUnusedAssets()
     when no live scene object references it. false (default) = created by game code (`new Material`, `new Texture2D`, Object::Instantiate(material)): never swept. */
  bool engineOwned=false;
  bool resCached=false;                              // engine-internal: present in the Resources caches (pack_glue.cpp)
};

class Component : public Object { public: GameObject* gameObject=nullptr; Transform* transform() const; GameObject* go() const{return gameObject;}
  Renderer* renderer() const; Camera* camera() const; Collider* collider() const; Animation* animation() const;  // legacy shortcuts used by the C# (`.renderer`, `.camera` ...)
  virtual void Awake(){} virtual void Start(){} virtual void Update(){} virtual void LateUpdate(){} virtual void OnDestroy(){}
  virtual void OnRenderImage(Texture* /*src*/, Texture* /*dst*/){}
  virtual int RenderKind() const { return 0; }             // WP2: 1 MeshFilter, 2 Renderer (incl. Mesh/SkinnedMeshRenderer), 3 TextMesh, 4 Camera: lets the render loop classify components without dynamic_cast (slow on the Vita CPU)
  virtual bool HasRenderImage() const { return false; }     // WP2: true on components that override OnRenderImage (the camera then renders into the screen-sized source texture first)
  virtual void OnApplicationPause(bool){} virtual void OnApplicationQuit(){}
  // C# `IEnumerator Start()`: override StartCo() (and HasCoStart() -> true); the engine runs it like Unity does (first MoveNext where Start would run)
  virtual bool HasCoStart() const { return false; }
  virtual Coroutine StartCo() { co_return; }
  void StartCoroutine(Coroutine c);               // C# `StartCoroutine(Foo())`: bound to this component (stops when it is destroyed / its GameObject is inactive)
  bool enabled = true;
  bool startCalled = false;
  // Object::Instantiate: after the whole hierarchy is cloned, components holding Transform* into the source hierarchy fix them up here (SkinnedMeshRenderer bones...)
  virtual void RemapReferences(const std::unordered_map<Transform*, Transform*>&) {}
  // C# calls these on any Component (`transform.GetComponentsInChildren<Renderer>()`, `renderer.gameObject.GetComponent<X>()`): forwarded to the GameObject
  template<class T> T* GetComponent();
  template<class T> T* AddComponent();
  template<class T> void GetComponentsInChildren(std::vector<T*>& out, bool includeInactive=false);
  template<class T> T* GetComponentInChildren(bool includeInactive=false); };

class Transform : public Component { public:
  Vector3 localPosition; Quaternion localRotation; Vector3 localScale = Vector3::one;     // plain fields (55+25+11 uses in the C#)
  Transform* parent() const { return parent_; } void SetParent(Transform* p, bool keepWorld=true);   // C# `t.parent = p`
  int childCount() const { return (int)children_.size(); } Transform* GetChild(int i) const { return children_[i]; }
  /* range-for support: for (Transform* c : t->children()) */ const std::vector<Transform*>& children() const { return children_; }
  Vector3 position() const; void setPosition(Vector3); Quaternion rotation() const; void setRotation(Quaternion);
  Vector3 lossyScale() const;
  Vector3 eulerAngles() const; void setEulerAngles(Vector3); Vector3 localEulerAngles() const; void setLocalEulerAngles(Vector3);
  void Translate(float x,float y,float z);       // relative to self (Space.Self)
  void Translate(Vector3 v);
  void Rotate(float x,float y,float z);          // Space.Self, applies Quaternion.Euler(x,y,z) on the right
  void LookAt(Vector3 target, Vector3 up = Vector3::up);
  Matrix4x4 localToWorldMatrix() const;           // computed on demand (WP1 may add dirty-flag caching)
  Matrix4x4 worldToLocalMatrix() const;
  int GetChildCount() const{return childCount();}
  Transform* Find(const std::string& path) const;       // child by name or "a/b/c" path (includes inactive); nullptr if missing
  Transform* root();                                    // C# `transform.root`
  Vector3 TransformPoint(Vector3) const; Vector3 InverseTransformPoint(Vector3) const; Vector3 TransformDirection(Vector3) const;
  Vector3 forward() const { return rotation() * Vector3::forward; } Vector3 right() const { return rotation() * Vector3::right; } Vector3 up() const { return rotation() * Vector3::up; }
 private: Transform* parent_ = nullptr; std::vector<Transform*> children_; };

class GameObject : public Object { public:
  int layer = 0; std::string tag; bool activeSelf = true; Transform* transform = nullptr;
  bool isAsset = false;                             // part of a Resources.Load'ed prefab tree: not in the scene, never updated/rendered, Instantiate source only
  GameObject(); explicit GameObject(const std::string& name); ~GameObject();
  bool activeInHierarchy() const;
  bool active() const{return activeInHierarchy();}  void setActive(bool b);        // C# `go.active = b` == SetActive(b)  (Unity 4 obsolete alias)
  void SetActive(bool b);                                                           // sets activeSelf only; children inherit
  void SetActiveRecursively(bool b);                                                // sets activeSelf of self AND all descendants (Unity 4 behaviour)
  template<class T> T* AddComponent() { T* c = new T(); AttachComponent(c); return c; }      // runs Awake() immediately
  template<class T> T* GetComponent() { for (Component* c : components) if (T* t = dynamic_cast<T*>(c)) return t; return nullptr; }
  template<class T> void GetComponents(std::vector<T*>& out) { for (Component* c : components) if (T* t = dynamic_cast<T*>(c)) out.push_back(t); }
  template<class T> T* GetComponentInChildren(bool includeInactive=false) { std::vector<T*> v; GetComponentsInChildren<T>(v, includeInactive); return v.empty() ? nullptr : v[0]; }
  // **Unity 4 default skips inactive GameObjects (and their subtrees)** - FldObject relies on it
  template<class T> void GetComponentsInChildren(std::vector<T*>& out, bool includeInactive=false) {
    if (!includeInactive && !activeSelf) return;
    for (Component* c : components) if (T* t = dynamic_cast<T*>(c)) out.push_back(t);
    for (Transform* ch : transform->children()) ch->gameObject->GetComponentsInChildren<T>(out, includeInactive); }
  Renderer* renderer(); Camera* camera(); Collider* collider(); Animation* animation();
  static GameObject* CreatePrimitive(int primitiveType /*PrimitiveType::Quad only*/);
  static GameObject* Find(const std::string&);
  static GameObject* FindWithTag(const std::string&);
  std::vector<Component*> components;               // engine-owned, creation order (Transform first)
  void AttachComponent(Component* c, bool callAwake = true); };
enum PrimitiveType { Quad = 5 };

template<class T> T* Component::GetComponent() { return gameObject ? gameObject->GetComponent<T>() : nullptr; }
template<class T> T* Component::AddComponent() { return gameObject ? gameObject->AddComponent<T>() : nullptr; }
template<class T> void Component::GetComponentsInChildren(std::vector<T*>& out, bool includeInactive) { if (gameObject) gameObject->GetComponentsInChildren<T>(out, includeInactive); }
template<class T> T* Component::GetComponentInChildren(bool includeInactive) { return gameObject ? gameObject->GetComponentInChildren<T>(includeInactive) : nullptr; }

// Object::Instantiate clones components through this registry (built-in engine components are registered by the engine). A game component that must survive
// Instantiate() (a MonoBehaviour that is part of a cloned hierarchy) calls RegisterComponentClone<MyComponent>() once; it needs a usable copy constructor.
namespace engine { void RegisterCloneFn(const std::type_info&, Component* (*)(const Component*)); }
template<class T> void RegisterComponentClone() { engine::RegisterCloneFn(typeid(T), [](const Component* c) -> Component* { return new T(*static_cast<const T*>(c)); }); }

}  // namespace miniunity

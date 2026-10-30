// engine/object.cpp -- Object / Component / Transform / GameObject / Instantiate (WP1).
//
// Ownership: GameObjects are heap objects owned by the engine registry; components are owned by their GameObject.
// Destroy() is deferred to Engine::EndFrame (FlushDestroy); DestroyImmediate() deletes at once.
// Every Object (assets included) is tracked in a registry so that stale pointers are detected (IsLive) and asset sweeps can enumerate them.
//
// Prefab assets (Resources.Load of a prefab) are real GameObject trees built in "asset mode" (isAsset): not in g_gameObjects, no Awake, no Start,
// never updated/rendered; Instantiate() clones them into the scene (components through a typeid clone registry).
#include "miniunity.hpp"
#include "engine/engine_internal.hpp"
#include "utils/logger.h"

#include <algorithm>
#include <typeindex>
#include <unordered_set>

namespace miniunity {

namespace engine {
std::vector<GameObject*> g_gameObjects;              // scene objects, creation order
std::vector<Component*> g_startQueue;                // components awaiting Start()
bool g_assetBuild = false;                           // GameObjects created now are prefab asset nodes
uint32_t g_delVersion = 0;                           // bumped by every deletion (loops re-validate when it changes)
static std::vector<Object*> g_pendingDestroy;
static std::unordered_set<Object*>& Live() { static std::unordered_set<Object*>* s = new std::unordered_set<Object*>(); return *s; }
static uint32_t g_nextId = 1;

bool IsLive(Object* o) { return o && Live().count(o); }
void ForEachLive(const std::function<void(Object*)>& f) { std::vector<Object*> v(Live().begin(), Live().end()); for (Object* o : v) if (Live().count(o)) f(o); }
size_t LiveCount() { return Live().size(); }

static void RemoveFromScene(GameObject* g) {
  if (g->isAsset) return;
  auto it = std::find(g_gameObjects.begin(), g_gameObjects.end(), g);
  if (it != g_gameObjects.end()) g_gameObjects.erase(it);
}
static void CompactScene() {
  g_gameObjects.erase(std::remove_if(g_gameObjects.begin(), g_gameObjects.end(), [](GameObject* g) { return !IsLive(g); }), g_gameObjects.end());
}
static void DropFromStartQueue(Component* c) {
  if (!g_startQueue.empty()) g_startQueue.erase(std::remove(g_startQueue.begin(), g_startQueue.end(), c), g_startQueue.end());
}

static void MarkDestroyed(GameObject* g) {
  if (g->destroyed) return;
  g->destroyed = true;
  for (Component* c : g->components) c->destroyed = true;
  for (Transform* t : g->transform->children()) MarkDestroyed(t->gameObject);
}

// bulk: the caller compacts g_gameObjects afterwards (avoids O(n^2) erases when a whole map is torn down)
static void DeleteGameObject(GameObject* g, bool bulk) {
  if (!IsLive(g)) return;
  g_delVersion++;
  // children first (copy: deleting a child unparents it)
  std::vector<Transform*> kids = g->transform->children();
  for (Transform* t : kids) DeleteGameObject(t->gameObject, bulk);
  for (Component* c : g->components) c->destroyed = true;
  if (!g->isAsset) for (Component* c : g->components) if (IsLive(c)) c->OnDestroy();
  g->transform->SetParent(nullptr, false);
  if (!bulk) RemoveFromScene(g);
  std::vector<Component*> comps;
  comps.swap(g->components);
  for (Component* c : comps) { if (!g->isAsset) DropFromStartQueue(c); delete c; }
  g->transform = nullptr;
  delete g;
}

static void DeleteComponent(Component* c) {
  if (!IsLive(c) || !c->gameObject) return;
  GameObject* g = c->gameObject;
  if (c == g->transform) return;                       // Unity: cannot destroy a Transform alone
  g_delVersion++;
  c->OnDestroy();
  g->components.erase(std::remove(g->components.begin(), g->components.end(), c), g->components.end());
  DropFromStartQueue(c);
  delete c;
}

void PurgeReferences(Object* o);                       // components.cpp: clears every pointer to `o` held by live materials/renderers/filters/animations

static void DeleteAsset(Object* o) {
  PurgeReferences(o);
  delete o;
}

void FlushDestroy() {
  // Destroy() while flushing (OnDestroy handlers) is queued for the next frame
  std::vector<Object*> list;
  list.swap(g_pendingDestroy);
  bool anyGO = false;
  for (Object* o : list) {
    if (!IsLive(o)) continue;
    if (auto* g = dynamic_cast<GameObject*>(o)) { DeleteGameObject(g, true); anyGO = true; }
    else if (auto* c = dynamic_cast<Component*>(o)) DeleteComponent(c);
    else DeleteAsset(o);
  }
  if (anyGO) CompactScene();
}

void DestroyAllExceptDontDestroy() {
  std::vector<GameObject*> roots;
  for (GameObject* g : g_gameObjects) if (!g->transform->parent() && !g->dontDestroyOnLoad) roots.push_back(g);
  for (GameObject* g : roots) DeleteGameObject(g, true);
  CompactScene();
  g_pendingDestroy.erase(std::remove_if(g_pendingDestroy.begin(), g_pendingDestroy.end(), [](Object* o) { return !IsLive(o); }), g_pendingDestroy.end());
}

// ------------------------------------------------------------------------------------------------ clone registry
typedef Component* (*CloneFn)(const Component*);
static std::unordered_map<std::type_index, CloneFn>& CloneRegistry() { static auto* m = new std::unordered_map<std::type_index, CloneFn>(); return *m; }
void RegisterCloneFn(const std::type_info& t, CloneFn f) { CloneRegistry()[std::type_index(t)] = f; }
template <class T> static void Reg() { RegisterCloneFn(typeid(T), [](const Component* c) -> Component* { return new T(*static_cast<const T*>(c)); }); }
static void EnsureBuiltinClones() {
  static bool done = false;
  if (done) return;
  done = true;
  Reg<MeshFilter>(); Reg<SkinnedMeshFilter>(); Reg<MeshRenderer>(); Reg<SkinnedMeshRenderer>(); Reg<Camera>(); Reg<Animation>();
  Reg<BoxCollider>(); Reg<SphereCollider>(); Reg<MeshCollider>(); Reg<TextMesh>();
}
}  // namespace engine

using namespace engine;

// ------------------------------------------------------------------------------------------------ Object
Object::Object() { instanceId = g_nextId++; Live().insert(this); }
Object::Object(const Object& o) : name(o.name), engineOwned(o.engineOwned) { instanceId = g_nextId++; Live().insert(this); }
Object::~Object() { if (resCached) engine::ResourcesOnObjectDeleted(this); Live().erase(this); }

void Object::Destroy(Object* o, float t) {
  (void)t;                                              // delayed destroy is not used by the game
  if (!IsLive(o) || o->destroyed) return;
  if (auto* g = dynamic_cast<GameObject*>(o)) MarkDestroyed(g); else o->destroyed = true;
  g_pendingDestroy.push_back(o);
}
void Object::DestroyImmediate(Object* o) {
  if (!IsLive(o)) return;
  if (auto* g = dynamic_cast<GameObject*>(o)) DeleteGameObject(g, false);
  else if (auto* c = dynamic_cast<Component*>(o)) DeleteComponent(c);
  else DeleteAsset(o);
}
void Object::DontDestroyOnLoad(Object* o) {
  GameObject* g = dynamic_cast<GameObject*>(o);
  if (!g) if (auto* c = dynamic_cast<Component*>(o)) g = c->gameObject;
  if (g) { while (g->transform->parent()) g = g->transform->parent()->gameObject; g->dontDestroyOnLoad = true; }   // a child marks its root
  else if (o) o->dontDestroyOnLoad = true;
}

// ---- Instantiate
namespace {
struct CloneCtx {
  std::unordered_map<Transform*, Transform*> remap;
  std::vector<Component*> comps;                         // every cloned component (RemapReferences + Awake)
  std::unordered_map<Component*, Component*> compMap;    // source component -> clone (Instantiate(Component))
};

GameObject* CloneGO(GameObject* src, Transform* parent, CloneCtx& cx) {
  bool savedMode = g_assetBuild;
  g_assetBuild = false;                                  // clones are always scene objects
  GameObject* g = new GameObject(src->name);
  g_assetBuild = savedMode;
  g->layer = src->layer; g->tag = src->tag; g->activeSelf = src->activeSelf;
  Transform* t = g->transform; Transform* st = src->transform;
  t->localPosition = st->localPosition; t->localRotation = st->localRotation; t->localScale = st->localScale;
  if (parent) t->SetParent(parent, false);
  cx.remap[st] = t;
  cx.compMap[st] = t;
  for (Component* c : src->components) {
    if (c == st) continue;
    auto it = CloneRegistry().find(std::type_index(typeid(*c)));
    if (it == CloneRegistry().end()) { Debug::LogError("Instantiate: component type %s has no clone function (RegisterComponentClone<T>()), skipped", typeid(*c).name()); continue; }
    Component* nc = it->second(c);
    nc->gameObject = nullptr; nc->startCalled = false; nc->destroyed = false; nc->dontDestroyOnLoad = false;
    g->AttachComponent(nc, false);
    cx.comps.push_back(nc);
    cx.compMap[c] = nc;
  }
  for (Transform* ch : st->children()) CloneGO(ch->gameObject, t, cx);
  return g;
}

GameObject* CloneHierarchy(GameObject* src, CloneCtx& cx) {
  EnsureBuiltinClones();
  GameObject* root = CloneGO(src, nullptr, cx);
  for (Component* c : cx.comps) c->RemapReferences(cx.remap);
  for (Component* c : cx.comps) if (c->gameObject && c->gameObject->activeInHierarchy()) c->Awake();
  root->name = src->name + "(Clone)";
  root->transform->name = root->name;
  return root;
}
}  // namespace

Object* Object::Instantiate(Object* original) {
  if (!original) { Debug::LogError("Object::Instantiate: null original"); return nullptr; }
  if (auto* g = dynamic_cast<GameObject*>(original)) { CloneCtx cx; return CloneHierarchy(g, cx); }
  if (auto* c = dynamic_cast<Component*>(original)) {
    if (!c->gameObject) return nullptr;
    CloneCtx cx; GameObject* root = CloneHierarchy(c->gameObject, cx);
    auto it = cx.compMap.find(c);
    return it != cx.compMap.end() ? static_cast<Object*>(it->second) : static_cast<Object*>(root);
  }
  if (auto* m = dynamic_cast<Material*>(original)) { Material* n = new Material(*m); n->name = m->name + "(Clone)"; n->engineOwned = false; n->instanceOwner = nullptr; return n; }
  if (auto* m = dynamic_cast<Mesh*>(original)) { Mesh* n = new Mesh(*m); n->name = m->name + "(Clone)"; n->engineOwned = false; n->packId = 0; n->revision++; return n; }
  Debug::LogError("Object::Instantiate: unsupported object type %s", typeid(*original).name());
  return nullptr;
}
Object* Object::Instantiate(Object* original, Vector3 position, Quaternion rotation) {
  Object* o = Instantiate(original);
  Transform* t = nullptr;
  if (auto* g = dynamic_cast<GameObject*>(o)) t = g->transform; else if (auto* c = dynamic_cast<Component*>(o)) t = c->gameObject ? c->gameObject->transform : nullptr;
  if (t) { t->localPosition = position; t->localRotation = rotation; }       // the clone is a root: world == local
  return o;
}

// ------------------------------------------------------------------------------------------------ Component
Transform* Component::transform() const { return gameObject ? gameObject->transform : nullptr; }
Renderer* Component::renderer() const { return gameObject ? gameObject->renderer() : nullptr; }
Camera* Component::camera() const { return gameObject ? gameObject->camera() : nullptr; }
Collider* Component::collider() const { return gameObject ? gameObject->collider() : nullptr; }
Animation* Component::animation() const { return gameObject ? gameObject->animation() : nullptr; }
void Component::StartCoroutine(Coroutine c) { engine::StartCoroutineOwned(std::move(c), this); }

// ------------------------------------------------------------------------------------------------ GameObject
GameObject::GameObject() : GameObject("GameObject") {}
GameObject::GameObject(const std::string& n) {
  name = n;
  isAsset = g_assetBuild;
  if (isAsset) engineOwned = true; else g_gameObjects.push_back(this);
  transform = new Transform();
  transform->gameObject = this;
  transform->name = n;
  components.push_back(transform);
}
GameObject::~GameObject() {}

void GameObject::AttachComponent(Component* c, bool callAwake) {
  c->gameObject = this;
  c->name = name;
  components.push_back(c);
  if (isAsset) return;                                  // prefab asset nodes are inert
  g_startQueue.push_back(c);
  if (callAwake) c->Awake();
}
bool GameObject::activeInHierarchy() const {
  for (const Transform* t = transform; t; t = t->parent()) if (!t->gameObject->activeSelf) return false;
  return true;
}
void GameObject::SetActive(bool b) { activeSelf = b; }
void GameObject::setActive(bool b) { SetActive(b); }
void GameObject::SetActiveRecursively(bool b) {
  activeSelf = b;
  for (Transform* t : transform->children()) t->gameObject->SetActiveRecursively(b);
}
template <class T> static T* FindComp(GameObject* g) { for (Component* c : g->components) if (T* t = dynamic_cast<T*>(c)) return t; return nullptr; }
Renderer* GameObject::renderer() { return FindComp<Renderer>(this); }
Camera* GameObject::camera() { return FindComp<Camera>(this); }
Collider* GameObject::collider() { return FindComp<Collider>(this); }
Animation* GameObject::animation() { return FindComp<Animation>(this); }
GameObject* GameObject::Find(const std::string& n) {
  for (GameObject* g : g_gameObjects) if (g->activeInHierarchy() && g->name == n) return g;
  return nullptr;
}
GameObject* GameObject::FindWithTag(const std::string& tag) {
  for (GameObject* g : g_gameObjects) if (g->activeInHierarchy() && g->tag == tag) return g;
  return nullptr;
}

// ------------------------------------------------------------------------------------------------ Transform
static Vector3 Inv(Vector3 s) { return Vector3(s.x != 0 ? 1.f / s.x : 0, s.y != 0 ? 1.f / s.y : 0, s.z != 0 ? 1.f / s.z : 0); }

Vector3 Transform::position() const {
  Vector3 p = localPosition;
  for (const Transform* t = parent_; t; t = t->parent_) p = t->localPosition + t->localRotation * Vector3::Scale(p, t->localScale);
  return p;
}
Quaternion Transform::rotation() const {
  Quaternion q = localRotation;
  for (const Transform* t = parent_; t; t = t->parent_) q = t->localRotation * q;
  return q;
}
Vector3 Transform::lossyScale() const {
  Vector3 s = localScale;
  for (const Transform* t = parent_; t; t = t->parent_) s = Vector3::Scale(s, t->localScale);
  return s;
}
static Vector3 InverseTransformPointOf(const Transform* t, Vector3 p) {
  if (t->parent()) p = InverseTransformPointOf(t->parent(), p);
  return Vector3::Scale(Quaternion::Inverse(t->localRotation) * (p - t->localPosition), Inv(t->localScale));
}
void Transform::setPosition(Vector3 p) { localPosition = parent_ ? InverseTransformPointOf(parent_, p) : p; }
void Transform::setRotation(Quaternion q) { localRotation = parent_ ? Quaternion::Inverse(parent_->rotation()) * q : q; }
Vector3 Transform::eulerAngles() const { return rotation().eulerAngles(); }
void Transform::setEulerAngles(Vector3 d) { setRotation(Quaternion::Euler(d)); }
Vector3 Transform::localEulerAngles() const { return localRotation.eulerAngles(); }
void Transform::setLocalEulerAngles(Vector3 d) { localRotation = Quaternion::Euler(d); }
void Transform::Translate(float x, float y, float z) { Translate(Vector3(x, y, z)); }
void Transform::Translate(Vector3 v) { localPosition = localPosition + localRotation * v; }
void Transform::Rotate(float x, float y, float z) { localRotation = localRotation * Quaternion::Euler(x, y, z); }
void Transform::LookAt(Vector3 target, Vector3 up) { setRotation(Quaternion::LookRotation(target - position(), up)); }
Matrix4x4 Transform::localToWorldMatrix() const {
  Matrix4x4 m = Matrix4x4::TRS(localPosition, localRotation, localScale);
  for (const Transform* t = parent_; t; t = t->parent_) m = Matrix4x4::TRS(t->localPosition, t->localRotation, t->localScale) * m;
  return m;
}
Matrix4x4 Transform::worldToLocalMatrix() const {
  // inverse of a TRS chain: S^-1 R^-1 T^-1, innermost first
  Matrix4x4 m = Matrix4x4::identity();
  for (const Transform* t = this; t; t = t->parent_) {
    Matrix4x4 inv = Matrix4x4::TRS(Vector3(), Quaternion::Inverse(t->localRotation), Inv(t->localScale)) *
                    Matrix4x4::TRS(Vector3() - t->localPosition, Quaternion::identity, Vector3::one);
    m = m * inv;
  }
  return m;
}
Vector3 Transform::TransformPoint(Vector3 p) const { return localToWorldMatrix().MultiplyPoint3x4(p); }
Vector3 Transform::InverseTransformPoint(Vector3 p) const { return InverseTransformPointOf(this, p); }
Vector3 Transform::TransformDirection(Vector3 d) const { return rotation() * d; }
Transform* Transform::root() { Transform* t = this; while (t->parent_) t = t->parent_; return t; }
Transform* Transform::Find(const std::string& path) const {
  const Transform* cur = this;
  size_t pos = 0;
  while (cur && pos <= path.size()) {
    size_t sl = path.find('/', pos);
    std::string seg = path.substr(pos, sl == std::string::npos ? std::string::npos : sl - pos);
    const Transform* next = nullptr;
    for (const Transform* c : cur->children_) if (c->gameObject->name == seg) { next = c; break; }
    cur = next;
    if (sl == std::string::npos) break;
    pos = sl + 1;
  }
  return const_cast<Transform*>(cur == this ? nullptr : cur);
}
void Transform::SetParent(Transform* p, bool keepWorld) {
  if (p == parent_) return;
  for (Transform* a = p; a; a = a->parent_) if (a == this) { Debug::LogError("Transform::SetParent: cycle"); return; }
  Vector3 wp, ws; Quaternion wr;
  if (keepWorld) { wp = position(); ws = lossyScale(); wr = rotation(); }
  if (parent_) parent_->children_.erase(std::remove(parent_->children_.begin(), parent_->children_.end(), this), parent_->children_.end());
  parent_ = p;
  if (p) p->children_.push_back(this);
  if (keepWorld) {
    localPosition = p ? InverseTransformPointOf(p, wp) : wp;
    localRotation = p ? Quaternion::Inverse(p->rotation()) * wr : wr;
    Vector3 ps = p ? p->lossyScale() : Vector3::one;
    localScale = Vector3::Scale(ws, Inv(ps));
  }
}

}  // namespace miniunity

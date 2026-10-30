// miniunity/assets.hpp -- Texture / Material / Mesh / Shader / Font / Resources / TextAsset / AnimationClip.
// OWNER: WP1 (data classes, Resources + pack glue)   WP2 (Shader table, RenderTexture, Font/TextMesh glyph data, GL upload)
//
// WP1 additions (additive, 2026-10-27): Texture2D lazy pack residency (packId, MakeResident/Evict; GetNativeTextureID() is now out-of-line and uploads on demand),
// Material property blocks (colors/floats/vectors/textures/keywords, instanceOwner), Mesh skin data + bounds-on-SetTriangles + revision counter,
// Font lookup table, AnimationClip curves (keyed, linear/slerp), AnimationState, Resources::Load(path, ResKind) + typed Load<T> for every kind, memory stats.
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <type_traits>
#include "miniunity/object.hpp"

namespace miniunity {

enum class TextureFormat { Alpha8=1, ARGB4444=2, RGB24=3, RGBA32=4, ARGB32=5, RGB565=7, RGBA4444=13, ETC_RGB4=34 };   // Unity enum values
enum class FilterMode { Point=0, Bilinear=1, Trilinear=2 }; enum class TextureWrapMode { Repeat=0, Clamp=1 };
class Texture : public Object { public: int width=0,height=0; FilterMode filterMode=FilterMode::Bilinear; TextureWrapMode wrapMode=TextureWrapMode::Repeat; virtual unsigned GetNativeTextureID() const{return 0;} };
class Texture2D : public Texture { public:
  // Used for: 512x512 Alpha8 message canvas (native writes via glTexSubImage2D!), 256x256 ARGB32 + 1024x1024 RGB24 map images (SetPixels32+Apply)
  Texture2D(int w,int h,TextureFormat f,bool mipmap=false);
  ~Texture2D();
  Texture2D() : format(TextureFormat::RGBA32) {}        // for FromPack
  TextureFormat format; void SetPixels32(const Color32*,int n); void Apply(bool mipmaps=false);
  unsigned GetNativeTextureID() const override;      // the REAL GL texture name; pack textures are uploaded here on first use (and again after Evict())
  // ---- WP1: pack-backed lazy residency. packId != 0: pixels can be re-read from the pack at any time, so the GL copy is a cache
  uint32_t packId = 0;                               // gid in the pack (0 = runtime texture: never evicted)
  bool MakeResident() const;                         // upload now (no-op if already resident / runtime texture)
  void Evict();                                      // release the GL texture of a pack-backed texture (UnloadUnusedAssets does it for unreferenced ones)
  size_t ResidentBytes() const;                      // GL memory held right now (0 if evicted)
  uint32_t lastUse = 0;                              // Time::frameCount of the last GetNativeTextureID()
  // pack-backed textures (Resources.Load): pixels straight from the DQTX blob. fmt = DQIV_TF_* (1 A8, 2 RGB565, 3 RGBA4444, 4 RGBA8888)
  static Texture2D* FromPack(const char* name, int w, int h, int packFormat, int wrap, int filter, int origFormat, const void* pixels);
  unsigned glId = 0; int packFormat = 0; int origFormat = 0;                    // origFormat: Unity TextureFormat of the source asset (RGBA32 vs RGB24 decides fld_atest/fld_normal)
  void ApplySampler();                                                          // pushes filterMode/wrapMode to GL (WP2)
};
class RenderTexture : public Texture { public: RenderTexture(int w,int h,int depthBits); ~RenderTexture(); /* FBO: color texture + depth */
  unsigned GetNativeTextureID() const override { return colorTex; }
  unsigned fbo = 0, colorTex = 0, depthRb = 0; };
class Shader : public Object { public: static Shader* Find(const std::string& name);   // "Custom/Chara", "BackColor", "Custom/Capture"... maps to a ShaderDesc in the table (04_SHADERS.md)
  int id = -1; /* index into the 51-entry table (engine/shaders.cpp) */
  int DefaultQueue() const;                          // the shader's own render queue (Material.renderQueue == -1 uses it; FldObject tests `renderQueue == 3000` on the effective value)
};
class Material : public Object { public:
  Material(Shader*); Material(const Material& cloneOf);            // `new Material(other)`   (engine/material.cpp)
  Shader* shader; Color color = Color::white; Texture* mainTexture = nullptr; int renderQueue = -1;   // -1 = shader default; setters: `m->renderQueue = 3000+prio`
  Vector2 mainTextureOffset, mainTextureScale = Vector2(1,1);
  // `color` == "_Color", `mainTexture`/`mainTextureOffset`/`mainTextureScale` == "_MainTex" (+_ST); every other property lives in these tables
  // (pack materials fill them: _Ambient/_Emission/_LColor/_LDir/_SpecColor/_Shininess...; WP2 reads them to set shader uniforms)
  struct TexEnv { Texture* tex = nullptr; Vector2 scale = Vector2(1,1), offset; };
  std::unordered_map<std::string, Color> colors;                   // SetColor("_OrgColor", c) ...
  std::unordered_map<std::string, float> floats;                   // SetFloat("_Brightness", f) ...
  std::unordered_map<std::string, Matrix4x4> matrices;             // SetMatrix("_sleepMatrix", m) (DQ4 Chara sleep shaders)
  std::unordered_map<std::string, Color> vectors;                  // SetVector("_LDir", x,y,z,w): the 4 floats are stored as r,g,b,a
  std::unordered_map<std::string, TexEnv> textures;                // non-_MainTex texture slots
  std::vector<std::string> keywords;
  Object* instanceOwner = nullptr;                                 // the Renderer whose `.material` getter created this per-renderer instance (null = shared asset)
  uint32_t packId = 0;                                             // source gid when loaded from the pack
  void SetTextureOffset(const char* prop, Vector2); void SetTextureScale(const char* prop, Vector2);
  void SetColor(const char* prop, Color); Color GetColor(const char* prop) const;
  void SetFloat(const char* prop, float); float GetFloat(const char* prop) const;
  void SetVector(const char* prop, float x, float y, float z, float w); Color GetVector(const char* prop) const;
  void SetMatrix(const char* prop, const Matrix4x4& m) { matrices[prop] = m; } Matrix4x4 GetMatrix(const char* prop) const { auto it = matrices.find(prop); return it == matrices.end() ? Matrix4x4::identity() : it->second; }
  void SetTexture(const char* prop, Texture* t); Texture* GetTexture(const char* prop) const;
  bool HasProperty(const char* prop) const; };
struct SkinWeight { uint8_t bone[4]; float weight[4]; };            // pack DqvSkinVertex: indices into Mesh::bindposes / SkinnedMeshRenderer::bones
class Mesh : public Object { public:
  std::vector<Vector3> vertices; std::vector<Vector2> uv; std::vector<Color> colors; std::vector<Color32> colors32; std::vector<Vector3> normals;
  std::vector<std::vector<int>> subTriangles; int subMeshCount() const { return (int)subTriangles.size(); }  void SetSubMeshCount(int n){ subTriangles.resize(n); }   // C# `mesh.subMeshCount = n`
  void SetTriangles(const std::vector<int>& t, int sub=0){ if (sub >= (int)subTriangles.size()) subTriangles.resize(sub+1); subTriangles[sub] = t; RecalculateBounds(); revision++; }   // Unity: assigning triangles recalculates bounds
  void SetTriangles(const int* t,int n,int sub=0){ SetTriangles(std::vector<int>(t,t+n), sub); }
  void setTriangles(const std::vector<int>& t){ SetTriangles(t, 0); }            // C# `mesh.triangles = arr`
  std::vector<int> triangles() const { return subTriangles.empty() ? std::vector<int>() : subTriangles[0]; }
  void Clear(){ vertices.clear(); uv.clear(); colors.clear(); colors32.clear(); normals.clear(); subTriangles.clear(); revision++; }
  void Optimize(){} void MarkDynamic(){ dynamic = true; } void RecalculateBounds(); Bounds bounds;
  // skinned meshes (29 in the pack, flag DQIV_MF_SKIN): CPU skinning by SkinnedMeshRenderer, see engine/animation.cpp
  std::vector<SkinWeight> boneWeights; std::vector<Matrix4x4> bindposes;
  // GPU hook for WP2: contents changed since the last upload when `revision` differs from what the backend recorded. Code that fills `vertices`/`uv`/... directly
  // may call SetDirty(); SetTriangles/Clear/RecalculateBounds bump it themselves. `dynamic` = MarkDynamic() was called (update-every-frame meshes).
  uint32_t revision = 1; bool dynamic = false; void SetDirty() { revision++; }
  // WP2 render caches (engine/camera.cpp + engine/backend_vitagl.cpp): u16 index arrays per submesh (valid while `revision` is unchanged) and, for immutable pack meshes,
  // GL buffer objects (interleaved pos/uv/colour + one index buffer per submesh). Copies start without buffers; the destructor releases them through g_meshGpuFree.
  struct GpuCache {
    uint32_t idxRev = 0; std::vector<std::vector<uint16_t>> idx16; std::vector<int> idxMax;
    unsigned vbo = 0; std::vector<unsigned> ibo;   /* ibo[k] per submesh, ibo[subMeshCount] = tinted colour buffer */ Color tintKey; int tintStable = 0; int vboVerts = 0, vboStride = 0; bool vboUv = false, vboCol = false; int vboFail = 0;
    GpuCache() {} GpuCache(const GpuCache& o) : idxRev(o.idxRev), idx16(o.idx16), idxMax(o.idxMax) {}
    GpuCache& operator=(const GpuCache& o) { idxRev = o.idxRev; idx16 = o.idx16; idxMax = o.idxMax; return *this; } };
  mutable GpuCache gpu;
  ~Mesh();
  uint32_t packId = 0;                                                            // source gid (0 = runtime mesh)
  Object* instanceOwner = nullptr;                                                // MeshFilter whose `.mesh` getter created this per-filter instance
  int vertexCount() const { return (int)vertices.size(); } };
extern void (*g_meshGpuFree)(unsigned vbo, const std::vector<unsigned>& ibo);   // set by the backend
class Font : public Object { public:   // static bitmap fonts (Unity "Font" with m_CharacterRects): SE-GARRIR (EN, 218 glyphs, 256x256 A8), GMaruGoPro-M(JA 9911), FOTK-YDHopeL(KO), FZHTK_TL(TC), FZHTK_SL(SC)
  struct CharInfo { int index; Rect uv; Rect vert; float width; bool flipped; };   // verbatim from CharacterRects
  std::vector<CharInfo> chars; Material* material = nullptr; Texture2D* texture = nullptr; float lineSpacing = 0, ascent = 0, fontSize = 0, pixelScale = 0.1f;
  std::unordered_map<int,int> lookup;                // code point -> index in `chars` (built by the loader / on first GetCharacterInfo)
  bool dynamic = false;                              // TrueType-only font (mplus-2c-medium_16): no glyph rects, dropped by the port
  bool GetCharacterInfo(uint16_t ch, CharInfo& out) const; };
struct TextAsset : Object { std::vector<uint8_t> bytes; std::string text() const { return std::string(bytes.begin(), bytes.end()); } };

enum class WrapMode { Once=1, Loop=2, PingPong=4, Default=0, ClampForever=8, Clamp=1 };
// Keyed curves exactly as serialized (no tangents: linear / slerp between keys, all shipped clips are 60 Hz bakes).
struct AnimCurve { std::string path;               // "a/b/c" relative to the GameObject that owns the Animation ("" = itself)
  int type = 0;                                    // 0 position, 1 rotation, 2 scale, 3 float property (ignored)
  std::string attr; int classId = 0;               // float curves only
  int stride = 4;                                  // floats per key: position/scale {t,x,y,z}=4, rotation {t,x,y,z,w}=5, float {t,v}=2
  std::vector<float> keys; int keyCount() const { return stride ? (int)keys.size() / stride : 0; } };
struct AnimationClip : Object { float length = 0; float sampleRate = 60; WrapMode wrapMode = WrapMode::Default; std::vector<AnimCurve> curves; uint32_t packId = 0; };
struct AnimationState { std::string name; AnimationClip* clip = nullptr; WrapMode wrapMode = WrapMode::Default; float time = 0, speed = 1, weight = 1; bool enabled = false; int layer = 0;
  float length() const { return clip ? clip->length : 0.f; }
  float normalizedTime() const { return length() > 0 ? time / length() : 0.f; } void setNormalizedTime(float n) { time = n * length(); }
  float fadeTarget = 1, fadeRate = 0;              // engine-internal blend (CrossFade)
  bool queuedStart = false; };

enum class ResKind { Any = 0, Texture, Mesh, Material, Shader, Prefab, Anim, Text, Font };
struct Resources {
  // Resources.Load over the pack. Any = Prefab > Texture > Text > Font > Material > Mesh > Anim > Shader. Results are cached per (path, kind); gid-level sharing
  // makes a Material/Mesh/Texture used by many prefabs a single object (like Unity).
  static Object* Load(const std::string& path, ResKind kind = ResKind::Any);
  // T in {Texture2D, Texture, TextAsset, Transform (prefab root transform), GameObject (prefab asset root), Font, Material, Mesh, Shader, AnimationClip, Object (any)}
  template<class T> static T* Load(const std::string& path) {
    if constexpr (std::is_base_of_v<Texture, T>) return dynamic_cast<T*>(Load(path, ResKind::Texture));
    else if constexpr (std::is_same_v<T, TextAsset>) return dynamic_cast<T*>(Load(path, ResKind::Text));
    else if constexpr (std::is_same_v<T, Font>) return dynamic_cast<T*>(Load(path, ResKind::Font));
    else if constexpr (std::is_same_v<T, Material>) return dynamic_cast<T*>(Load(path, ResKind::Material));
    else if constexpr (std::is_same_v<T, Mesh>) return dynamic_cast<T*>(Load(path, ResKind::Mesh));
    else if constexpr (std::is_same_v<T, Shader>) return dynamic_cast<T*>(Load(path, ResKind::Shader));
    else if constexpr (std::is_same_v<T, AnimationClip>) return dynamic_cast<T*>(Load(path, ResKind::Anim));
    else if constexpr (std::is_same_v<T, GameObject>) return dynamic_cast<T*>(Load(path, ResKind::Prefab));
    else if constexpr (std::is_same_v<T, Transform>) { GameObject* g = dynamic_cast<GameObject*>(Load(path, ResKind::Prefab)); return g ? g->transform : nullptr; }
    else return dynamic_cast<T*>(Load(path, ResKind::Any)); }
  // WP2: bounded texture residency. If the GL copies of pack textures exceed `maxBytes`, evicts the least recently used ones that were not drawn in the last `minAgeFrames`
  // frames (they re-upload on demand). Returns the number evicted. Called from the frame loop every 60 frames (DQIV_TEX_BUDGET_MB).
  static int EnforceTextureBudget(size_t maxBytes, uint32_t minAgeFrames = 2);
  static Object* LoadByGid(uint32_t gid);                      // pack object by id (builtin-bit ids map to engine defaults)
  // UnloadAsset: deletes the asset now and clears every reference to it (materials' textures, renderers' meshes/materials). Shaders are never deleted.
  static void UnloadAsset(Object*);
  // Mark & sweep: roots = live scene components + game-owned assets; unreferenced engine-owned assets (prefab trees, pack meshes/materials/clips/text, renderer
  // instances) are deleted, unreferenced pack textures lose their GL copy (re-uploaded on demand). Called by LoadLevel and by game/Cleaner.
  static void UnloadUnusedAssets();
  static bool Init(const char* packDir);                        // opens the pack (engine/pack_glue.cpp)
  struct Stats { int live = 0, gameObjects = 0, components = 0, materials = 0, meshes = 0, textures = 0, texturesResident = 0, textAssets = 0, clips = 0, prefabs = 0;
                 size_t texBytes = 0, meshBytes = 0, cacheEntries = 0; };
  static Stats GetStats();                                      // counts of live Objects by class + resident GL texture bytes + CPU mesh bytes
  static void LogStats(const char* tag);                        // one-line summary to the log (+ heap in use)
};


}  // namespace miniunity

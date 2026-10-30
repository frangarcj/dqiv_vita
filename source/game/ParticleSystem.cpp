// game/ParticleSystem.cpp -- OWNER: WP5a. Port of ParticleSystem.cs. SystemInit/ResetNativePacket are real (they hand the static arrays to libdq4core,
// which retains the pointers); the five op-40 functions are stubs.
#include "game/ParticleSystem.hpp"
#include "native/native_bridge.hpp"

int ParticleSystem::s_triangles[1176];
Vector3 ParticleSystem::s_posScale[2];
Vector3 ParticleSystem::s_vertices[784];
Vector2 ParticleSystem::s_uvs[784];
Color32 ParticleSystem::s_colors[784];

static GameObject* s_self = nullptr; static Mesh* s_mesh = nullptr; static MeshRenderer* s_renderer = nullptr; static Material* s_mat = nullptr;

static void setup() {
  s_self = new GameObject();
  s_mesh = s_self->AddComponent<MeshFilter>()->mesh();
  s_mesh->MarkDynamic();
  s_mesh->SetSubMeshCount(1);
  s_renderer = s_self->AddComponent<MeshRenderer>();
  s_mat = new Material(Shader::Find("Custom/Particle"));
  s_mat->mainTexture = Resources::Load<Texture2D>("etc/particles");
  s_renderer->setMaterial(s_mat);
  s_renderer->enabled = true;
}
static void c02_updateWithTriangles(int param);
static void c00_init_town(int param) { setup(); c02_updateWithTriangles(param); }
static void c01_init_field(int param) { setup(); s_self->layer = 9; c02_updateWithTriangles(param); }
static void c02_updateWithTriangles(int param) {
  if (!s_self) return;
  s_self->transform->setPosition(ParticleSystem::s_posScale[0]);
  s_self->transform->localScale = ParticleSystem::s_posScale[1];
  s_mesh->vertices.assign(ParticleSystem::s_vertices, ParticleSystem::s_vertices + 784);
  s_mesh->uv.assign(ParticleSystem::s_uvs, ParticleSystem::s_uvs + 784);
  s_mesh->colors32.assign(ParticleSystem::s_colors, ParticleSystem::s_colors + 784);
  param >>= 8;
  if (param > 1176) param = 1176;
  if (param < 0) param = 0;
  s_mesh->SetTriangles(ParticleSystem::s_triangles, param, 0);       // bumps the revision (uploads vertices/uv/colours too)
}
static void c03_update(int param) {
  (void)param;
  if (!s_self) return;
  s_self->transform->setPosition(ParticleSystem::s_posScale[0]);
  s_mesh->vertices.assign(ParticleSystem::s_vertices, ParticleSystem::s_vertices + 784);
  s_mesh->uv.assign(ParticleSystem::s_uvs, ParticleSystem::s_uvs + 784);
  s_mesh->colors32.assign(ParticleSystem::s_colors, ParticleSystem::s_colors + 784);
  s_mesh->SetDirty();
}
static void c04_release(int param) {
  (void)param;
  if (!s_self) return;
  Object::Destroy(s_mat); Object::Destroy(s_self);
  s_self = nullptr; s_mesh = nullptr; s_renderer = nullptr; s_mat = nullptr;
}
ParticleSystem::FUNCS ParticleSystem::s_funcs[5] = { c00_init_town, c01_init_field, c02_updateWithTriangles, c03_update, c04_release };

void ParticleSystem::SystemInit() {
  s_triangles[0] = 196;
  NativeCall::ParticleSystemInit((float*)s_posScale, s_triangles, (float*)s_vertices, (float*)s_uvs, (uint8_t*)s_colors);
}
void ParticleSystem::ResetNativePacket() {
  NativeCall::ParticleSystemResetNativePacket((float*)s_posScale, s_triangles, (float*)s_vertices, (float*)s_uvs, (uint8_t*)s_colors);
}

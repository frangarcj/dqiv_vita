// game/Volcano.cpp -- OWNER: WP6. Line-by-line port of Volcano.cs.
#include "game/Volcano.hpp"

#include <cstdlib>
#include <vector>

void Volcano::Update() {
  Color white = Color::white;
  int num = 0;                                           // Utility.Rand(1) == Random.Next(1) == 0
  if (((m_counter + 1) & 3) == 0)
    for (int i = 0; i < 3; i++) m_smoke[i] = (m_smoke[i] + 2) % 24;
  for (int j = 0; j < 3; j++) {
    int num2 = m_smoke[j];
    float size = -10.f + (float)(20 * num2) / 24.f + (float)(2 * num);
    white.a = (float)(31 * (24 - num2) / 24) / 31.f;
    SetVerticesColors(8 + j * 4, 4.f - (float)num, 4.f - (float)(4 * (24 - num2)) / 24.f, 16.f + (float)num2 + (float)num, size, m_smoke_w, m_smoke_h, white);
  }
  m_counter++;
  m_mesh->vertices.assign(m_vertices, m_vertices + 20);
  m_mesh->colors.assign(m_colors, m_colors + 20);
}

void Volcano::SetVerticesColors(int index, float x, float y, float z, float size, float w, float h, Color color) {
  m_vertices[index].Set(x - size, y, z + size);
  m_vertices[index + 1].Set(w + x + size, y, z + size);
  m_vertices[index + 2].Set(w + x + size, y, 0.f - h + z - size);
  m_vertices[index + 3].Set(x - size, y, 0.f - h + z - size);
  m_colors[index] = color; m_colors[index + 1] = color; m_colors[index + 2] = color; m_colors[index + 3] = color;
}

void Volcano::Init(Mesh* mesh, Material* mat, float w, float h, Rect mountain, Rect volcano, Rect smoke) {
  m_mesh = mesh;
  m_material = new Material(*mat);
  m_material->shader = Shader::Find("Custom/FieldVolcano");
  renderer()->setMaterials(std::vector<Material*>{ m_material, mat });
  SetVerticesColors(0, 0.f, 0.f, 0.f, 0.f, w, h, Color::white);
  SetVerticesColors(4, 4.f, 8.f, 7.f, 0.f, (volcano.width - volcano.x) * 1024.f, (volcano.height - volcano.y) * 1024.f, Color::white);
  m_smoke_w = (smoke.width - smoke.x) * 1024.f;
  m_smoke_h = (smoke.height - smoke.y) * 1024.f;
  Update();
  m_mesh->uv = {
    Vector2(mountain.x, mountain.y), Vector2(mountain.width, mountain.y), Vector2(mountain.width, mountain.height), Vector2(mountain.x, mountain.height),
    Vector2(volcano.x, volcano.y), Vector2(volcano.width, volcano.y), Vector2(volcano.width, volcano.height), Vector2(volcano.x, volcano.height),
    Vector2(smoke.x, smoke.y), Vector2(smoke.width, smoke.y), Vector2(smoke.width, smoke.height), Vector2(smoke.x, smoke.height),
    Vector2(smoke.x, smoke.y), Vector2(smoke.width, smoke.y), Vector2(smoke.width, smoke.height), Vector2(smoke.x, smoke.height),
    Vector2(smoke.x, smoke.y), Vector2(smoke.width, smoke.y), Vector2(smoke.width, smoke.height), Vector2(smoke.x, smoke.height) };
  m_mesh->SetSubMeshCount(2);
  m_mesh->SetTriangles(std::vector<int>{ 0, 1, 3, 1, 2, 3 }, 1);
  m_mesh->SetTriangles(std::vector<int>{ 4, 5, 7, 5, 6, 7, 8, 9, 11, 9, 10, 11, 12, 13, 15, 13, 14, 15, 16, 17, 19, 17, 18, 19 }, 0);
}

void Volcano::OnDestroy() {
  Object::Destroy(m_material);
  m_material = nullptr;
  m_mesh = nullptr;
}

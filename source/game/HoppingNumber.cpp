// game/HoppingNumber.cpp -- OWNER: WP4a.  C# source: re/cs_out/Assembly-CSharp/HoppingNumber.cs
#include "game/HoppingNumber.hpp"

static const float c_db[38] = { 0.f, 0.f, -4.f, -4.f, -6.f, -6.f, -6.f, -6.f, -4.f, -4.f, -2.f, -2.f, 0.f, 0.f, -2.f, -2.f, 0.f, 0.f, 0.f, 0.f,
                                0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };

void HoppingNumber::Create(Vector3 dispPos, int number) { (new GameObject())->AddComponent<HoppingNumber>()->Setup(dispPos, number); }

void HoppingNumber::Setup(Vector3 dispPos, int number) {
  int num = 1000;
  Vector3 zero = Vector3::zero;
  float num2 = (float)MenuParts::s_info[11].m_dps;
  int num3 = 1;
  m_maker = new MeshMaker(MenuParts::s_info[m_nowParts = 11].GetMaterial(false));
  bool flag = false;
  while (num != 0) {
    int num4 = number / num % 10;
    if (num4 != 0 || flag) {
      AddIcon(zero, num4, true, 2.f);
      zero.x += num2;
      m_waits[m_num] = -num3;
      m_num++;
      num3 += 4;
      flag = true;
    }
    num /= 10;
  }
  Build(4, Vector3::zero);
  m_meshFilter = GetComponent<MeshFilter>();
  m_mesh = new Mesh(*OwnMesh(m_meshFilter));          // Object.Instantiate(m_meshFilter.mesh)
  m_vertices = m_mesh->vertices;
  m_uv = m_mesh->uv;
  Mesh* mesh = OwnMesh(m_meshFilter);
  for (int i = 0, num5 = m_num << 2; i < num5; i++) mesh->uv[i].y = 0.f;
  SetVisible(true);
  transform()->setPosition(dispPos);
  transform()->localScale = Vector3(0.8f, 1.f, 0.8f);
}

void HoppingNumber::internalUpdate() {
  int num = 0;
  Mesh* shown = OwnMesh(m_meshFilter);
  // C# double-buffers two meshes (m_mesh <-> filter.mesh); the observable effect is "uv/vertices recomputed from the saved originals each frame".
  std::vector<Vector2>& uv = shown->uv;
  std::vector<Vector3>& vertices = shown->vertices;
  int i = 0, num2 = 0;
  for (; i < m_num; i++) {
    m_waits[i] += 2;
    if (m_waits[i] >= 0) {
      if (m_waits[i] < 38) {
        float num3 = c_db[m_waits[i]];
        for (int num4 = 0; num4 < 4; num4++) {
          vertices[num2].z = m_vertices[num2].z - num3;
          uv[num2] = m_uv[num2];
          num2++;
        }
      } else {
        num++;
        for (int j = 0; j < 4; j++) uv[num2++].y = 0.f;
      }
    } else {
      for (int k = 0; k < 4; k++) uv[num2++].y = 0.f;
    }
  }
  shown->RecalculateBounds();
  if (num == m_num) {
    delete m_mesh; m_mesh = nullptr;
    Object::Destroy(gameObject);
  }
}

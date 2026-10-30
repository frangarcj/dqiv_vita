// game/BlinkMapSymbol.cpp -- OWNER: WP5c.  Port of BlinkMapSymbol.cs.
#include "game/BlinkMapSymbol.hpp"

void BlinkMapSymbol::SetParameter(MenuPort::Param prm) {
  const float c_size = 4.f, c_bias = 1.7578125f;
  int length = prm ? prm.Length() : 0;
  MeshFilter* meshFilter = gameObject->AddComponent<MeshFilter>();
  Mesh* mesh = new Mesh(); meshFilter->setMesh(mesh); meshFilter->meshIsInstance = true;
  MeshRenderer* meshRenderer = gameObject->AddComponent<MeshRenderer>();
  std::vector<Vector3> array((size_t)length << 2);
  std::vector<int> array2((size_t)length * 6);
  const int array3[6] = { 3, 1, 0, 2, 1, 3 };
  gameObject->layer = 8;
  int num = 0, num2 = 0;
  for (int i = 0; i < length; i++) {
    float num3 = (float)(prm[i] & 0xFF) * c_bias;
    float num4 = (0.f - (float)((prm[i] >> 8) & 0xFF)) * c_bias;
    for (int num5 : array3) array2[num2++] = num + num5;
    array[num++].Set(num3, 0.f, num4 - c_size);
    array[num++].Set(num3 + c_size, 0.f, num4 - c_size);
    array[num++].Set(num3 + c_size, 0.f, num4);
    array[num++].Set(num3, 0.f, num4);
  }
  mesh->vertices = array;
  mesh->SetSubMeshCount(1);
  mesh->SetTriangles(array2, 0);
  m_mat = new Material(Shader::Find("Custom/SmallMapSymbol"));
  meshRenderer->setMaterial(m_mat);
  m_mat->color = Color::red;
  m_uol.Adds(meshFilter, mesh, meshRenderer, m_mat);
}

void BlinkMapSymbol::OnDestroy() { m_uol.Destroy(); m_mat = nullptr; }

void BlinkMapSymbol::Update() {
  if (!m_mat) return;
  Color color = m_mat->color;
  color.g += Time::deltaTime * m_blinkRate;
  if (color.g < 0.f) { m_blinkRate = 0.f - m_blinkRate; color.g = 0.f; }
  else if (color.g > 1.f) { m_blinkRate = 0.f - m_blinkRate; color.g = 1.f; }
  m_mat->color = color;
}

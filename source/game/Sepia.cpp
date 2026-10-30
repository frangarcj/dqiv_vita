// game/Sepia.cpp -- OWNER: WP6. Port of Sepia.cs (MakeNormalizedPlane/DestroyObject are unused helpers in the C# and not ported).
#include "game/Sepia.hpp"

namespace {
Material* m_SepiaMaterial = nullptr;
bool m_IsSepia = false;
Sepia* m_Self = nullptr;
CameraClearFlags m_CFlags = CameraClearFlags::Nothing;
}  // namespace

void Sepia::OnRenderImage(Texture* src, Texture* dst) {
  if (m_IsSepia) {
    m_SepiaMaterial->mainTexture = src;
    Graphics::Blit(src, dst, m_SepiaMaterial);
    m_SepiaMaterial->mainTexture = nullptr;
  }
}

void Sepia::Setup() {
  if (!m_IsSepia) {
    Camera* cam = Camera::main();
    m_Self = cam->gameObject->AddComponent<Sepia>();
    m_SepiaMaterial = new Material(Shader::Find("Custom/Sepia"));
    m_IsSepia = true;
    m_CFlags = cam->clearFlags;
    cam->clearFlags = CameraClearFlags::Color;
  }
}

void Sepia::Release() {
  if (m_IsSepia) {
    Object::DestroyImmediate(m_SepiaMaterial);
    Object::DestroyImmediate(m_Self);
    Camera::main()->clearFlags = m_CFlags;
    m_SepiaMaterial = nullptr;
    m_Self = nullptr;
    m_IsSepia = false;
  }
}

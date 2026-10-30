// game/TapToStart.cpp -- OWNER: WP4a.  C# source: re/cs_out/Assembly-CSharp/TapToStart.cs
#include "game/TapToStart.hpp"
#include "game/UiCameras.hpp"

TapToStart* TapToStart::Create() { return (new GameObject())->AddComponent<TapToStart>()->Setup(); }

TapToStart* TapToStart::Setup() {
  m_material = new Material(MenuParts::l_pictureShader);
  m_material->mainTexture = Resources::Load<Texture2D>("boot/TapToStart");
  m_maker = new MeshMaker(m_material);
  m_maker->SetVertex(Vector3::zero, 223.04001f, 41.48f);
  m_maker->SetUV(0.f, 0.f, 1.f, 1.f);
  m_maker->Apply(true);
  Build(4, 4, 0.f, 0.f, -228.f);
  m_acc = 0.025f;
  m_material->color = Color(1.f, 1.f, 1.f, m_alpha = 0.f);
  m_first = true;
  m_beginFrame = Time::frameCount;
  m_waitTouch = 799;
  m_copyrightAlpha = 0.f;
  m_copyrightMaterial = new Material(MenuParts::l_pictureShader);
  m_copyrightMaterial->mainTexture = Resources::Load<Texture2D>("boot/Retro5_copyright");
  m_copyright = MenuParts::CreateBuildDummy(7, 0.f, 0.f, 0.f);
  m_copyright->m_maker = new MeshMaker(m_copyrightMaterial);
  m_copyright->m_maker->SetVertex(Vector3::zero, 450.f, 40.0005f);
  m_copyright->m_maker->SetUV(0.f, 0.f, 1.f, 1.f);
  m_copyright->m_maker->Apply(true);
  m_copyright->Build(4, 7, 0.f, 0.f, 25.000313f);
  m_copyright->renderer()->enabled = false;
  if (UiCameras::BootPhase()) UiCameras::Retag(m_copyright->transform(), UiCameras::kBootLayer);     // landscape: the copyright line is a free-standing part, centre it with the rest of the start flow
  m_waitCopyright = 767;
  m_copyrightMaterial->color = Color(1.f, 1.f, 1.f, m_copyrightAlpha = 0.f);
  m_uol.Adds(gameObject, m_material, m_copyright, m_copyrightMaterial);
  return this;
}

void TapToStart::OnDestroy() {
  // C#: Resources.UnloadAsset(copyright tex); Resources.UnloadAsset(tap tex).  Not done here: Resources::UnloadAsset deletes the Texture2D at once and the
  // cached textures are shared with other users (DemoOverlay's copyright quad, a later re-open of the title screen): both are ~100 KB.
  m_uol.Destroy();
}

void TapToStart::CountReset() {
  if (m_first) m_waitTouch = 0;
  m_waitCopyright = 999999;
  m_copyrightMaterial->color = Color(1.f, 1.f, 1.f, m_copyrightAlpha = 1.f);
}

void TapToStart::Update() {
  int num = Time::frameCount - m_beginFrame;
  renderer()->enabled = true;
  if (m_waitTouch <= num) {
    m_first = false;
    m_alpha += m_acc;
    if (m_alpha > 1.f || m_alpha < 0.f) {
      m_alpha = !(m_acc > 0.f) ? 0.f : 1.f;
      m_waitTouch = num + 20;
      m_acc = 0.f - m_acc;
    }
    m_material->color = Color(1.f, 1.f, 1.f, m_alpha);
  }
  m_copyright->renderer()->enabled = true;
  if (m_waitCopyright > num) return;
  if (m_waitCopyright == 999999) {
    if ((m_copyrightAlpha -= 0.0625f) < 0.f) {
      m_copyrightAlpha = 0.f;
      m_waitCopyright = 999999;
    }
  } else if ((m_copyrightAlpha += 0.0625f) > 1.f) {
    m_copyrightAlpha = 1.f;
    m_waitCopyright = 999999;
  }
  m_copyrightMaterial->color = Color(1.f, 1.f, 1.f, m_copyrightAlpha);
}

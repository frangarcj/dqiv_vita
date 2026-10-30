// game/Boot.cpp -- OWNER: WP3.  Port of Boot.cs.
// Differences: no OBB-download branch (dataPath always ends with ".obb"), no NoNeon, Escape handling goes through ApplicationQuit stubs,
// logo timing is shortened with DQIV_FAST_BOOT (CMake option, default ON while developing) so test runs reach the native boot quickly.
#include "game/Boot.hpp"
#include "game/GetLangages.hpp"

#include "game/ApplicationQuit.hpp"
#include "native/native_bridge.hpp"
#include "utils/logger.h"

bool Boot::s_useFixedUpdate = false;
std::string Boot::s_deviceModel;
std::string Boot::s_gpuName;

#ifdef DQIV_FAST_BOOT
static const float c_fadeTime = 0.35f, c_waitTime = 0.6f;
#else
static const float c_fadeTime = 1.f, c_waitTime = 3.f;
#endif

bool Boot::Quiting() {
  switch (m_quiting) {
    case 0: if (Input::GetKeyDown(KeyCode::Escape)) m_quiting++; break;
    case 1: ApplicationQuit::OpenQuitDialog(); Time::timeScale = 0.f; m_quiting++; break;
    case 2:
      switch (ApplicationQuit::GetQuitDialogResult()) {
        case 1: m_quiting++; break;
        case 2: Time::timeScale = 1.f; m_quiting = 0; break;
      }
      break;
    case 3: ApplicationQuit::ForceQuit(); break;
  }
  return m_quiting > 0;
}

bool Boot::Touch(bool& prs) {
  bool flag = Input::touchCount > 0;
  bool result = (prs ^ flag) & flag;
  prs = flag;
  return result;
}

Coroutine Boot::Logo() {
  GameObject* obj = GameObject::CreatePrimitive(PrimitiveType::Quad);
  Shader* logoShader = Shader::Find("Custom/Picture");           // Boot.m_logoShader
  Material* mat = new Material(logoShader);
  camera()->orthographicSize = 225.f * (1.f / camera()->aspect());
  obj->renderer()->enabled = false;
  obj->transform->SetParent(transform(), false);
  obj->transform->localPosition = Vector3(0.f, 0.f, 150.f);
  obj->transform->localScale = Vector3(450.f, 150.f, 1.f);
  obj->transform->localRotation = Quaternion::identity;
  obj->renderer()->setMaterial(mat);
  co_yield 0;
  const char* logos[2] = { "title/sqex", "title/arte_logo720_240" };   // Boot.m_logoTexture[2] (DQ4: Resources "Title/", DQ5 had "Boot/")
  for (const char* path : logos) {
    Texture* a = Resources::Load<Texture>(path);
    bool press = false, bSkip = false;
    mat->mainTexture = a;
    mat->color = Color(1.f, 1.f, 1.f, 0.f);
    co_yield 0;
    obj->renderer()->enabled = a != nullptr;
    press = true;
    for (float t = 0.f; t < c_fadeTime; t += Time::deltaTime) {
      if (Touch(press)) bSkip = true;
      mat->color = Color(1.f, 1.f, 1.f, t / c_fadeTime);
      co_yield 0;
    }
    mat->color = Color::white;
    if (!bSkip) {
      for (float t2 = 0.f; t2 < c_waitTime; t2 += Time::deltaTime) {
        if (Touch(press)) break;
        co_yield 0;
      }
    }
    for (float t3 = 0.f; t3 < c_fadeTime; t3 += Time::deltaTime) {
      mat->color = Color(1.f, 1.f, 1.f, 1.f - t3 / c_fadeTime);
      co_yield 0;
    }
    obj->renderer()->enabled = false;
  }
  Object::Destroy(obj);
  delete mat;
}

Coroutine Boot::StartCo() {
  Screen::fullScreen = true;
  GetLangages::Init();                                                // language from the config (README "Language")
  { TextAsset* qm = Resources::Load<TextAsset>("quitmsg"); ApplicationQuit::SystemInit(qm ? qm->text() : std::string()); }   // Boot.m_quitmsg (DQ4: top-level Resources "quitmsg")
  {
    Coroutine i2 = Logo();
    while (i2.MoveNext()) {
      while (Quiting()) co_yield 0;
      co_yield 0;
    }
  }
  s_useFixedUpdate = false;
  while (NativeCall::IsLoadNativeCode() == 0) co_yield 0;
  // DQ4 (Boot.cs): no loadFilelistFromAssets (libdq4core parses the OBB itself) and no deviceInfo; instead android.os.Build.MODEL -> s_deviceModel and the
  // GPU name from /system/lib/egl/egl.cfg -> s_gpuName (used by Field.SystemInit: PowerVR/SGX half-texel UV bias). Vita = SGX543.
  s_deviceModel = "";                                                 // Build.MODEL has no Vita equivalent (Field's FieldAdjustDeviceModel list has no match -> default bias)
  s_gpuName = "POWERVR SGX543MP4+";
  Engine::LoadLevel("dq4sp");
}

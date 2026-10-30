// game/MenuMessage.cpp -- OWNER: WP4a.  C# source: re/cs_out/Assembly-CSharp/MenuMessage.cs
#include "game/Layout.hpp"
#include "game/MenuMessage.hpp"
#include "game/Fade.hpp"
#include "game/Global.hpp"
#include "game/MainLoop.hpp"
#include "utils/logger.h"

MenuMessage::CANVAS_DATA MenuMessage::canvas_data[4] = {
  { 192, 4, Vector2(0.f, -235.f), 1.5f, 1.f, -90.f, false },
  { 193, 0, Vector2(225.f, 15.5f), 0.f, 1.f, 758.f, false },
  { 194, 0, Vector2(225.f, 15.5f), 0.f, 0.9f, 692.f, false },
  { 194, 0, Vector2(225.f, 15.5f), 0.f, 0.9f, 692.f, true },
};
MenuParts* MenuMessage::m_window = nullptr;
int MenuMessage::s_shakeTable[8][2] = { { -6, 6 }, { 6, -6 }, { 6, -6 }, { -6, 6 }, { 6, 6 }, { -6, -6 }, { -6, -6 }, { 6, 6 } };

MenuMessage::MenuMessage(short port) : MenuPort(port) {
  m_window = MenuParts::CreateBuildFrameParts(-1, 4, 0.f, 0.f, 0.f);
  m_window->name = "MessageWindow";
  m_window->gameObject->name = "MessageWindow";
  m_window->DontDestroyOnLoad();
  m_frame = m_window->m_list[0];
  m_canvas = new GameObject();
  m_canvas->transform->SetParent(m_window->transform());
  m_canvas->layer = 8;
  m_canvas->name = "TextCanvas";
  m_canvasMesh = OwnMesh(m_canvas->AddComponent<MeshFilter>());
  m_canvasRenderer = m_canvas->AddComponent<MeshRenderer>();
  m_canvasMaterial = new Material(Shader::Find("Custom/CanvasAlpha8"));
  m_tex = new Texture2D(512, 512, TextureFormat::Alpha8, false);
  m_canvasMaterial->mainTexture = m_tex;
  m_canvasRenderer->setMaterial(m_canvasMaterial);
  m_canvasRenderer->enabled = false;
  Object::DontDestroyOnLoad(m_canvas);
  m_feed = MenuParts::Create(5);
  m_feed->name = "MessageCursor";
  m_feed->gameObject->name = "MessageCursor";
  m_feed->AddIcon(Vector3::zero, 1, true);
  m_feed->Build(4, Vector3::zero, m_window->transform());
  m_feed->DontDestroyOnLoad();
  if (GetLangages::m_langagesNumber == 1) {
    GameObject* who = new GameObject();
    m_who = who->AddComponent<TextMesh>();
    MenuParts::EnsureMeshRenderer(who);
    who->layer = 8;
    who->transform->Rotate(90.f, 0.f, 0.f);
    who->transform->SetParent(m_window->transform());
    who->transform->localPosition = Vector3(-104.5f, 4.f, -104.f);
    m_who->font = MenuParts::l_unityFont;
    m_who->characterSize = 10.f;
    m_who->anchor = (int)TextAnchor::UpperCenter;
    m_who->alignment = (int)TextAlignment::Center;
    MenuParts::SetTextColor(m_who->renderer(), 0);
    m_who->renderer()->enabled = false;
  }
  SetCanvasMesh((short)0);
  // the handshake: native TextureWriter::Create(glTexId, format, 512, 512, flag) (02 §D "Text canvas texture")
  SendInts((int)m_tex->GetNativeTextureID(), (int)m_tex->format, m_tex->width, m_tex->height, (Layout::Aspect(MainLoop::m_camera->aspect()) > 0.74f) ? 1 : 0);
  l_info("MenuMessage: canvas texture gl=%u fmt=%d %dx%d sent to native on port %d", m_tex->GetNativeTextureID(), (int)m_tex->format, m_tex->width, m_tex->height, (int)port);
}

float MenuMessage::s_bottomZ = -300.f;

void MenuMessage::SetCanvasMesh(short no) {
  if (m_nowType != no) {
    float num = canvas_data[no].dify;
    m_nowType = no;
    if (num > eIndent::SCREEN_HEIGHT) num = eIndent::SCREEN_HEIGHT - 1.f;
    if (canvas_data[no].bCheckYN && num + 48.f > eIndent::SCREEN_HEIGHT) num -= num + 48.f - eIndent::SCREEN_HEIGHT;
    m_frame->Change(canvas_data[no].frame);
    SetCanvasMesh(canvas_data[no].indent, canvas_data[no].cursor, canvas_data[no].canvasYDif, 1.f / canvas_data[no].scale);
    m_window->transform()->Translate(0.f, 10.f, 0.f - num);
    {   // landscape dialogue camera (UiCameras): remember the nominal bottom edge of the frame so the window can sit flush with the bottom of the surface;
        // a Yes/No box hangs below the window (type 3 reserves its height), so keep that room
        Bounds fb = OwnMesh(m_window->gameObject->GetComponent<MeshFilter>())->bounds;
        s_bottomZ = m_window->transform()->position().z + fb.min().z - (canvas_data[no].bCheckYN ? CANVAS_YN_HEIGHT : 0.f);
    }
    Fade::SetFadePriority(1, (no == 0) ? 1 : 0);
  }
}

void MenuMessage::SetCanvasMesh(int indent, Vector2 cursor, float canvasYDif, float scale) {
  Vector3 array[4];
  Vector2 array2[4];
  Bounds bounds = OwnMesh(m_window->gameObject->GetComponent<MeshFilter>())->bounds;
  float num = (float)MenuParts::s_info[4].m_dps;
  float num2 = canvasYDif * 450.f / (float)(Layout::Wide() ? Layout::UiW() : Screen::width);
  float num3 = bounds.size().x - num * 1.5f;
  float num4 = bounds.size().z - num * 2.f - num2;
  array[0].Set(bounds.min().x + num * 0.5f, 4.f, bounds.min().z + num + num4 - num2);
  array[1] = array[0] + Vector3(num3, 0.f, 0.f);
  array[2] = array[0] + Vector3(num3, 0.f, 0.f - num4);
  array[3] = array[0] + Vector3(0.f, 0.f, 0.f - num4);
  num3 = num3 / 512.f * scale;
  num4 = num4 / 512.f * scale;
  array2[0].Set(0.f, 0.f);
  array2[1].Set(num3, 0.f);
  array2[2].Set(num3, num4);
  array2[3].Set(0.f, num4);
  m_window->transform()->setPosition(eIndent::CalcScreenPosition(indent, 0.f, 0.f, -4.f));
  m_canvas->transform->localPosition = Vector3::zero;
  m_canvasMesh->Clear();
  m_canvasMesh->vertices.assign(array, array + 4);
  m_canvasMesh->uv.assign(array2, array2 + 4);
  m_canvasMesh->SetTriangles(std::vector<int>{ 2, 3, 1, 1, 3, 0 });
  m_canvasMesh->RecalculateBounds();
  m_feed->transform()->localPosition = Vector3(cursor.x, 8.f, cursor.y);
}

void MenuMessage::Close() { Debug::LogError("Don't Close."); }

void MenuMessage::SetVisible(bool bShow) {
  m_window->renderer()->enabled = bShow;
  m_canvas->renderer()->enabled = bShow;
  if (!bShow) {
    m_feed->renderer()->enabled = false;
    m_count = 17;
  }
  if (m_who != nullptr) {
    m_who->renderer()->enabled = bShow;
    if (!bShow) m_who->text = "";
  }
}

void MenuMessage::NoticeReceive(Param prm) {
  switch (prm[0]) {
    case 0: SetCanvasMesh(prm[1]); break;
    case 1:
      m_bCursor = prm[1] == 1;
      m_count = 0;
      m_feed->renderer()->enabled = m_bCursor;
      break;
    case 2:
      m_shake = 8;
      break;
    case 3: {
      float y = (float)((uint16_t)prm[1] % 512) / 512.f;
      m_canvasMaterial->SetTextureOffset("_MainTex", Vector2(0.f, y));
      break;
    }
    case 4:
      if (!(m_who != nullptr)) break;
      if (m_nowType == 0) {
        if (prm[1] == 0) {
          if (m_bWho) {
            m_frame->Change(192);
            m_who->text = "";
            m_bWho = false;
          }
        } else {
          if (!m_bWho) {
            m_frame->Change(191);
            m_bWho = true;
          }
          m_who->text = Receive().ConvertString();
        }
      } else {
        m_bWho = false;
        m_who->text = "";
      }
      break;
  }
}

void MenuMessage::Update(int pushState, Ray& ray) {
  (void)pushState; (void)ray;
  if (m_bCursor) {
    if (m_count == 0) m_feed->renderer()->enabled = true;
    else if (m_count == 16) m_feed->renderer()->enabled = false;
    else if (m_count == 31) m_count = -1;
    m_count++;
  }
  if (m_shake > 0) {
    m_shake--;
    m_window->transform()->Translate((float)s_shakeTable[m_shake][0], 0.f, (float)s_shakeTable[m_shake][1]);
  }
}

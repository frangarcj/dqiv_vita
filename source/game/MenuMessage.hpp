// game/MenuMessage.hpp -- OWNER: WP4a.  Port of MenuMessage.cs: category-2 port, the message window. The window text itself is rasterised by libdq4core
// (TextCanvas) into the 512x512 GL_ALPHA texture whose GL name we send at creation; every frame MenuPortSystem::LateUpdate -> UnityRenderEvent(1)
// glTexSubImage2D()s the dirty rows. Here: window frame (FRAME_ENUM 191..194), canvas quad + V-scroll, page-feed cursor, shake, speaker name (EN).
#pragma once
#include "game/MenuParts.hpp"
#include "game/MenuPort.hpp"
#include "miniunity.hpp"
using namespace miniunity;

class MenuMessage : public MenuPort {
public:
  struct CANVAS_DATA { int frame; int indent; Vector2 cursor; float canvasYDif; float scale; float dify; bool bCheckYN; };
  static constexpr float DEFAULT_BTL_DIF = 105.f, CANVAS_Z = 4.f, CANVAS_YN_HEIGHT = 48.f;
  static constexpr int CANVAS_PIXEL_SIZE = 512, CANVAS_SIZE = 262144, SHAKE_DIST = 6;
  static CANVAS_DATA canvas_data[4];
  static MenuParts* m_window;
  static float s_bottomZ;                 // world z of the lowest edge of the window incl. the Yes/No reserve (UiCameras bottom alignment)
  static int s_shakeTable[8][2];

  explicit MenuMessage(short port);
  void SetCanvasMesh(short no);
  void SetCanvasMesh(int indent, Vector2 cursor, float canvasYDif, float scale);
  void Close() override;
  void SetVisible(bool bShow) override;
  void NoticeReceive(Param prm) override;
  void Update(int pushState, Ray& ray) override;

private:
  GameObject* m_canvas = nullptr;
  MenuParts* m_feed = nullptr;
  Mesh* m_canvasMesh = nullptr;
  Renderer* m_canvasRenderer = nullptr;
  Material* m_canvasMaterial = nullptr;
  Texture2D* m_tex = nullptr;
  MenuParts::IBasePtr m_frame;
  bool m_bCursor = false;
  int m_count = 0, m_shake = 0;
  short m_nowType = -1;
  TextMesh* m_who = nullptr;
  bool m_bWho = false;
};

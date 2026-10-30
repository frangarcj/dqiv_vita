// game/MenuPort.hpp -- OWNER: WP4a.  Port of MenuPort.cs: base class of every port (a UI "screen" driven through the short[] mailbox, 02 §D)
// and MenuPort::Param, the view of one native->host message inside MenuPortSystem::s_nativeBuffer.
//
// C# -> C++: `Param` (a class, null when the port has no message) is a small value type: `if (Param p = Receive()) ...` / `p.valid()`.
//            `params short[]/int[]/byte[]` Send/SendInts/SendBytes: variadic templates, same call shape as the C#.
//            `ref Ray` -> `Ray&`.
#pragma once
#include <initializer_list>
#include <string>
#include <vector>
#include "miniunity.hpp"
using namespace miniunity;

class MenuPort {
public:
  class Param {
  public:
    explicit Param(short offset = 0) : m_offset(offset) {}
    explicit operator bool() const { return m_offset != 0; }          // C#: `param != null`
    short operator[](int index) const;
    short Length() const;
    short Next() const;
    int GetInt(int index) const;
    float GetFloat(int index) const;
    std::string ConvertString() const;                                // UTF-16 units p[0..Length) -> UTF-8
    std::string Dump() const;
  private:
    short m_offset;
  };

  static constexpr int PUSH_STATE_NONE = 0, PUSH_STATE_BEGIN = 1, PUSH_STATE_PRESS = 2, PUSH_STATE_FINISH = 3, PUSH_STATE_UN_PRESS_WAIT = 4;

  static int s_frameCounter;
  short m_port;
  int m_uiLayer = 8;                          // 8 = MenuCamera (panel); landscape: UiCameras::kDialogueLayer (message, Yes/No) / kBootLayer (start flow, centred panel)
  bool m_bootCounted = false;                 // counted in UiCameras::s_bootPorts

  explicit MenuPort(short port) : m_port(port) {}
  virtual ~MenuPort() {}

  static MenuPort* CreateMenuPort(short port, short catego, short type);
  virtual void NoticeReceive(Param prm) { (void)prm; }
  virtual void Close() {}
  virtual void SetVisible(bool bShow) { (void)bShow; }
  virtual void Update(int pushState, Ray& ray) { (void)pushState; (void)ray; }
  virtual void OnGUI() {}

  Param Receive();
  // C# `params short[]` / `params int[]` / `params byte[]`: same call shapes, e.g. Send(1, m_buttonId, m_gamepad); SendInts(texId, fmt, w, h, flag);
  template <class... T> void Send(T... v) { SendShorts(std::vector<short>{ static_cast<short>(v)... }); }
  template <class... T> void SendInts(T... v) { SendIntVec(std::vector<int>{ static_cast<int>(v)... }); }
  template <class... T> void SendBytes(T... v) { SendByteVec(std::vector<unsigned char>{ static_cast<unsigned char>(v)... }); }
  void SendShorts(const std::vector<short>& prms);
  void SendIntVec(const std::vector<int>& prms);
  void SendByteVec(const std::vector<unsigned char>& prms);
};

// Ports implemented by other work packages (category 3 = EndrollPort, 5 = SlimeTouchPort (WP6); category 4 = NetConnection stub). MenuPort::CreateMenuPort
// calls these factories; the defaults (menu_port_factories.cpp, weak) log and return nullptr. A WP6 port overrides with a strong definition:
//   MenuPort* dqiv_CreateEndrollPort(short port, short type) { return new EndrollPort(port, type); }
MenuPort* dqiv_CreateEndrollPort(short port, short type);
MenuPort* dqiv_CreateNetConnection(short port);
MenuPort* dqiv_CreateSlimeTouchPort(short port, short type);

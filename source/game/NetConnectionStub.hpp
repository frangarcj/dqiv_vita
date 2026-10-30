// game/NetConnectionStub.hpp -- OWNER: WP6 (first version by WP4: the Vita has no network services).  Port of NetConnection.cs (category-4 MenuPort: cloud save, SQMK campaign
// codes) in its "Application.internetReachability == NotReachable" behaviour: SQMK auth ends silently, every cloud operation replies error 408, present codes reply 801.
// The status bitmask protocol (Send(0, status): bit0 busy, bit1 wired, bit2 cloud authed, bit3 SQMK authed) and the 15-frame task pacing are kept so the native UI never hangs.
#pragma once
#include "game/MenuPort.hpp"
#include "miniunity.hpp"
using namespace miniunity;

class NetConnection : public MenuPort {
public:
  explicit NetConnection(short port) : MenuPort(port) {}
  void NoticeReceive(Param prm) override;
  void Update(int pushState, Ray& ray) override;
private:
  enum Task { T_NONE, T_SQMK_AUTH, T_OFFLINE_ERROR };
  short m_status = 0, m_statusOld = 0;
  int m_task = T_NONE, m_taskWait = 0;
  void SetStatus(int no, bool on) { if (on) m_status |= (short)(1 << no); else m_status &= (short)~(1 << no); }
  bool GetStatus(int no) const { return (m_status & (1 << no)) != 0; }
  void SetTask(int task);
};

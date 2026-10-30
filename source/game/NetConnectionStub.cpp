// game/NetConnectionStub.cpp -- C# source: re/cs_out/Assembly-CSharp/NetConnection.cs (offline behaviour only, see the header)
#include "game/NetConnectionStub.hpp"
#include "utils/logger.h"

void NetConnection::SetTask(int task) {
  if (m_task != T_NONE) { Debug::LogError("override task"); return; }
  m_task = task;
  m_taskWait = 15;
  SetStatus(0, true);
}

void NetConnection::NoticeReceive(Param prm) {
  switch (prm[0]) {
    case 0: if (!GetStatus(3)) SetTask(T_SQMK_AUTH); break;
    case 1: Send(1, 801); break;                                   // s_presentcodeList == null
    case 2: break;                                                 // campaign code auth needs SQMK auth (never offline)
    case 10: if (!GetStatus(2)) SetTask(T_OFFLINE_ERROR); break;   // CloudAuth
    case 11: SetTask(T_OFFLINE_ERROR); break;                      // CloudSyncFileCheck
    case 13: SetTask(T_OFFLINE_ERROR); break;                      // CloudSyncFile
    case 12: case 80: break;                                       // Application.OpenURL: nothing to open on the Vita
  }
}

void NetConnection::Update(int, Ray&) {
  if (m_task == T_NONE) {
    if (m_taskWait > 0) m_taskWait--;
    else SetStatus(0, false);
  } else if (m_taskWait > 0) {
    m_taskWait--;
  } else {
    if (m_task == T_SQMK_AUTH) SetStatus(3, false);                // NotReachable: SQMK_Auth just returns
    else { Send(1, 408); SetStatus(2, false); }                    // CloudErrorCheck: NotReachable -> error 408
    l_info("NetConnection (offline): task %d -> %s", m_task, m_task == T_SQMK_AUTH ? "no SQMK" : "error 408");
    m_task = T_NONE;
    m_taskWait = 15;
    SetStatus(0, true);
  }
  SetStatus(1, false);                                             // internetReachability == NotReachable
  if (m_status != m_statusOld) Send(0, m_statusOld = m_status);
}

MenuPort* dqiv_CreateNetConnection(short port) { return new NetConnection(port); }

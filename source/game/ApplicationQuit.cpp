// game/ApplicationQuit.cpp -- OWNER: WP3. See ApplicationQuit.hpp. C# source: ApplicationQuit.cs
#include "game/ApplicationQuit.hpp"
#include "game/GetLangages.hpp"

#include <cstring>
#include <vector>

#include <psp2/common_dialog.h>
#include <psp2/message_dialog.h>

#include "game/Fade.hpp"
#include "game/Global.hpp"
#include "game/Sound.hpp"
#include "platform/cfg.hpp"
#include "config.h"
#include "utils/logger.h"

extern "C" volatile int dqiv_common_dialog_active = 0;

static std::string s_title = "Dragon Quest IV", s_message = "Quit the game?", s_yes = "Yes", s_no = "No";
int ApplicationQuit::s_quitStatus = -1;
bool ApplicationQuit::s_disable = false;
static bool s_dialogOpen = false;
static char s_dialogText[512];
static int s_fadeWait = 0;

void ApplicationQuit::SystemInit(const std::string& quitmsg) {
  if (!quitmsg.empty()) {
    std::vector<std::string> array;
    size_t p = 0;
    while (true) { size_t q = quitmsg.find('\t', p); array.push_back(quitmsg.substr(p, q == std::string::npos ? std::string::npos : q - p)); if (q == std::string::npos) break; p = q + 1; }
    size_t num = (size_t)GetLangages::m_langagesNumber * 4;
    if (array.size() >= num + 4) { s_title = array[num]; s_message = array[num + 1]; s_yes = array[num + 2]; s_no = array[num + 3]; }
  }
  s_quitStatus = 0;
}

void ApplicationQuit::OpenQuitDialog() {
  if (s_dialogOpen) return;
  snprintf(s_dialogText, sizeof(s_dialogText), "%s\n\n%s", s_title.c_str(), s_message.c_str());
  SceMsgDialogUserMessageParam up;
  memset(&up, 0, sizeof(up));
  up.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_YESNO;
  up.msg = (const SceChar8*)s_dialogText;
  SceMsgDialogParam param;
  sceMsgDialogParamInit(&param);
  _sceCommonDialogSetMagicNumber(&param.commonParam);
  param.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
  param.userMsgParam = &up;
  int r = sceMsgDialogInit(&param);
  l_info("ApplicationQuit: sceMsgDialogInit -> 0x%08x", r);
  if (r >= 0) { s_dialogOpen = true; dqiv_common_dialog_active = 1; }
}

int ApplicationQuit::GetQuitDialogResult() {
  if (!s_dialogOpen) return 2;           // dialog could not be shown: behave as "No" (never quit by accident)
  if (sceMsgDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED) return 0;
  SceMsgDialogResult res;
  memset(&res, 0, sizeof(res));
  sceMsgDialogGetResult(&res);
  sceMsgDialogTerm();
  s_dialogOpen = false; dqiv_common_dialog_active = 0;
  l_info("ApplicationQuit: dialog result button=%d", res.buttonId);
  return res.buttonId == SCE_MSG_DIALOG_BUTTON_ID_YES ? 1 : 2;
}

void ApplicationQuit::ForceQuit() {
  s_quitStatus = 5;
  Application::Quit();
}

void ApplicationQuit::Create() {
  if (s_quitStatus == 0 && !s_disable) {
    (new GameObject("ApplicationQuit"))->AddComponent<ApplicationQuit>();
    s_quitStatus = 1;
  }
}

void ApplicationQuit::Update() {
  switch (s_quitStatus) {
    case 1:
      if ((Global::Flag & 0x20000) == 0) {
        Sound::SystemPause(true);
        OpenQuitDialog();
        Global::Flag |= 65536;
        Time::timeScale = 0.f;
        s_quitStatus++;
      }
      break;
    case 2:
      switch (GetQuitDialogResult()) {
        case 1: s_quitStatus++; break;
        case 2:
          Global::Flag &= -65537;
          Time::timeScale = 1.f;
          s_quitStatus = 0;
          Object::Destroy(gameObject);
          Sound::SystemPause(false);
          break;
      }
      break;
    case 3:
      Time::timeScale = 1.f;
      Fade::SetLayer(0);
      Fade::SetFadeOutBlack(30);
      s_quitStatus++;
      break;
    case 4:
      if (Fade::IsEnd() || ++s_fadeWait > 60) s_quitStatus++;      // (deviation: the 60-frame cap keeps Quit working while Fade is still a stub)
      break;
    case 5:
      Application::Quit();
      break;
  }
}

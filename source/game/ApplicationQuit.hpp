// game/ApplicationQuit.hpp -- OWNER: WP3.  Port of ApplicationQuit.cs: the "quit the game?" flow (state machine 0..5) + the Boot-time dialog helpers.
// The Android dialog (UnityAndroidDialog.ShowDialog / m_result) becomes sceMsgDialog YES/NO; the text comes from Resources "boot/quitmsg"
// (tab separated, 4 strings per language: title, message, yes, no). GetQuitDialogResult: 0 pending, 1 yes, 2 no.
// Create() is called by MenuPortSystem on port-0 cmd 999 and when TouchPad.m_bToggleESC (Start) is set.
#pragma once
#include <string>
#include "miniunity.hpp"
using namespace miniunity;

class ApplicationQuit : public Component {
public:
  static bool s_disable;
  static void SystemInit(const std::string& quitmsg);
  static void OpenQuitDialog();
  static int GetQuitDialogResult();
  static void ForceQuit();
  static void Create();
  void Update() override;
private:
  static int s_quitStatus;
};

extern "C" volatile int dqiv_common_dialog_active;     // 1 while a sceCommonDialog is up: the renderer must present with vglSwapBuffers(GL_TRUE)

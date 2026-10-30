// game/GetLangages.hpp -- OWNER: WP4a (shared by WP4b screens).  Port of the one field of GetLangages.cs (Assembly-CSharp-firstpass) the UI uses:
// `GetLangages.m_langagesNumber` = DevInit's language argument (0 JA, 1 EN, 2 KO, 3 TC, 4 SC).  Default DQIV_LANG (English); GetLangages::Init() overrides it from the `lang=` config key.
#pragma once
#include "config.h"

struct GetLangages {
  static inline int m_langagesNumber = DQIV_LANG;
  static void Init();            // reads `lang=` from the config (see README "Language"): en (default) | ja | ko | zh-tw | zh-cn, or 0..4; call before DevInit / ApplicationQuit::SystemInit / MenuParts::SystemInit
};

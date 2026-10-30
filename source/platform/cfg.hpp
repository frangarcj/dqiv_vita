// platform/cfg.hpp -- OWNER: WP3. ux0:data/dqiv/dqivn<slot>.cfg: `key=value` lines (written by tools/run_vita3k.sh and by hand on the Vita).
//   shot_frames=300,600   screenshot request frames (GameLoop)         audiotest=1     play BGM/SE at boot and log levels (Sound)
//   input_script=1        enable ux0:data/dqiv/input_script<slot>.txt    confirm=circle  swap Cross/Circle (JP layout)
//   lang=en|ja|ko|zh-tw|zh-cn   game language (GetLangages::Init)
//   ux0:data/dqiv/config.txt is read first (user config, all slots), then dqivn<slot>.cfg.
//   autostart=1           shorthand: any button/idle = tap on the logos (debug)
#pragma once
#include <string>
// layout=landscape|split|portrait (default landscape: 960x544 world + 408x544 UI panel, see game/Layout.hpp);  ui_side=left|right;  land_naive=1 (debug)
namespace Cfg {
void Load();                                          // idempotent
std::string Get(const char* key, const char* def = "");
int GetInt(const char* key, int def = 0);
bool Has(const char* key);
}

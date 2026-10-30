// engine/statics.cpp -- Time / Screen / Application / Input / Debug (WP1).
#include "miniunity.hpp"
#include "config.h"
#include "utils/logger.h"

#include <cstdarg>
#include <cstdio>

namespace miniunity {

float Time::deltaTime = 1.f / 30.f, Time::timeScale = 1.f, Time::time = 0.f;
int Time::frameCount = 0;

int Screen::width = DQIV_LOGICAL_W, Screen::height = DQIV_LOGICAL_H;
bool Screen::fullScreen = true;

bool Application::quitRequested = false;
// Application.dataPath-derived strings. streamingAssetsPath is "jar:file://<obb>!/assets" in the C#; unused by the port.
std::string Application::dataPath = OBB_PATH, Application::persistentDataPath = SAVE_PATH, Application::streamingAssetsPath = std::string("jar:file://") + OBB_PATH + "!/assets";
void Application::Quit() { quitRequested = true; }

int Input::touchCount = 0;
Touch Input::touches[5];
bool Input::escapeDown = false;
bool Input::GetKeyDown(KeyCode k) { return k == KeyCode::Escape && escapeDown; }

void Debug::Log(const char* fmt, ...) {
  char buf[1024]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
  l_info("[cs] %s", buf);
}
void Debug::LogError(const char* fmt, ...) {
  char buf[1024]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
  l_error("[cs] %s", buf);
}

}  // namespace miniunity

// miniunity/statics.hpp -- Time / Screen / Application / Input / Debug.        OWNER: WP1 (Input fill: WP3 platform/vita_input.cpp)
#pragma once
#include <string>
#include "miniunity/math.hpp"

namespace miniunity {

struct Time { static float deltaTime, timeScale; static int frameCount; static float time; };
struct Screen { static int width, height; static bool fullScreen; };     // width/height of the logical (viewport) surface, e.g. 408x544
struct Application { static void Quit(); static bool quitRequested; static void LoadLevel(const char* name); static std::string dataPath, persistentDataPath, streamingAssetsPath; static void OpenURL(const char*){}
  static bool internetReachable() { return false; } /* NotReachable */ };
enum class KeyCode { Escape = 27 }; enum class TouchPhase { Began, Moved, Stationary, Ended, Canceled };
struct Touch { int fingerId = 0; Vector2 position, deltaPosition; float deltaTime = 0; int tapCount = 0; TouchPhase phase = TouchPhase::Canceled; };
struct Input { static int touchCount; static Touch touches[5]; static bool GetKeyDown(KeyCode k); static bool escapeDown; };   // filled once per frame by platform/vita_input.cpp (06_INPUT.md)
struct Debug { static void Log(const char* fmt,...) __attribute__((format(printf,1,2))); static void LogError(const char* fmt,...) __attribute__((format(printf,1,2))); };

}  // namespace miniunity

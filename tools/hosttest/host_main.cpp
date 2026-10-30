// Host harness: loads the pack from argv[1] (default ../extract/pack) and runs the engine self tests with ASan/UBSan.
#include "miniunity.hpp"
#include "engine/engine_internal.hpp"
#include "utils/logger.h"
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include "game/FldObject.hpp"
#include "game/FbxObject.hpp"
extern "C" void _log_print(int t, const char* fmt, ...) {
  static const char* nm[] = { "DBG", "INF", "WRN", "ERR", "FTL", "OK ", "..." };
  va_list ap; va_start(ap, fmt); fprintf(stderr, "[%s] ", nm[t]); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap);
}
extern "C" void log_file_reset(void) {}
namespace miniunity {      // camera.cpp is not part of the host build: the two members FldObject needs
Camera* Camera::main() { for (GameObject* g : engine::g_gameObjects) if (g->tag == "MainCamera") return g->GetComponent<Camera>(); return nullptr; }
float Camera::aspect() const { return 0.75f; }
}
int main(int argc, char** argv) {
  const char* pack = argc > 1 ? argv[1] : "/Users/frangar/Fun/dqiv/work/extract/pack";
  if (!miniunity::Resources::Init(pack)) return 2;
  if (argc > 2 && !strcmp(argv[2], "--fld")) {            // WP5b: FldObject load / animate / switch / term over the given maps (ASan): host_main <pack> --fld <map> [<map>...]
    using namespace miniunity;
    GameObject* camGo = new GameObject("cam"); camGo->tag = "MainCamera"; camGo->AddComponent<Camera>();
    FldObject* fld = new FldObject();
    for (int a = 3; a < argc; a++) {
      if (!strncmp(argv[a], "fbx/", 4)) {
        int no = atoi(argv[a] + 5);
        FbxObject* f = new FbxObject(no); f->SetVisible(true); f->PlayAnimation(0);
        for (int fr = 0; fr < 30; fr++) { Time::deltaTime = 1.f / 30; engine::TickAnimations(Time::deltaTime); engine::SkinAll(); engine::FlushDestroy(); }
        fprintf(stderr, "FBX %s: obj %p mats %zu end=%d\n", argv[a], (void*)f->m_obj, f->m_materials.size(), (int)f->IsEndAnimation());
        f->Release(); delete f; engine::FlushDestroy(); continue;
      }
      bool ok = fld->Load(argv[a]);
      fprintf(stderr, "FLD %s: ok=%d objs=%d cams=%d events=%d bg=(%.2f %.2f %.2f)\n", argv[a], ok, fld->GetMapObjNum(), fld->GetCameraNoLength(), fld->GetEventAnimationNum(),
              fld->GetMapEtc().bg_color.r, fld->GetMapEtc().bg_color.g, fld->GetMapEtc().bg_color.b);
      Resources::UnloadUnusedAssets();                    // NativeManager op 0 ends with Cleaner.Cleaning()
      if (fld->GetCameraNoLength() > 0) fld->SetCameraNo(1, 0);
      for (int fr = 0; fr < 90; fr++) { Time::deltaTime = 1.f / 30; fld->Update(); engine::FlushDestroy(); if (fr == 3 || fr == 60) Resources::UnloadUnusedAssets(); }
      for (int e = 1; e <= fld->GetEventAnimationNum() && e <= 3; e++) { fld->SetEventAnimation(e); fld->Update(); fld->SetEventAnimation(0); }
      for (int i = 0; i < fld->GetMapObjNum() && i < 40; i++) { fld->SetCommonAnimation(i, 1); fld->Update(); fld->SetCommonAnimation(i, 0); fld->SetMapObjAlpha(i, 15, 0); fld->SetMapObjOnOff(i, 0); fld->Update(); fld->SetMapObjAlpha(i, 31, 0); }
      fld->SetMaterialColor(Color(0.5f, 0.5f, 0.8f, 1.f)); fld->calculateExecSepia(); fld->Update(); engine::FlushDestroy();
      Resources::LogStats(argv[a]);
    }
    fld->Term(); engine::FlushDestroy(); Resources::UnloadUnusedAssets(); Resources::LogStats("after term");
    return 0;
  }
  int fails = miniunity::engine::RunSelfTests(argc > 2 ? argv[2] : "fbx/c0010", argc > 3 ? argv[3] : "map/btl/btl_dk/stage/btl_dk2.fld", argc > 4 ? argv[4] : "map/d/dn/out/dnout2.fld");
  return fails ? 1 : 0;
}

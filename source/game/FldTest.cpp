#ifdef DQIV_DEV_TOOLS
#include "game/FldTest.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "config.h"
#include "game/FbxObject.hpp"
#include "game/FldObject.hpp"
#include "miniunity.hpp"
#include "utils/logger.h"

using namespace miniunity;

namespace {

struct Entry { std::string path; float scale = 1.f; int frames = 120; int cam = 0; float elev = 0.55f; };
std::vector<Entry> g_entries;

bool ReadList() {
  FILE* f = fopen(DATA_PATH "fldtest" DQIVN_SLOT_STR ".on", "r");
  if (!f) return false;
  char line[512];
  while (fgets(line, sizeof(line), f)) {
    Entry e; char path[400]; float sc = 1.f, el = 0.55f; int fr = 120, cm = 0;
    int n = sscanf(line, "%399s %f %d %d %f", path, &sc, &fr, &cm, &el);
    if (n < 1 || path[0] == '#') continue;
    e.path = path; if (n >= 2) e.scale = sc; if (n >= 3) e.frames = fr; if (n >= 4) e.cam = cm; if (n >= 5) e.elev = el;
    g_entries.push_back(e);
  }
  fclose(f);
  return !g_entries.empty();
}

class FldTestDriver : public Component {
public:
  FldObject fld; int index = -1; int frameInEntry = 0; Camera* cam = nullptr; Vector3 center; float radius = 10.f; bool useMapCam = false;
  FbxObject* fbx = nullptr;

  void Frame() {
    // bounds of every visible model
    bool any = false; Vector3 mn, mx;
    for (auto& op : fld.m_map_objs) {
      Transform* t = op->model; if (!t) continue;
      MeshFilter* mf = t->GetComponent<MeshFilter>(); if (!mf || !mf->sharedMesh) continue;
      Bounds b = mf->sharedMesh->bounds; Matrix4x4 m = t->localToWorldMatrix();
      for (int i = 0; i < 8; i++) {
        Vector3 p = m.MultiplyPoint3x4(Vector3(b.center.x + ((i & 1) ? b.extents.x : -b.extents.x), b.center.y + ((i & 2) ? b.extents.y : -b.extents.y), b.center.z + ((i & 4) ? b.extents.z : -b.extents.z)));
        if (!any) { mn = mx = p; any = true; }
        else { mn = Vector3(std::fmin(mn.x, p.x), std::fmin(mn.y, p.y), std::fmin(mn.z, p.z)); mx = Vector3(std::fmax(mx.x, p.x), std::fmax(mx.y, p.y), std::fmax(mx.z, p.z)); }
      }
    }
    if (any) { center = (mn + mx) * 0.5f; radius = (mx - mn).magnitude() * 0.5f; }
    l_info("FldTest: bounds center (%.1f,%.1f,%.1f) radius %.1f", center.x, center.y, center.z, radius);
  }

  void FbxBounds() {
    bool any = false; Vector3 mn, mx;
    std::vector<Renderer*> rs; fbx->m_obj->GetComponentsInChildren<Renderer>(rs);
    for (Renderer* r : rs) {
      Mesh* mesh = nullptr;
      if (auto* sr = dynamic_cast<SkinnedMeshRenderer*>(r)) mesh = sr->skinned ? sr->skinned : sr->sharedMesh;
      else if (MeshFilter* mf = r->GetComponent<MeshFilter>()) mesh = mf->sharedMesh;
      if (!mesh) continue;
      mesh->RecalculateBounds();
      Bounds b = mesh->bounds; Matrix4x4 m = r->transform()->localToWorldMatrix();
      for (int i = 0; i < 8; i++) {
        Vector3 p = m.MultiplyPoint3x4(Vector3(b.center.x + ((i & 1) ? b.extents.x : -b.extents.x), b.center.y + ((i & 2) ? b.extents.y : -b.extents.y), b.center.z + ((i & 4) ? b.extents.z : -b.extents.z)));
        if (!any) { mn = mx = p; any = true; }
        else { mn = Vector3(std::fmin(mn.x, p.x), std::fmin(mn.y, p.y), std::fmin(mn.z, p.z)); mx = Vector3(std::fmax(mx.x, p.x), std::fmax(mx.y, p.y), std::fmax(mx.z, p.z)); }
      }
    }
    if (any) { center = (mn + mx) * 0.5f; radius = (mx - mn).magnitude() * 0.5f; }
    l_info("FldTest: fbx bounds center (%.2f,%.2f,%.2f) radius %.2f (%d renderers)", center.x, center.y, center.z, radius, (int)rs.size());
  }

  void Next() {
    index = (index + 1) % (int)g_entries.size();
    const Entry& e = g_entries[index];
    frameInEntry = 0;
    if (fbx) { fbx->Release(); delete fbx; fbx = nullptr; }
    if (e.path.compare(0, 4, "fbx/") == 0) {              // "fbx/c0010": a skinned model with its first animation (FbxObject)
      fld.Term();
      fbx = new FbxObject(atoi(e.path.c_str() + 5));
      if (!fbx->m_obj) return;
      fbx->SetVisible(true); fbx->SetPosition(Vector3::zero); fbx->PlayAnimation(e.cam);
      cam->backgroundColor = Color(0.25f, 0.25f, 0.3f, 1.f); useMapCam = false;
      FbxBounds();
      l_info("FldTest: fbx %s: %d materials, playing=%d", e.path.c_str(), (int)fbx->m_materials.size(), (int)!fbx->IsEndAnimation());
      return;
    }
    l_info("FldTest: loading %s (scale %.1f)", e.path.c_str(), e.scale);
    bool ok = fld.Load(e.path);
    fld.SetDispScale(e.scale, e.scale, e.scale);
    l_info("FldTest: Load -> %d, %d map objects, cameras %d, bg (%.2f,%.2f,%.2f) fov %.1f", ok, fld.GetMapObjNum(), fld.GetCameraNoLength(),
           fld.GetMapEtc().bg_color.r, fld.GetMapEtc().bg_color.g, fld.GetMapEtc().bg_color.b, fld.GetMapEtc().fov);
    cam->backgroundColor = fld.GetMapEtc().bg_color;
    useMapCam = e.cam > 0 && fld.GetCameraNoLength() >= e.cam;
    if (useMapCam) { fld.SetCameraNo(e.cam, 0); fld.SetCameraNo(e.cam, 1); }
    fld.Update();       // positions models / applies the first camera
    Frame();
    Resources::LogStats("fldtest");
  }

  void Update() override {
    if (!cam) return;
    if (fbx && fbx->m_obj && !fbx->IsEndAnimation() == false && frameInEntry % 120 == 119) fbx->PlayAnimation(g_entries[index].cam);
    if (index < 0 || frameInEntry >= g_entries[index].frames) Next();
    frameInEntry++;
    if (!fbx) fld.Update();
    const Entry& e = g_entries[index];
    if (!useMapCam) {
      float a = frameInEntry * 0.01f, dist = radius * e.scale * 1.6f;
      Vector3 c = center * e.scale;
      cam->transform()->setPosition(c + Vector3(std::sin(a) * dist * 0.6f, dist * e.elev, -std::cos(a) * dist * 0.6f));
      cam->transform()->LookAt(c);
    }
  }
};

}  // namespace

namespace FldTest {

bool Requested() { return ReadList(); }

void Create() {
  GameObject* camGo = new GameObject("FldTestCamera");
  camGo->tag = "MainCamera";
  Camera* cam = camGo->AddComponent<Camera>();
  cam->orthographic = false; cam->fieldOfView = 40.f; cam->nearClipPlane = 0.3f; cam->farClipPlane = 2000.f;
  cam->clearFlags = CameraClearFlags::Color; cam->backgroundColor = Color(0.1f, 0.1f, 0.3f, 1.f); cam->depth = -1.f; cam->cullingMask = -1;
  Object::DontDestroyOnLoad(camGo);
  GameObject* drv = new GameObject("FldTestDriver");
  FldTestDriver* d = drv->AddComponent<FldTestDriver>(); d->cam = cam;
  Object::DontDestroyOnLoad(drv);
  l_info("FldTest: %d map(s) listed", (int)g_entries.size());
}

}  // namespace FldTest

#endif  // DQIV_DEV_TOOLS

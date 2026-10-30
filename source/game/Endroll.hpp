// game/Endroll.hpp -- OWNER: WP6.  Ports of Endroll.cs / EndrollEx.cs / EndrollPort.cs: the ending credits (a long textured-quad mesh built from endroll/Retro5_endroll_en_NN(.bin),
// scrolled by 1.82 units per Update; sends petal animation cues and the end-of-scroll message to native through the MenuPort).
// C# source of truth: re/cs_out/Assembly-CSharp/{Endroll,EndrollEx,EndrollPort}.cs
#pragma once
#include <map>
#include <vector>
#include "game/MenuPort.hpp"
#include "game/MeshMaker.hpp"
#include "miniunity.hpp"
using namespace miniunity;

class Endroll : public Component {
public:
  static constexpr float POSITIVE_H = 227.555f;
  Material* m_material = nullptr; Texture2D* m_texture = nullptr;
  Material* m_material_2 = nullptr; Texture2D* m_texture_2 = nullptr;
  unsigned short m_width = 0, m_height = 0;
  float m_scale = 1.f; int m_index = 0; float m_counter = 0.f;
  int m_nPetal = 0; std::vector<float> m_petal; int m_exclude = 0;
  MenuPort* m_port = nullptr;
  void Setup(MenuPort* port, int index);
  void SetExclude(bool exclude);
  void OnDestroy() override;
  void Update() override { internalUpdate(); }
protected:
  void internalUpdate();
private:
  UnityOBJList m_uol;
};

// Boot.s_useFixedUpdate is always false in this port (NEON path): EndrollEx (internalUpdate from FixedUpdate) is not created, kept for completeness.
class EndrollEx : public Endroll { public: void Update() override {} };

class EndrollPort : public MenuPort {
public:
  Endroll* m_drawInstance = nullptr;
  EndrollPort(short port, int type);
  void Close() override;
  void NoticeReceive(Param prm) override;
private:
  struct CameraConf { float depth; bool enabled; int cullingMask; Color backgroundColor; CameraClearFlags clearFlags; };
  static std::map<Camera*, CameraConf> s_cameraConf;
  void SetupCamera(); void RestoreCamera(); void PushCamera(Camera* camera); void PopCamera(Camera* camera);
};

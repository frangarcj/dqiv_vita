// game/SlimeTouch.hpp -- OWNER: WP6.  Ports of CgController.cs / SlimeTouchBehaviour.cs / SlimeTouchPort.cs: the "Slime Touch" mini game's 3D CG overlays (start/gameover/sweat/board
// prefabs animated by legacy Animation) and its 8 touch boxes on the DSSA ortho camera.
// C# source of truth: re/cs_out/Assembly-CSharp/{CgController,SlimeTouchBehaviour,SlimeTouchPort}.cs
#pragma once
#include <vector>
#include "game/MenuPort.hpp"
#include "miniunity.hpp"
using namespace miniunity;

class CgController {
public:
  void Setup();
  void Clean();
  void Update();
  void Draw(int i_cg);
  void NormaSet(int id, int i_cg, int track);
private:
  GameObject* cg[8] = {}; Animation* anim[8] = {}; AnimationState* stat[8] = {}; std::vector<Renderer*> render[8]; Material* mat[8] = {}; Texture2D* tex[3] = {};
  int drawBit = 0, playBit = 0, _playBit = 0;
};

class SlimeTouchBehaviour : public Component {
public:
  BoxCollider* box[8] = {}; int updateFlag = 0; CgController* controller = nullptr;
  void Awake() override;
  void OnDestroy() override;
  void Update() override;
  void SetTouchState(bool state);
};

class SlimeTouchPort : public MenuPort {
public:
  static MenuPort* s_port;
  SlimeTouchPort(short port, int type);
  void Close() override;
  void NoticeReceive(Param prm) override;
private:
  SlimeTouchBehaviour* behaviour = nullptr;
  Vector3 camera_pos;
};

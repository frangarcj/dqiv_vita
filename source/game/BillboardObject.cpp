// game/BillboardObject.cpp -- OWNER: WP5a. Port of BillboardObject.cs.
#include "game/BillboardObject.hpp"
#include "game/MainLoop.hpp"

Quaternion BillboardObject::s_shadowRot = Quaternion::identity;
Transform* BillboardObject::s_billboardTarget = nullptr;

void BillboardObject::SystemInit() { s_billboardTarget = MainLoop::m_camera->transform(); }

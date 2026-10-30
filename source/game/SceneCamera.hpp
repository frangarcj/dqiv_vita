// game/SceneCamera.hpp -- OWNER: WP5a  [HEADER-FIRST STUB: declarations are the contract, bodies in SceneCamera.cpp are stubs]
// C# source of truth: re/cs_out/Assembly-CSharp/SceneCamera.cs   (translate line by line, keep names; see README "Conventions")
#pragma once
#include <string>
#include <vector>
#include "miniunity.hpp"
using namespace miniunity;

class SceneCamera {
public:
  struct InitParam { float fov=9.5f, angleX=-47, angleY=0, angleZ=0, dist=39.55f, nearClip=5, farClip=100, scaleW=0.0625f; bool targetCamera=true; float viewY=0, viewH=1; float yOffset=0; bool bBGClear=false; InitParam(){} InitParam(float fov,float ax,float ay,float az,float dist,float n,float f,float sw,bool target,float vy=0,float vh=1,float yo=0,bool bg=false):fov(fov),angleX(ax),angleY(ay),angleZ(az),dist(dist),nearClip(n),farClip(f),scaleW(sw),targetCamera(target),viewY(vy),viewH(vh),yOffset(yo),bBGClear(bg){} void Apply(); Vector3 GetAngleVector() const { return Vector3(angleX,angleY,angleZ); } };
  static Vector3 s_camera_pos; static Quaternion s_billboardRot;
  // [WP2 port of the rest of SceneCamera.cs: members the title/town camera needs; WP5a keeps the file]
  static float s_fov, s_nearClip, s_farClip, s_scaleW, s_yOffset, s_lookDownAdjust, s_distanceAdjust, s_distance;
  static Vector3 s_angle, s_target, s_position, s_camUp; static bool s_targetCamera; static Quaternion s_yReverse; static GameObject* s_clearPlane; static Camera* s_DSSAOrthoCamera;
  static void SetProjectionFitScreenByWidth(); static void Setup(); static void SetAngleYDeg(float y); static void SetDistance(float d); static void AddDistance(float d); static void SubDistance(float d);
  static void SetFOV(float sin, float cos); static void SetScaleW(float w); static void SetViewAndCenterY(Rect view, float yOffset);
  static void SystemInit();
  static void SetPosition(Vector3 p);
  static void SetTarget(Vector3 p);
  static void SetAngleDeg(Vector3 deg);
  static void SetNear(float v);
  static void SetFar(float v);
  static void SetFOV2(float deg);
  static void SetYOffset(float v);
  static void SetLookDownAdjust(float v);
  static void SetTargetCamera(bool bTarget);
  static void Update();
  static void LayoutUpdate();                    // landscape: once per frame, eases the world shift (Layout.hpp)
};

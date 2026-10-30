// miniunity/math.hpp -- value types with Unity's names/semantics.            OWNER: WP1 (engine core)
// Left-handed, +Y up, column-major matrices (m[row + col*4], like GL), Quaternion.Euler = qY*qX*qZ.
#pragma once
#include <cstdint>
#include <cmath>

namespace miniunity {

struct Vector2 { float x=0,y=0; static const Vector2 zero, one; Vector2(){} Vector2(float x,float y):x(x),y(y){}
  void Set(float nx,float ny){x=nx;y=ny;} float magnitude() const; Vector2 normalized() const; static Vector2 Lerp(Vector2,Vector2,float);
  Vector2 operator+(Vector2 o)const; Vector2 operator-(Vector2 o)const; Vector2 operator*(float f)const; };
struct Vector3 { float x=0,y=0,z=0; static const Vector3 zero, one, forward, up, right, down;
  Vector3(){} Vector3(float x,float y,float z=0):x(x),y(y),z(z){} void Set(float nx,float ny,float nz){x=nx;y=ny;z=nz;}
  float magnitude() const; Vector3 normalized() const; Vector3 operator+(Vector3)const; Vector3 operator-(Vector3)const; Vector3 operator*(float)const;
  static Vector3 Lerp(Vector3,Vector3,float); static Vector3 Scale(Vector3,Vector3); static Vector3 ClampMagnitude(Vector3,float);
  static float Dot(Vector3,Vector3); static Vector3 Cross(Vector3,Vector3);
  Vector3 operator-() const { return Vector3(-x,-y,-z); }                                     // WP1 additions (additive)
  static float Distance(Vector3 a, Vector3 b) { return (a-b).magnitude(); } };
struct Quaternion { float x=0,y=0,z=0,w=1; static const Quaternion identity;
  static Quaternion Euler(Vector3 deg);      // Unity: q = qY * qX * qZ (a vector is rotated by Z first, then X, then Y); degrees
  static Quaternion Euler(float x,float y,float z);
  static Quaternion AngleAxis(float deg, Vector3 axis); static Quaternion Inverse(Quaternion);
  static Quaternion LookRotation(Vector3 fwd, Vector3 up = Vector3::up);
  static Quaternion Slerp(Quaternion a, Quaternion b, float t);       // shortest arc, t clamped (WP1 addition)
  static Quaternion Lerp(Quaternion a, Quaternion b, float t);        // normalised lerp, shortest arc
  static float Dot(Quaternion a, Quaternion b) { return a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w; }
  Quaternion normalized() const;
  Vector3 eulerAngles() const; void SetEulerAngles(Vector3 deg);   // C# `q.eulerAngles = v`
  Quaternion operator*(Quaternion)const; Vector3 operator*(Vector3)const; };
struct Color { float r=1,g=1,b=1,a=1; static const Color white, black, clear, red, yellow, gray, cyan; Color(){} Color(float r,float g,float b,float a=1):r(r),g(g),b(b),a(a){}
  static Color Lerp(Color,Color,float); Color operator*(Color)const; };
struct Color32 { uint8_t r=255,g=255,b=255,a=255; Color32(){} Color32(uint8_t r,uint8_t g,uint8_t b,uint8_t a):r(r),g(g),b(b),a(a){} Color32(const Color&); /* implicit like C#: round(c*255) */ };
struct Rect { float x=0,y=0,width=0,height=0; Rect(){} Rect(float x,float y,float w,float h):x(x),y(y),width(w),height(h){} };  // NB: FieldSymbol uses it as (xMin,yMin,xMax,yMax)-ish by name only
struct Bounds { Vector3 center, extents; Vector3 size() const; Vector3 min() const; Vector3 max() const;
  void Encapsulate(Vector3 p);                 // WP1 addition (an empty Bounds{} encapsulates its first point as the new box)
};
struct Ray { Vector3 origin, direction; };
struct RaycastHit { Vector3 point; float distance=0; };
struct Matrix4x4 { float m[16]; /* column-major like GL; m[row + col*4]; operator()(row,col) == C# this[row,col] */
  float& operator()(int r,int c){return m[r+c*4];} float operator()(int r,int c) const {return m[r+c*4];}
  static Matrix4x4 identity(); Matrix4x4 operator*(const Matrix4x4&)const;
  static Matrix4x4 Perspective(float fovDegY,float aspect,float zn,float zf);   // Unity/GL clip: z in [-1,1], like gluPerspective
  static Matrix4x4 Ortho(float left,float right,float bottom,float top,float zn,float zf);
  static Matrix4x4 TRS(Vector3 t,Quaternion r,Vector3 s); Vector3 MultiplyPoint(Vector3)const;
  Vector3 MultiplyPoint3x4(Vector3) const;     // affine, no divide by w (WP1 additions)
  Vector3 MultiplyVector(Vector3) const;       // no translation
  Matrix4x4 inverse() const; /* general 4x4 inverse (identity if singular) */ };
namespace Mathf { float Abs(float); float Sqrt(float); float Sin(float rad); float Cos(float rad); float Atan2(float y,float x); extern const float Rad2Deg, Deg2Rad, PI; }

}  // namespace miniunity

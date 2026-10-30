// engine/math.cpp -- miniunity math types (WP1).
#include "miniunity/math.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace miniunity {

namespace Mathf {
const float PI = 3.14159265358979f, Rad2Deg = 57.29578f, Deg2Rad = 0.0174532925f;
float Abs(float f) { return std::fabs(f); }
float Sqrt(float f) { return std::sqrt(f); }
float Sin(float r) { return std::sin(r); }
float Cos(float r) { return std::cos(r); }
float Atan2(float y, float x) { return std::atan2(y, x); }
}  // namespace Mathf

const Vector2 Vector2::zero(0, 0), Vector2::one(1, 1);
float Vector2::magnitude() const { return std::sqrt(x * x + y * y); }
Vector2 Vector2::normalized() const { float m = magnitude(); return m > 1e-5f ? Vector2(x / m, y / m) : Vector2(); }
Vector2 Vector2::Lerp(Vector2 a, Vector2 b, float t) { t = std::clamp(t, 0.f, 1.f); return Vector2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t); }
Vector2 Vector2::operator+(Vector2 o) const { return Vector2(x + o.x, y + o.y); }
Vector2 Vector2::operator-(Vector2 o) const { return Vector2(x - o.x, y - o.y); }
Vector2 Vector2::operator*(float f) const { return Vector2(x * f, y * f); }

const Vector3 Vector3::zero(0, 0, 0), Vector3::one(1, 1, 1), Vector3::forward(0, 0, 1), Vector3::up(0, 1, 0), Vector3::right(1, 0, 0), Vector3::down(0, -1, 0);
float Vector3::magnitude() const { return std::sqrt(x * x + y * y + z * z); }
Vector3 Vector3::normalized() const { float m = magnitude(); return m > 1e-5f ? Vector3(x / m, y / m, z / m) : Vector3(); }
Vector3 Vector3::operator+(Vector3 o) const { return Vector3(x + o.x, y + o.y, z + o.z); }
Vector3 Vector3::operator-(Vector3 o) const { return Vector3(x - o.x, y - o.y, z - o.z); }
Vector3 Vector3::operator*(float f) const { return Vector3(x * f, y * f, z * f); }
Vector3 Vector3::Lerp(Vector3 a, Vector3 b, float t) { t = std::clamp(t, 0.f, 1.f); return a + (b - a) * t; }
Vector3 Vector3::Scale(Vector3 a, Vector3 b) { return Vector3(a.x * b.x, a.y * b.y, a.z * b.z); }
Vector3 Vector3::ClampMagnitude(Vector3 v, float m) { float l = v.magnitude(); return l > m ? v * (m / l) : v; }
float Vector3::Dot(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vector3 Vector3::Cross(Vector3 a, Vector3 b) { return Vector3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }

const Quaternion Quaternion::identity;
Quaternion Quaternion::AngleAxis(float deg, Vector3 axis) {
  axis = axis.normalized();
  float h = deg * Mathf::Deg2Rad * 0.5f, s = std::sin(h);
  Quaternion q; q.x = axis.x * s; q.y = axis.y * s; q.z = axis.z * s; q.w = std::cos(h); return q;
}
Quaternion Quaternion::operator*(Quaternion b) const {
  Quaternion q;
  q.w = w * b.w - x * b.x - y * b.y - z * b.z;
  q.x = w * b.x + x * b.w + y * b.z - z * b.y;
  q.y = w * b.y - x * b.z + y * b.w + z * b.x;
  q.z = w * b.z + x * b.y - y * b.x + z * b.w;
  return q;
}
Vector3 Quaternion::operator*(Vector3 v) const {
  // v' = v + 2w(u x v) + 2u x (u x v), u = (x,y,z)
  Vector3 u(x, y, z);
  Vector3 c = Vector3::Cross(u, v);
  Vector3 cc = Vector3::Cross(u, c);
  return v + c * (2.f * w) + cc * 2.f;
}
Quaternion Quaternion::Euler(float x, float y, float z) {
  return AngleAxis(y, Vector3(0, 1, 0)) * AngleAxis(x, Vector3(1, 0, 0)) * AngleAxis(z, Vector3(0, 0, 1));   // qY * qX * qZ
}
Quaternion Quaternion::Euler(Vector3 d) { return Euler(d.x, d.y, d.z); }
Quaternion Quaternion::Inverse(Quaternion q) {
  float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
  if (n < 1e-12f) return identity;
  Quaternion r; r.x = -q.x / n; r.y = -q.y / n; r.z = -q.z / n; r.w = q.w / n; return r;
}
static Quaternion FromBasis(Vector3 r, Vector3 u, Vector3 f) {   // columns of the rotation matrix
  float m00 = r.x, m10 = r.y, m20 = r.z, m01 = u.x, m11 = u.y, m21 = u.z, m02 = f.x, m12 = f.y, m22 = f.z;
  Quaternion q; float t;
  float tr = m00 + m11 + m22;
  if (tr > 0) { t = std::sqrt(tr + 1.f) * 2.f; q.w = 0.25f * t; q.x = (m21 - m12) / t; q.y = (m02 - m20) / t; q.z = (m10 - m01) / t; }
  else if (m00 > m11 && m00 > m22) { t = std::sqrt(1.f + m00 - m11 - m22) * 2.f; q.w = (m21 - m12) / t; q.x = 0.25f * t; q.y = (m01 + m10) / t; q.z = (m02 + m20) / t; }
  else if (m11 > m22) { t = std::sqrt(1.f + m11 - m00 - m22) * 2.f; q.w = (m02 - m20) / t; q.x = (m01 + m10) / t; q.y = 0.25f * t; q.z = (m12 + m21) / t; }
  else { t = std::sqrt(1.f + m22 - m00 - m11) * 2.f; q.w = (m10 - m01) / t; q.x = (m02 + m20) / t; q.y = (m12 + m21) / t; q.z = 0.25f * t; }
  return q;
}
Quaternion Quaternion::LookRotation(Vector3 fwd, Vector3 up) {
  Vector3 f = fwd.normalized();
  if (f.magnitude() < 0.5f) return identity;
  Vector3 r = Vector3::Cross(up, f).normalized();
  if (r.magnitude() < 0.5f) r = Vector3::Cross(Vector3(0, 0, 1), f).normalized();   // up parallel to forward
  Vector3 u = Vector3::Cross(f, r);
  return FromBasis(r, u, f);
}
Quaternion Quaternion::normalized() const {
  float n = std::sqrt(x * x + y * y + z * z + w * w);
  if (n < 1e-12f) return identity;
  Quaternion q; q.x = x / n; q.y = y / n; q.z = z / n; q.w = w / n; return q;
}
Quaternion Quaternion::Lerp(Quaternion a, Quaternion b, float t) {
  t = std::clamp(t, 0.f, 1.f);
  if (Dot(a, b) < 0) { b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w; }
  Quaternion q; q.x = a.x + (b.x - a.x) * t; q.y = a.y + (b.y - a.y) * t; q.z = a.z + (b.z - a.z) * t; q.w = a.w + (b.w - a.w) * t;
  return q.normalized();
}
Quaternion Quaternion::Slerp(Quaternion a, Quaternion b, float t) {
  t = std::clamp(t, 0.f, 1.f);
  float d = Dot(a, b);
  if (d < 0) { b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w; d = -d; }
  if (d > 0.9995f) return Lerp(a, b, t);
  float th = std::acos(std::min(d, 1.f)), s = std::sin(th);
  float wa = std::sin((1 - t) * th) / s, wb = std::sin(t * th) / s;
  Quaternion q; q.x = a.x * wa + b.x * wb; q.y = a.y * wa + b.y * wb; q.z = a.z * wa + b.z * wb; q.w = a.w * wa + b.w * wb;
  return q;
}
Vector3 Quaternion::eulerAngles() const {
  // R = Ry(y) Rx(x) Rz(z)   =>   R12 = -sin x
  float xx = x, yy = y, zz = z, ww = w;
  float R12 = 2 * (yy * zz - xx * ww);
  float R02 = 2 * (xx * zz + yy * ww), R22 = 1 - 2 * (xx * xx + yy * yy);
  float R10 = 2 * (xx * yy + zz * ww), R11 = 1 - 2 * (xx * xx + zz * zz);
  float ex, ey, ez;
  if (std::fabs(R12) < 0.999999f) { ex = std::asin(std::clamp(-R12, -1.f, 1.f)); ey = std::atan2(R02, R22); ez = std::atan2(R10, R11); }
  else { ex = std::copysign(Mathf::PI * 0.5f, -R12); float R20 = 2 * (xx * zz - yy * ww), R00 = 1 - 2 * (yy * yy + zz * zz); ey = std::atan2(-R20, R00); ez = 0; }
  auto wrap = [](float d) { d *= Mathf::Rad2Deg; d = std::fmod(d, 360.f); if (d < 0) d += 360.f; return d; };
  return Vector3(wrap(ex), wrap(ey), wrap(ez));
}
void Quaternion::SetEulerAngles(Vector3 d) { *this = Euler(d); }

const Color Color::white(1, 1, 1, 1), Color::black(0, 0, 0, 1), Color::clear(0, 0, 0, 0), Color::red(1, 0, 0, 1), Color::yellow(1, 0.92f, 0.016f, 1),
  Color::gray(0.5f, 0.5f, 0.5f, 1), Color::cyan(0, 1, 1, 1);
Color Color::Lerp(Color a, Color b, float t) { t = std::clamp(t, 0.f, 1.f); return Color(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t); }
Color Color::operator*(Color o) const { return Color(r * o.r, g * o.g, b * o.b, a * o.a); }
static uint8_t To8(float f) { return (uint8_t)std::clamp((int)std::lround(f * 255.f), 0, 255); }
Color32::Color32(const Color& c) : r(To8(c.r)), g(To8(c.g)), b(To8(c.b)), a(To8(c.a)) {}

Vector3 Bounds::size() const { return extents * 2.f; }
Vector3 Bounds::min() const { return center - extents; }
Vector3 Bounds::max() const { return center + extents; }

void Bounds::Encapsulate(Vector3 p) {
  Vector3 mn = min(), mx = max();
  mn.x = std::min(mn.x, p.x); mn.y = std::min(mn.y, p.y); mn.z = std::min(mn.z, p.z);
  mx.x = std::max(mx.x, p.x); mx.y = std::max(mx.y, p.y); mx.z = std::max(mx.z, p.z);
  center = (mn + mx) * 0.5f; extents = (mx - mn) * 0.5f;
}
Vector3 Matrix4x4::MultiplyPoint3x4(Vector3 p) const {
  return Vector3(m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12], m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13], m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]);
}
Vector3 Matrix4x4::MultiplyVector(Vector3 p) const {
  return Vector3(m[0] * p.x + m[4] * p.y + m[8] * p.z, m[1] * p.x + m[5] * p.y + m[9] * p.z, m[2] * p.x + m[6] * p.y + m[10] * p.z);
}
Matrix4x4 Matrix4x4::inverse() const {
  // cofactor expansion (MESA gluInvertMatrix), column-major in/out
  const float* a = m; float inv[16];
  inv[0] = a[5]*a[10]*a[15] - a[5]*a[11]*a[14] - a[9]*a[6]*a[15] + a[9]*a[7]*a[14] + a[13]*a[6]*a[11] - a[13]*a[7]*a[10];
  inv[4] = -a[4]*a[10]*a[15] + a[4]*a[11]*a[14] + a[8]*a[6]*a[15] - a[8]*a[7]*a[14] - a[12]*a[6]*a[11] + a[12]*a[7]*a[10];
  inv[8] = a[4]*a[9]*a[15] - a[4]*a[11]*a[13] - a[8]*a[5]*a[15] + a[8]*a[7]*a[13] + a[12]*a[5]*a[11] - a[12]*a[7]*a[9];
  inv[12] = -a[4]*a[9]*a[14] + a[4]*a[10]*a[13] + a[8]*a[5]*a[14] - a[8]*a[6]*a[13] - a[12]*a[5]*a[10] + a[12]*a[6]*a[9];
  inv[1] = -a[1]*a[10]*a[15] + a[1]*a[11]*a[14] + a[9]*a[2]*a[15] - a[9]*a[3]*a[14] - a[13]*a[2]*a[11] + a[13]*a[3]*a[10];
  inv[5] = a[0]*a[10]*a[15] - a[0]*a[11]*a[14] - a[8]*a[2]*a[15] + a[8]*a[3]*a[14] + a[12]*a[2]*a[11] - a[12]*a[3]*a[10];
  inv[9] = -a[0]*a[9]*a[15] + a[0]*a[11]*a[13] + a[8]*a[1]*a[15] - a[8]*a[3]*a[13] - a[12]*a[1]*a[11] + a[12]*a[3]*a[9];
  inv[13] = a[0]*a[9]*a[14] - a[0]*a[10]*a[13] - a[8]*a[1]*a[14] + a[8]*a[2]*a[13] + a[12]*a[1]*a[10] - a[12]*a[2]*a[9];
  inv[2] = a[1]*a[6]*a[15] - a[1]*a[7]*a[14] - a[5]*a[2]*a[15] + a[5]*a[3]*a[14] + a[13]*a[2]*a[7] - a[13]*a[3]*a[6];
  inv[6] = -a[0]*a[6]*a[15] + a[0]*a[7]*a[14] + a[4]*a[2]*a[15] - a[4]*a[3]*a[14] - a[12]*a[2]*a[7] + a[12]*a[3]*a[6];
  inv[10] = a[0]*a[5]*a[15] - a[0]*a[7]*a[13] - a[4]*a[1]*a[15] + a[4]*a[3]*a[13] + a[12]*a[1]*a[7] - a[12]*a[3]*a[5];
  inv[14] = -a[0]*a[5]*a[14] + a[0]*a[6]*a[13] + a[4]*a[1]*a[14] - a[4]*a[2]*a[13] - a[12]*a[1]*a[6] + a[12]*a[2]*a[5];
  inv[3] = -a[1]*a[6]*a[11] + a[1]*a[7]*a[10] + a[5]*a[2]*a[11] - a[5]*a[3]*a[10] - a[9]*a[2]*a[7] + a[9]*a[3]*a[6];
  inv[7] = a[0]*a[6]*a[11] - a[0]*a[7]*a[10] - a[4]*a[2]*a[11] + a[4]*a[3]*a[10] + a[8]*a[2]*a[7] - a[8]*a[3]*a[6];
  inv[11] = -a[0]*a[5]*a[11] + a[0]*a[7]*a[9] + a[4]*a[1]*a[11] - a[4]*a[3]*a[9] - a[8]*a[1]*a[7] + a[8]*a[3]*a[5];
  inv[15] = a[0]*a[5]*a[10] - a[0]*a[6]*a[9] - a[4]*a[1]*a[10] + a[4]*a[2]*a[9] + a[8]*a[1]*a[6] - a[8]*a[2]*a[5];
  float det = a[0]*inv[0] + a[1]*inv[4] + a[2]*inv[8] + a[3]*inv[12];
  if (std::fabs(det) < 1e-30f) return identity();
  Matrix4x4 r; for (int i = 0; i < 16; i++) r.m[i] = inv[i] / det; return r;
}

Matrix4x4 Matrix4x4::identity() { Matrix4x4 r; std::memset(r.m, 0, sizeof(r.m)); r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1; return r; }
Matrix4x4 Matrix4x4::operator*(const Matrix4x4& o) const {
  Matrix4x4 r;
  for (int c = 0; c < 4; c++)
    for (int rr = 0; rr < 4; rr++) {
      float s = 0;
      for (int k = 0; k < 4; k++) s += m[rr + k * 4] * o.m[k + c * 4];
      r.m[rr + c * 4] = s;
    }
  return r;
}
Matrix4x4 Matrix4x4::Perspective(float fovDegY, float aspect, float zn, float zf) {
  Matrix4x4 r; std::memset(r.m, 0, sizeof(r.m));
  float f = 1.f / std::tan(fovDegY * Mathf::Deg2Rad * 0.5f);
  r.m[0] = f / aspect; r.m[5] = f; r.m[10] = (zf + zn) / (zn - zf); r.m[11] = -1.f; r.m[14] = 2.f * zf * zn / (zn - zf);
  return r;
}
Matrix4x4 Matrix4x4::Ortho(float l, float r_, float b, float t, float zn, float zf) {
  Matrix4x4 r = identity();
  r.m[0] = 2.f / (r_ - l); r.m[5] = 2.f / (t - b); r.m[10] = -2.f / (zf - zn);
  r.m[12] = -(r_ + l) / (r_ - l); r.m[13] = -(t + b) / (t - b); r.m[14] = -(zf + zn) / (zf - zn);
  return r;
}
Matrix4x4 Matrix4x4::TRS(Vector3 t, Quaternion q, Vector3 s) {
  float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z, xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z, wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
  Matrix4x4 r = identity();
  r.m[0] = (1 - 2 * (yy + zz)) * s.x; r.m[1] = 2 * (xy + wz) * s.x;       r.m[2] = 2 * (xz - wy) * s.x;
  r.m[4] = 2 * (xy - wz) * s.y;       r.m[5] = (1 - 2 * (xx + zz)) * s.y; r.m[6] = 2 * (yz + wx) * s.y;
  r.m[8] = 2 * (xz + wy) * s.z;       r.m[9] = 2 * (yz - wx) * s.z;       r.m[10] = (1 - 2 * (xx + yy)) * s.z;
  r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
  return r;
}
Vector3 Matrix4x4::MultiplyPoint(Vector3 p) const {
  float x = m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12], y = m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13], z = m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14];
  float w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
  if (w != 0.f && w != 1.f) { x /= w; y /= w; z /= w; }
  return Vector3(x, y, z);
}

}  // namespace miniunity

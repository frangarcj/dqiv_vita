// engine/colliders.cpp -- BoxCollider / SphereCollider / MeshCollider raycasts (WP1). No physics engine, only `collider.Raycast(ray, out hit, maxDistance)`.
// Unity rules kept: a ray that starts INSIDE a box/sphere does not hit it; the hit distance is in world units; ray.direction need not be normalised.
// MeshCollider tests every triangle double-sided (Unity is one-sided, but the UI frames are flat quads hit from the camera side; being forgiving here
// avoids dead buttons if a mesh winding differs).
#include "miniunity.hpp"
#include "engine/engine_internal.hpp"

#include <algorithm>
#include <cmath>

namespace miniunity {

namespace {
struct LocalRay { Vector3 o, d; };
LocalRay ToLocal(const Collider* c, const Ray& r) {
  Matrix4x4 w2l = c->transform()->worldToLocalMatrix();
  return LocalRay{ w2l.MultiplyPoint3x4(r.origin), w2l.MultiplyVector(r.direction) };
}
bool Finish(const Ray& r, float t, float maxDist, RaycastHit* hit) {
  float len = r.direction.magnitude();
  if (t < 0 || t * len > maxDist) return false;
  if (hit) { hit->point = r.origin + r.direction * t; hit->distance = t * len; }
  return true;
}
}  // namespace

bool BoxCollider::Raycast(const Ray& ray, RaycastHit* hit, float maxDist) const {
  if (!transform()) return false;
  LocalRay l = ToLocal(this, ray);
  Vector3 mn = center - size * 0.5f, mx = center + size * 0.5f;
  const float o[3] = { l.o.x, l.o.y, l.o.z }, d[3] = { l.d.x, l.d.y, l.d.z }, lo[3] = { mn.x, mn.y, mn.z }, hi[3] = { mx.x, mx.y, mx.z };
  float tmin = 0, tmax = 1e30f;
  bool inside = true;
  for (int i = 0; i < 3; i++) {
    if (o[i] < lo[i] || o[i] > hi[i]) inside = false;
    if (std::fabs(d[i]) < 1e-12f) { if (o[i] < lo[i] || o[i] > hi[i]) return false; continue; }
    float t1 = (lo[i] - o[i]) / d[i], t2 = (hi[i] - o[i]) / d[i];
    if (t1 > t2) std::swap(t1, t2);
    tmin = std::max(tmin, t1); tmax = std::min(tmax, t2);
    if (tmin > tmax) return false;
  }
  if (inside) return false;
  return Finish(ray, tmin, maxDist, hit);
}

bool SphereCollider::Raycast(const Ray& ray, RaycastHit* hit, float maxDist) const {
  if (!transform()) return false;
  Vector3 s = transform()->lossyScale();
  float sc = std::max(std::fabs(s.x), std::max(std::fabs(s.y), std::fabs(s.z)));
  Vector3 c = transform()->TransformPoint(center);
  float rad = radius * sc;
  Vector3 oc = ray.origin - c;
  float a = Vector3::Dot(ray.direction, ray.direction), b = 2.f * Vector3::Dot(oc, ray.direction), cc = Vector3::Dot(oc, oc) - rad * rad;
  if (a < 1e-20f || cc < 0) return false;                  // degenerate / origin inside
  float disc = b * b - 4 * a * cc;
  if (disc < 0) return false;
  float t = (-b - std::sqrt(disc)) / (2 * a);
  return Finish(ray, t, maxDist, hit);
}

bool MeshCollider::Raycast(const Ray& ray, RaycastHit* hit, float maxDist) const {
  if (!transform() || !sharedMesh) return false;
  LocalRay l = ToLocal(this, ray);
  const std::vector<Vector3>& v = sharedMesh->vertices;
  float best = 1e30f;
  for (const std::vector<int>& tris : sharedMesh->subTriangles) {
    for (size_t i = 0; i + 2 < tris.size(); i += 3) {
      if (tris[i] >= (int)v.size() || tris[i + 1] >= (int)v.size() || tris[i + 2] >= (int)v.size()) continue;
      const Vector3 &a = v[tris[i]], &b = v[tris[i + 1]], &c = v[tris[i + 2]];
      Vector3 e1 = b - a, e2 = c - a, p = Vector3::Cross(l.d, e2);
      float det = Vector3::Dot(e1, p);
      if (std::fabs(det) < 1e-12f) continue;
      float inv = 1.f / det;
      Vector3 tv = l.o - a;
      float u = Vector3::Dot(tv, p) * inv;
      if (u < 0 || u > 1) continue;
      Vector3 q = Vector3::Cross(tv, e1);
      float w = Vector3::Dot(l.d, q) * inv;
      if (w < 0 || u + w > 1) continue;
      float t = Vector3::Dot(e2, q) * inv;
      if (t >= 0 && t < best) best = t;
    }
  }
  if (best >= 1e30f) return false;
  return Finish(ray, best, maxDist, hit);
}

}  // namespace miniunity

// engine/renderer.cpp -- Material / Renderer / MeshFilter instancing glue (WP2 skeleton; WP1 moves these to engine/material.cpp + components.cpp).
// Camera, render loop, sorting and Present live in engine/camera.cpp; TextMesh in engine/textmesh.cpp.
#include "miniunity.hpp"
#include "engine/backend.hpp"
#include "engine/engine_internal.hpp"
#include "config.h"
#include "utils/logger.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace miniunity {

// ------------------------------------------------------------------------------------------------ Material / Renderer / MeshFilter
// Material (property blocks), Renderer::material()/materials(), MeshFilter::mesh() and Mesh::RecalculateBounds moved to engine/components.cpp (WP1).
void Renderer::setMaterial(Material* m) { sharedMaterial = m; materialIsInstance = false; if (mats.empty()) mats.push_back(m); else mats[0] = m; }
void Renderer::setSharedMaterial(Material* m) { setMaterial(m); }
void Renderer::setMaterials(const std::vector<Material*>& v) { mats = v; sharedMaterial = v.empty() ? nullptr : v[0]; materialIsInstance = false; }
void MeshFilter::setMesh(Mesh* m) { sharedMesh = m; meshIsInstance = false; }


}  // namespace miniunity

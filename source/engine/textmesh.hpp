// engine/textmesh.hpp -- TextMesh glyph mesh generation (WP2, private to engine/).
#pragma once
#include "miniunity.hpp"

namespace miniunity {
// Returns the (cached) glyph mesh of a TextMesh, rebuilding it when text/font/size/anchor/alignment/colour changed; nullptr when there is nothing to draw.
Mesh* TextMeshGetMesh(TextMesh* tm);
}

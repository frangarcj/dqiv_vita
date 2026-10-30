// engine/textmesh.cpp -- Unity 4 TextMesh for the game's bitmap fonts (WP2). Algorithm: re/arch/03_MINI_UNITY.md §5.
//
//   scale      = characterSize * font.pixelScale (0.1)           -> characterSize 10 == 1 world unit per font pixel
//   line pitch = font.lineSpacing * textMesh.lineSpacing * scale
//   glyph quad = pen + vert.x*scale .. + vert.width*scale, lineTop + vert.y*scale .. + vert.height*scale   (vert.y/height are negative: y goes DOWN from the line top)
//   uv         = Unity CharacterInfo rect; `flipped` glyphs are stored rotated 90 degrees clockwise in the atlas (verified on the EN atlas: the
//                star / down-triangle / N glyphs), so glyph corner TL<-atlas TR, TR<-BR, BR<-BL, BL<-TL.
//   anchor     = (col 0..2, row 0..2) of the text block (block = longest line x number of lines * pitch); alignment shifts each line in the block.
//   rich text  = <color=name|#rgb|#rrggbb|#rrggbbaa> ... </color>; <b> <i> <size=N> tags are consumed (no visual effect, like the game's bitmap font).
//   quads are wound clockwise seen from -Z (Unity front face), local space = XY plane facing -Z.
#include "miniunity.hpp"
#include "engine/textmesh.hpp"
#include "utils/logger.h"

#include <cmath>
#include <cstring>
#include <strings.h>
#include <string>
#include <vector>

namespace miniunity {

namespace {

struct TMCache {
  Mesh* mesh = nullptr;
  std::string text; const Font* font = nullptr; size_t fontChars = 0;
  float characterSize = 0, lineSpacing = 0, tabSize = 0; int anchor = -1, alignment = -1; bool richText = true; Color color;
  bool valid = false;
};

struct Glyph { Font::CharInfo ci; Color32 col; bool tab = false; };

uint32_t NextCodepoint(const std::string& s, size_t& i) {
  unsigned char c = (unsigned char)s[i++];
  if (c < 0x80) return c;
  int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
  uint32_t cp = n == 3 ? (c & 0x07) : n == 2 ? (c & 0x0F) : n == 1 ? (c & 0x1F) : c;
  while (n-- > 0 && i < s.size()) cp = (cp << 6) | ((unsigned char)s[i++] & 0x3F);
  return cp;
}

bool NamedColor(const std::string& n, Color& c) {
  struct E { const char* name; float r, g, b; };
  static const E t[] = {
    { "red", 1, 0, 0 }, { "green", 0, 1, 0 }, { "blue", 0, 0, 1 }, { "white", 1, 1, 1 }, { "black", 0, 0, 0 }, { "yellow", 1, 0.92f, 0.016f },
    { "cyan", 0, 1, 1 }, { "magenta", 1, 0, 1 }, { "gray", 0.5f, 0.5f, 0.5f }, { "grey", 0.5f, 0.5f, 0.5f }, { "orange", 1, 0.5f, 0 },
    { "aqua", 0, 1, 1 }, { "fuchsia", 1, 0, 1 }, { "lime", 0, 1, 0 }, { "navy", 0, 0, 0.5f }, { "olive", 0.5f, 0.5f, 0 }, { "purple", 0.5f, 0, 0.5f },
    { "silver", 0.75f, 0.75f, 0.75f }, { "teal", 0, 0.5f, 0.5f }, { "maroon", 0.5f, 0, 0 }, { "brown", 0.65f, 0.16f, 0.16f },
    { "lightblue", 0.68f, 0.85f, 0.9f }, { "darkblue", 0, 0, 0.55f },
  };
  for (const E& e : t) if (!strcasecmp(e.name, n.c_str())) { c = Color(e.r, e.g, e.b, 1.f); return true; }
  return false;
}
int Hex(char ch) { return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1; }
bool ParseColor(const std::string& v, Color& c) {
  if (!v.empty() && v[0] == '#') {
    std::string h = v.substr(1); int d[8]; size_t n = h.size();
    for (size_t k = 0; k < n && k < 8; k++) if ((d[k] = Hex(h[k])) < 0) return false;
    if (n == 3 || n == 4) { c = Color(d[0] * 17 / 255.f, d[1] * 17 / 255.f, d[2] * 17 / 255.f, n == 4 ? d[3] * 17 / 255.f : 1.f); return true; }
    if (n == 6 || n == 8) { c = Color((d[0] * 16 + d[1]) / 255.f, (d[2] * 16 + d[3]) / 255.f, (d[4] * 16 + d[5]) / 255.f, n == 8 ? (d[6] * 16 + d[7]) / 255.f : 1.f); return true; }
    return false;
  }
  return NamedColor(v, c);
}

void Build(TextMesh* tm, TMCache& cc) {
  Mesh* mesh = cc.mesh;
  mesh->Clear();
  const Font* font = tm->font;
  if (!font || tm->text.empty()) return;

  // ---- parse (rich text) into lines of glyphs
  std::vector<std::vector<Glyph>> lines(1);
  std::vector<Color> colorStack; Color cur = Color::white;
  const std::string& s = tm->text;
  size_t i = 0; Font::CharInfo space{}; bool haveSpace = font->GetCharacterInfo(' ', space);
  while (i < s.size()) {
    if (tm->richText && s[i] == '<') {
      size_t close = s.find('>', i);
      if (close != std::string::npos && close - i < 40) {
        std::string tag = s.substr(i + 1, close - i - 1), low = tag;
        for (char& ch : low) ch = (char)tolower((unsigned char)ch);
        bool consumed = true;
        if (low.rfind("color=", 0) == 0) { Color c; if (ParseColor(tag.substr(6), c)) { colorStack.push_back(cur); cur = c; } else consumed = false; }
        else if (low == "/color") { if (!colorStack.empty()) { cur = colorStack.back(); colorStack.pop_back(); } }
        else if (low == "b" || low == "/b" || low == "i" || low == "/i" || low == "/size" || low.rfind("size=", 0) == 0 || low.rfind("material=", 0) == 0 || low == "/material") {}
        else consumed = false;
        if (consumed) { i = close + 1; continue; }
      }
    }
    uint32_t cp = NextCodepoint(s, i);
    if (cp == '\n') { lines.emplace_back(); continue; }
    if (cp == '\r') continue;
    if (cp > 0xFFFF) continue;
    Glyph g{};
    if (cp == '\t') {                                    // tabSize x the space advance
      if (!haveSpace) continue;
      g.ci = space; g.ci.width = space.width * tm->tabSize; g.ci.vert = Rect(0, 0, 0, 0); g.tab = true;   // advance fixed up to the next tab stop below
    } else if (!font->GetCharacterInfo((uint16_t)cp, g.ci)) continue;
    Color vc = cur * tm->color;
    g.col = Color32(vc);
    lines.back().push_back(g);
  }

  // ---- layout
  float pixelScale = font->pixelScale > 0 ? font->pixelScale : 0.1f;
  float scale = tm->characterSize * pixelScale;
  float lineH = font->lineSpacing * tm->lineSpacing * scale;
  // [WP4] '\t' advances to the next tab stop (multiples of tabSize x the space advance from the line start). A fixed tabSize x space advance pushed the green
  // "MP Cost:<tab>  2" of the spell buttons (tabSize 8) outside its 156-unit button; with tab stops the number lands at the button's right edge as designed.
  for (auto& line : lines) {
    float cum = 0;
    for (Glyph& g : line) {
      if (g.tab) { float stop = space.width * tm->tabSize; if (stop > 0.f) g.ci.width = stop - std::fmod(cum, stop); }
      cum += g.ci.width;
    }
  }
  std::vector<float> widths(lines.size(), 0.f);
  float blockW = 0;
  for (size_t l = 0; l < lines.size(); l++) {
    float w = 0; for (const Glyph& g : lines[l]) w += g.ci.width * scale;
    widths[l] = w; blockW = std::max(blockW, w);
  }
  float blockH = lines.size() * lineH;
  int col = tm->anchor % 3, row = tm->anchor / 3;       // TextAnchor: UpperLeft 0 .. LowerRight 8
  float ox = -blockW * col * 0.5f, oy = blockH * row * 0.5f;

  std::vector<Vector3>& V = mesh->vertices; std::vector<Vector2>& UV = mesh->uv; std::vector<Color32>& C = mesh->colors32;
  std::vector<int> tris;
  for (size_t l = 0; l < lines.size(); l++) {
    float pen = ox + tm->alignment * 0.5f * (blockW - widths[l]);
    float lineTop = oy - (float)l * lineH;
    for (const Glyph& g : lines[l]) {
      const Font::CharInfo& ci = g.ci;
      if (ci.vert.width != 0.f && ci.vert.height != 0.f) {
        float x0 = pen + ci.vert.x * scale, x1 = x0 + ci.vert.width * scale;
        float yt = lineTop + ci.vert.y * scale, yb = yt + ci.vert.height * scale;
        float u0 = ci.uv.x, u1 = ci.uv.x + ci.uv.width;
        float vT = std::max(ci.uv.y, ci.uv.y + ci.uv.height), vB = std::min(ci.uv.y, ci.uv.y + ci.uv.height);
        Vector2 uvTL, uvTR, uvBR, uvBL;
        if (!ci.flipped) { uvTL = Vector2(u0, vB); uvTR = Vector2(u1, vB); uvBR = Vector2(u1, vT); uvBL = Vector2(u0, vT); }   // [WP4a] V swapped: verified upright on the EN font in Vita3K (was upside-down glyphs). `flipped` branch NOT re-verified
        else             { uvTL = Vector2(u1, vT); uvTR = Vector2(u1, vB); uvBR = Vector2(u0, vB); uvBL = Vector2(u0, vT); }
        int b = (int)V.size();
        V.push_back(Vector3(x0, yt, tm->offsetZ)); UV.push_back(uvTL); C.push_back(g.col);
        V.push_back(Vector3(x1, yt, tm->offsetZ)); UV.push_back(uvTR); C.push_back(g.col);
        V.push_back(Vector3(x1, yb, tm->offsetZ)); UV.push_back(uvBR); C.push_back(g.col);
        V.push_back(Vector3(x0, yb, tm->offsetZ)); UV.push_back(uvBL); C.push_back(g.col);
        tris.insert(tris.end(), { b, b + 1, b + 2, b, b + 2, b + 3 });
      }
      pen += ci.width * scale;
    }
  }
  mesh->SetTriangles(tris, 0);
}

}  // namespace

Mesh* TextMeshGetMesh(TextMesh* tm) {
  TMCache* cc = (TMCache*)tm->cache.p;
  if (!cc) { cc = new TMCache(); cc->mesh = new Mesh(); cc->mesh->name = "TextMesh"; tm->cache.p = cc; }
  bool same = cc->valid && cc->font == tm->font && cc->fontChars == (tm->font ? tm->font->chars.size() : 0) && cc->text == tm->text &&
              cc->characterSize == tm->characterSize && cc->lineSpacing == tm->lineSpacing && cc->tabSize == tm->tabSize &&
              cc->anchor == tm->anchor && cc->alignment == tm->alignment && cc->richText == tm->richText &&
              cc->color.r == tm->color.r && cc->color.g == tm->color.g && cc->color.b == tm->color.b && cc->color.a == tm->color.a;
  if (!same) {
    Build(tm, *cc);
    cc->text = tm->text; cc->font = tm->font; cc->fontChars = tm->font ? tm->font->chars.size() : 0; cc->characterSize = tm->characterSize;
    cc->lineSpacing = tm->lineSpacing; cc->tabSize = tm->tabSize; cc->anchor = tm->anchor; cc->alignment = tm->alignment; cc->richText = tm->richText;
    cc->color = tm->color; cc->valid = true;
  }
  return cc->mesh->vertices.empty() ? nullptr : cc->mesh;
}

void TextMesh::Awake() {
  if (!gameObject->GetComponent<MeshRenderer>()) gameObject->AddComponent<MeshRenderer>();     // RequireComponent(MeshRenderer)
}
TextMesh::~TextMesh() {
  TMCache* cc = (TMCache*)cache.p;
  if (cc) { delete cc->mesh; delete cc; cache.p = nullptr; }
}

}  // namespace miniunity

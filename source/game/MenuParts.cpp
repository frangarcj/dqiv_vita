// game/MenuParts.cpp -- OWNER: WP4a.  C# source: re/cs_out/Assembly-CSharp/MenuParts.cs
#include "game/MenuParts.hpp"
#include <cfloat>
#include <cstdio>
#include <cstring>
#include "utils/logger.h"

// ------------------------------------------------------------------------------------------------ statics
std::string MenuParts::FONT_NAME;
float MenuParts::BIAS_FONT_SIZE = 0.f, MenuParts::BIAS_FONT_LINE = 0.f, MenuParts::BIAS_FONT_Y = 0.f, MenuParts::NAME_BASE_SCALE = 1.f;
int MenuParts::APPLY_HERO = 0;

MenuParts::LangParam MenuParts::s_langParam[5] = {
  { "Menu/GMaruGoPro-M", 1.105f, 0.5f, 0.f, 0 },
  { "Menu/SE-GARRIR", 1.105f, 0.8013f, 3.5f, 76 },
  { "Menu/FOTK-YDHopeL", 1.105f, 1.f, 0.f, 76 },
  { "Menu/FZHTK_TL", 1.105f, 0.895f, 0.f, 72 },
  { "Menu/FZHTK_SL", 1.105f, 0.895f, 0.f, 72 },
};
MenuParts::TexInfo MenuParts::l_texInfo[2] = { MenuParts::TexInfo("MenuTexture"), MenuParts::TexInfo("Button") };

MenuParts::PartsInfo MenuParts::s_info[15] = {
  PartsInfo(0, 0, 0, 32, 48),   PartsInfo(0, 0, 352, 32, 48), PartsInfo(0, 0, 768, 48, 96), PartsInfo(0, 0, 576, 32, 48), PartsInfo(0, 0, 480, 8, 12),
  PartsInfo(0, 0, 640, 16, 18), PartsInfo(1, 192, 64, 32, 48), PartsInfo(1, 0, 0, 32, 16),   PartsInfo(1, 192, 0, 48, 64),  PartsInfo(1, 352, 104, 16, 8),
  PartsInfo(0, 96, 640, 48, 64), PartsInfo(0, 576, 640, 16, 16), PartsInfo(1, 192, 192, 32, 48), PartsInfo(0, 0, 656, 16, 18), PartsInfo(0, 448, 608, 32, 32),
};

Shader *MenuParts::l_fontShader, *MenuParts::l_grayShader, *MenuParts::l_reverseShader, *MenuParts::l_pictureShader, *MenuParts::l_partsShader;
Font* MenuParts::l_unityFont = nullptr;
RaycastHit MenuParts::l_hit;
std::vector<MeshList> MenuParts::l_frameData;
Color MenuParts::s_colorList[7];                    // filled in SystemInit (Color::white etc. are dynamically initialised in another TU)
Material* MenuParts::l_colorFontList[7];
std::shared_ptr<MenuParts::IBase> MenuParts::IBase::s_dummy = std::make_shared<MenuParts::IBase>();

void MenuParts::LangParam::Apply() const {
  FONT_NAME = fontName; BIAS_FONT_SIZE = size; BIAS_FONT_LINE = line; BIAS_FONT_Y = dif_y; NAME_BASE_SCALE = (float)nameScale;
}

// ------------------------------------------------------------------------------------------------ TexInfo / PartsInfo
void MenuParts::TexInfo::Init(Shader* shader) {
  m_material = new Material(shader);
  m_material->mainTexture = Resources::Load<Texture>("Menu/" + m_file);
  if (!m_material->mainTexture) l_error("MenuParts: Resources.Load(Menu/%s) failed", m_file.c_str());
  Object::DontDestroyOnLoad(m_material);
  if (m_material->mainTexture) Object::DontDestroyOnLoad(m_material->mainTexture);
}

void MenuParts::PartsInfo::Init(Material* material) {
  Texture2D* t = dynamic_cast<Texture2D*>(material->mainTexture);
  int width = t ? t->width : 1024;                    // (fallback only when the texture is missing: keeps the divisions below finite)
  m_material = material;
  m_uvsize = (float)m_pixelSize / (float)width;
  m_startV = (float)m_startY / (float)width;
  m_widthNum = width / m_pixelSize;
  m_startNo = m_startX / m_pixelSize;
}

Material* MenuParts::PartsInfo::GetMaterial(bool bClone) { return bClone ? new Material(*m_material) : m_material; }

void MenuParts::PartsInfo::SetUV(Vector2* dst, int no, int offset) {
  no += m_startNo;
  float new_x2 = m_uvsize * (float)(no % m_widthNum);
  float new_x = m_uvsize + new_x2;
  float num2 = m_uvsize * (float)(no / m_widthNum) + m_startV;
  float num = m_uvsize + num2;
  dst[offset++].Set(new_x2, 0.f - num2);
  dst[offset++].Set(new_x, 0.f - num2);
  dst[offset++].Set(new_x, 0.f - num);
  dst[offset++].Set(new_x2, 0.f - num);
}

void MenuParts::PartsInfo::SetUV(Vector2* dst, int no, int noWidth, int noHeight, int offset) {
  no += m_startNo;
  float new_x2 = m_uvsize * (float)(no % m_widthNum);
  float new_x = m_uvsize * (float)noWidth + new_x2;
  float num2 = m_uvsize * (float)(no / m_widthNum) + m_startV;
  float num = m_uvsize * (float)noHeight + num2;
  dst[offset++].Set(new_x2, 0.f - num2);
  dst[offset++].Set(new_x, 0.f - num2);
  dst[offset++].Set(new_x, 0.f - num);
  dst[offset++].Set(new_x2, 0.f - num);
}

void MenuParts::PartsInfo::AddParts(MeshMaker* maker, Vector3 pos, int no, bool bInBounds) {
  maker->SetVertex(pos, (float)m_dps, (float)m_dps);
  SetUV(MeshMaker::s_uv, no);
  maker->Apply(bInBounds);
}
void MenuParts::PartsInfo::AddParts(MeshMaker* maker, Vector3 pos, int no, bool bInBounds, float size) {
  size *= (float)m_dps;
  maker->SetVertex(pos, size, size);
  SetUV(MeshMaker::s_uv, no);
  maker->Apply(bInBounds);
}
void MenuParts::PartsInfo::AddParts(MeshMaker* maker, Vector3 pos, int no, bool bInBounds, float sizeW, float sizeH) {
  maker->SetVertex(pos, sizeW, sizeH);
  SetUV(MeshMaker::s_uv, no);
  maker->Apply(bInBounds);
}
void MenuParts::PartsInfo::AddContiguousParts(MeshMaker* maker, Vector3 pos, int no, int noWidth, int noHeight, bool bInBounds, float size) {
  size *= (float)m_dps;
  maker->SetVertex(pos, size * (float)noWidth, size * (float)noHeight);
  SetUV(MeshMaker::s_uv, no, noWidth, noHeight);
  maker->Apply(bInBounds);
}
void MenuParts::PartsInfo::AddSquare(MeshMaker* maker, Vector3 pos, int no, bool bInBounds, float sizeW, float sizeH) {
  AddParts(maker, pos, no, bInBounds);
  AddParts(maker, MeshMaker::s_vtx[1], no + 1, bInBounds, sizeW, (float)m_dps);
  AddParts(maker, MeshMaker::s_vtx[1], no + 2, bInBounds);
  no += m_widthNum;
  pos.z = MeshMaker::s_vtx[2].z;
  AddParts(maker, pos, no, bInBounds, (float)m_dps, sizeH);
  AddParts(maker, MeshMaker::s_vtx[1], no + 1, bInBounds, sizeW, sizeH);
  AddParts(maker, MeshMaker::s_vtx[1], no + 2, bInBounds, (float)m_dps, sizeH);
  no += m_widthNum;
  pos.z = MeshMaker::s_vtx[2].z;
  AddParts(maker, pos, no, bInBounds);
  AddParts(maker, MeshMaker::s_vtx[1], no + 1, bInBounds, sizeW, (float)m_dps);
  AddParts(maker, MeshMaker::s_vtx[1], no + 2, bInBounds);
}
void MenuParts::PartsInfo::AddBar(MeshMaker* maker, Vector3 pos, int no, bool bInBounds, float sizeW) {
  AddParts(maker, pos, no, bInBounds);
  AddParts(maker, MeshMaker::s_vtx[1], no + 1, bInBounds, sizeW, (float)m_dps);
  AddParts(maker, MeshMaker::s_vtx[1], no + 2, bInBounds);
}
void MenuParts::PartsInfo::SetPartsOffset(Material* m, int no) {
  float x = m_uvsize * (float)(no % m_widthNum);
  float num = m_uvsize * (float)(no / m_widthNum) + m_startV;
  m->SetTextureOffset("_MainTex", Vector2(x, 0.f - num));
}

// ------------------------------------------------------------------------------------------------ IBase and friends
MenuParts::IBase::IBase() {
  m_intChanger = [](int) { Debug::LogError("Unimplemented. int"); };
  m_stringChanger = [](const std::string&) { Debug::LogError("Unimplemented. string"); };
  m_bEnable = true;
}

MenuParts::IButton::IButton(MenuParts* parts, Material* material, Color baseColor) {
  m_shaderMats.assign(1, material);
  material->color = (m_baseColor = baseColor);
  m_intChanger = [this, parts](int value) {
    bool flag = false;
    int textColor = 0;
    Shader* shader = l_pictureShader;
    bool flag2 = false;
    switch (value) {
      default:
        m_bEnable = true;
        if ((m_flag & 2) != 0) { flag2 = true; shader = l_reverseShader; textColor = 4; }
        break;
      case 1:
        m_bEnable = true;
        if ((m_flag & 2) != 0) { shader = l_reverseShader; textColor = 4; }
        else flag2 = true;
        break;
      case 2:
        m_bEnable = false;
        flag = true;
        break;
      case 3:
        m_bEnable = false;
        shader = l_grayShader;
        textColor = 3;
        break;
      case 4:
        m_bEnable = false;
        shader = l_reverseShader;
        flag2 = true;
        textColor = 4;
        break;
      case 5:
        m_bEnable = true;
        shader = l_reverseShader;
        flag2 = true;
        textColor = 4;
        m_flag |= 2;
        break;
      case 6:
        m_bEnable = true;
        m_flag &= -3;
        break;
    }
    if ((m_flag & 1) != 0) parts->SetTextColor(textColor);
    Color color = !flag ? m_baseColor : Color::clear;
    for (Material* material2 : m_shaderMats) {
      material2->shader = shader;
      material2->color = color;
    }
    static const Vector2 s_pushedUV(0.1875f, 0.f);
    m_shaderMats[0]->SetTextureOffset("_MainTex", !flag2 ? Vector2::zero : s_pushedUV);
  };
}

MenuParts::IButton::IButton(Material* material) {
  m_intChanger = [this, material](int i) {
    float num;
    if (i < 0) { num = 0.f; m_bEnable = false; }
    else { m_bEnable = true; num = (i != 0) ? 0.75f : 1.f; }
    material->color = Color(num, num, num, num);
  };
}

MenuParts::IButton::IButton() {
  m_intChanger = [](int) {};
  m_stringChanger = [](const std::string&) {};
}

MenuParts::IIcon::IIcon(MenuParts* parts, int partsNo, int vertexIndex) {
  m_intChanger = [parts, partsNo, vertexIndex](int value) {
    MeshFilter* component = parts->GetComponent<MeshFilter>();
    Mesh* mesh = OwnMesh(component);
    s_info[partsNo].SetUV(mesh->uv.data(), value, vertexIndex);
  };
}

MenuParts::IText::IText(TextMesh* textMesh) {
  m_stringChanger = [textMesh](const std::string& text) { textMesh->text = text; };
  m_intChanger = [textMesh](int value) { textMesh->renderer()->setSharedMaterial(l_colorFontList[value]); };
}

MenuParts::IText::IText(TextMesh* textMesh, Transform* transform, float fsize) {
  if (APPLY_HERO == 1) {
    APPLY_HERO = 0;
    m_stringChanger = [this, textMesh, transform, fsize](const std::string& text2) {
      switch (m_sprm & 0xF000) {
        case 4096:
          transform->localScale = Vector3(NAME_BASE_SCALE / (float)(m_sprm & 0xFFF), 1.f, 1.f);
          textMesh->characterSize = fsize;
          break;
        case 8192: transform->localScale = Vector3::one; textMesh->characterSize = 8.f; break;
        case 12288: transform->localScale = Vector3::one; textMesh->characterSize = 9.f; break;
        case 16384: transform->localScale = Vector3::one; textMesh->characterSize = 6.25f; break;
        default: transform->localScale = Vector3::one; textMesh->characterSize = fsize; break;
      }
      textMesh->text = text2;
    };
  } else {
    m_stringChanger = [textMesh](const std::string& text2) { textMesh->text = text2; };
  }
  m_intChanger = [textMesh](int value) { textMesh->renderer()->setSharedMaterial(l_colorFontList[value]); };
}

MenuParts::ITextColorChanger::ITextColorChanger(MenuParts* parts) {
  m_intChanger = [parts](int color) { parts->SetTextColor(color); };
}

static bool FrameOk(int value) {
  if (value >= 0 && value < (int)MenuParts::l_frameData.size()) return true;
  l_error("MenuParts: frame %d out of range (frameData has %d)", value, (int)MenuParts::l_frameData.size());
  return false;
}

MenuParts::IFrame::IFrame(bool bDynamicMesh, MeshFilter* meshFilter, MeshCollider* collider) {
  if (bDynamicMesh) {
    MeshList::SetDynamicMesh(OwnMesh(meshFilter));
    m_intChanger = [meshFilter, collider](int value) { if (FrameOk(value)) l_frameData[value].BuildDynamicMesh(meshFilter, collider); };
    return;
  }
  m_intChanger = [meshFilter, collider](int value) {
    if (!FrameOk(value)) return;
    Mesh* mesh = l_frameData[value].Build(meshFilter);
    collider->sharedMesh = mesh;
  };
}

MenuParts::IFrame::IFrame(bool bDynamicMesh, MeshFilter* meshFilter) {
  if (bDynamicMesh) {
    MeshList::SetDynamicMesh(OwnMesh(meshFilter));
    m_intChanger = [meshFilter](int value) { if (FrameOk(value)) l_frameData[value].BuildDynamicMesh(meshFilter); };
  } else {
    m_intChanger = [meshFilter](int value) { if (FrameOk(value)) l_frameData[value].Build(meshFilter); };
  }
}

MenuParts::IBar::IBar(MenuParts* menuParts, int vtxIndex) : m_parts(menuParts), m_vtxIndex(vtxIndex) {
  m_intChanger = [this](int value) { firstFunc(value); };
}

void MenuParts::IBar::normalFunc(int value) {
  Mesh* mesh = OwnMesh(m_parts->GetComponent<MeshFilter>());
  int num;
  if (value < 0) { value = -(value + 1); num = m_vtxIndex - 4; }
  else num = m_vtxIndex;
  float x = m_zero + m_width * (float)value * 0.01f;
  mesh->vertices[num + 1].x = x;
  mesh->vertices[num + 2].x = x;
}

void MenuParts::IBar::firstFunc(int value) {
  Mesh* mesh = OwnMesh(m_parts->GetComponent<MeshFilter>());
  m_zero = mesh->vertices[m_vtxIndex].x;
  m_width = mesh->vertices[m_vtxIndex + 1].x - m_zero;
  m_intChanger = [this](int v) { normalFunc(v); };      // C#: (m_intChanger = normalFunc)(value)
  normalFunc(value);
}

MenuParts::ITexture::ITexture(Material* material) {
  material->color = Color::clear;
  m_intChanger = [material](int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Menu/face/f%02d", value);
    material->mainTexture = Resources::Load<Texture>(buf);
    material->color = material->mainTexture ? Color::white : Color::clear;
  };
}

// ------------------------------------------------------------------------------------------------ MenuParts
void MenuParts::SystemInit() {
  eIndent::Init();                                            // (C#: TouchPad.Awake does it; idempotent and needed before any UI is built)
  s_colorList[0] = Color::white; s_colorList[1] = Color::yellow; s_colorList[2] = Color::red; s_colorList[3] = Color::gray;
  s_colorList[4] = Color::black; s_colorList[5] = Color::cyan; s_colorList[6] = Color(0.f, 1.f, 0.f, 1.f);
  s_langParam[GetLangages::m_langagesNumber].Apply();
  l_partsShader = Shader::Find("Custom/MenuParts");
  l_fontShader = Shader::Find("Custom/FontAlpha8");
  l_pictureShader = Shader::Find("Custom/Picture");
  l_grayShader = Shader::Find("Custom/ButtonGray");
  l_reverseShader = Shader::Find("Custom/ButtonReverse");
  l_unityFont = Resources::Load<Font>(FONT_NAME);
  if (!l_unityFont) l_error("MenuParts::SystemInit: font %s not available (Resources.Load<Font>)", FONT_NAME.c_str());
  Texture* fontTex = !l_unityFont ? nullptr : (l_unityFont->material && l_unityFont->material->mainTexture) ? l_unityFont->material->mainTexture : l_unityFont->texture;
  for (int i = 0; i < 7; i++) {
    l_colorFontList[i] = new Material(l_fontShader);
    l_colorFontList[i]->mainTexture = fontTex;
    l_colorFontList[i]->color = s_colorList[i];
    Object::DontDestroyOnLoad(l_colorFontList[i]);
  }
  TextAsset* textAsset = Resources::Load<TextAsset>("Menu/frameData");
  if (textAsset) {
    l_frameData = MeshList::Load(textAsset->bytes);
    Resources::UnloadAsset(textAsset);
  } else l_error("MenuParts::SystemInit: Menu/frameData not available");
  if (l_unityFont) Object::DontDestroyOnLoad(l_unityFont);
  Object::DontDestroyOnLoad(l_partsShader);
  Object::DontDestroyOnLoad(l_fontShader);
  Object::DontDestroyOnLoad(l_grayShader);
  Object::DontDestroyOnLoad(l_reverseShader);
  Object::DontDestroyOnLoad(l_pictureShader);
  l_texInfo[0].Init(l_partsShader);
  l_texInfo[1].Init(l_pictureShader);
  for (PartsInfo& partsInfo : s_info) partsInfo.Init(l_texInfo[partsInfo.m_texNo].m_material);
  l_info("MenuParts::SystemInit: lang %d font '%s' (%s), %d frames, MenuTexture %p", GetLangages::m_langagesNumber, FONT_NAME.c_str(), l_unityFont ? "loaded" : "MISSING",
         (int)l_frameData.size(), (void*)l_texInfo[0].m_material->mainTexture);
}

void MenuParts::SetRiseupMaterial(Material* m, int partsNo, int no) {
  m->mainTexture = s_info[partsNo].GetTexture();
  s_info[partsNo].SetPartsOffset(m, no);
}

void MenuParts::SetTextColor(Renderer* a, int color) { a->setSharedMaterial(l_colorFontList[color]); }

void MenuParts::SetTextColor(int color) {
  if (m_textColorTargetBits == 0) return;
  for (int i = 0, count = (int)m_list.size(); i < count; i++) {
    if ((m_textColorTargetBits & (uint32_t)(1 << i)) != 0) m_list[i]->m_intChanger(color);
  }
}

bool MenuParts::defaultIsHit(Ray& ray) {
  Collider* c = collider();
  return m_button->m_bEnable && c != nullptr && c->Raycast(ray, &l_hit, FLT_MAX);
}

void MenuParts::Awake() {
  m_button = nullptr;
  m_buttonId = -1;
  m_isHit = [this](Ray& ray) { return defaultIsHit(ray); };
  m_gamepad = 0;
}

void MenuParts::OnDestroy() {
  // frame parts get their mesh lazily (IFrame -> filter.mesh) and it is not in m_uol: free the filter's instance mesh too
  if (MeshFilter* mf = gameObject->GetComponent<MeshFilter>())
    if (mf->sharedMesh && mf->meshIsInstance) Object::Destroy(mf->sharedMesh);
  m_uol.Destroy();
}

void MenuParts::SetVisible(bool bShow) {
  if (renderer() != nullptr) renderer()->enabled = bShow;
  std::vector<Renderer*> componentsInChildren;
  gameObject->GetComponentsInChildren<Renderer>(componentsInChildren);
  for (Renderer* r : componentsInChildren) r->enabled = bShow;
}

void MenuParts::SetPush(bool bPushed) {
  if (m_button != nullptr) m_button->Change(bPushed ? 1 : 0);
}

void MenuParts::SetTouchScreenButton(short buttonId, short gamePad) {
  m_isHit = [](Ray&) { return true; };
  m_buttonId = buttonId;
  m_gamepad = gamePad;
  m_se = (gamePad & 0xA10) != 0;
}

void MenuParts::ChangeParts(int partsNo, bool bCloneMaterial, bool bForceDivTriangles) {
  if (s_info[m_nowParts].m_texNo != s_info[partsNo].m_texNo || bCloneMaterial || bForceDivTriangles) {
    m_maker->DivTriangles(s_info[partsNo].GetMaterial(bCloneMaterial));
    if (bCloneMaterial) { m_uol.Add(m_maker->GetNowMaterial()); }
  }
  m_nowParts = partsNo;
}

Vector3 MenuParts::CalcPartsCenterPosition(int indent, float dx, float dy) {
  Vector3 result = m_maker->CalcBoundsPosition(indent, 0.f, 0.f, 0.f);
  dx *= (float)s_info[m_nowParts].m_dps;
  dy *= (float)s_info[m_nowParts].m_dps;
  switch (indent) {
    case 1: result = result + Vector3((0.f - dx) * 0.5f, 0.f, 0.f); break;
    case 2: result = result + Vector3(0.f - dx, 0.f, 0.f); break;
    case 3: result = result + Vector3(0.f, 0.f, dy * 0.5f); break;
    case 4: result = result + Vector3((0.f - dx) * 0.5f, 0.f, dy * 0.5f); break;
    case 5: result = result + Vector3(0.f - dx, 0.f, dy * 0.5f); break;
    case 6: result = result + Vector3(0.f, 0.f, dy); break;
    case 7: result = result + Vector3((0.f - dx) * 0.5f, 0.f, dy); break;
    case 8: result = result + Vector3(0.f - dx, 0.f, dy); break;
  }
  return result;
}

Vector3 MenuParts::CalcPartsPosition(int indent, float x, float y, float z, float dx, float dy) {
  Vector3 result = CalcPartsCenterPosition(indent, dx, dy);
  result.x += x; result.y += y; result.z += z;
  return result;
}

void MenuParts::AddIcon(Vector3 pos, int no, bool bInBounds) {
  m_list.push_back(std::make_shared<IIcon>(this, m_nowParts, m_maker->GetNowVertexIndex()));
  s_info[m_nowParts].AddParts(m_maker, pos, no, bInBounds);
}
void MenuParts::AddIcon(int indent, int no, bool bInBounds) {
  m_list.push_back(std::make_shared<IIcon>(this, m_nowParts, m_maker->GetNowVertexIndex()));
  s_info[m_nowParts].AddParts(m_maker, CalcPartsCenterPosition(indent), no, bInBounds);
}
void MenuParts::AddIcon(Vector3 pos, int no, bool bInBounds, float size) {
  m_list.push_back(std::make_shared<IIcon>(this, m_nowParts, m_maker->GetNowVertexIndex()));
  s_info[m_nowParts].AddParts(m_maker, pos, no, bInBounds, size);
}
void MenuParts::AddIcon(int indent, int no, bool bInBounds, float size) {
  m_list.push_back(std::make_shared<IIcon>(this, m_nowParts, m_maker->GetNowVertexIndex()));
  s_info[m_nowParts].AddParts(m_maker, CalcPartsCenterPosition(indent, size, size), no, bInBounds, size);
}
void MenuParts::AddIcon(Vector3 pos, int no, bool bInBounds, float sizeW, float sizeH) {
  m_list.push_back(std::make_shared<IIcon>(this, m_nowParts, m_maker->GetNowVertexIndex()));
  s_info[m_nowParts].AddParts(m_maker, pos, no, bInBounds, sizeW, sizeH);
}
void MenuParts::AddIcon(int indent, int no, bool bInBounds, float sizeW, float sizeH) {
  m_list.push_back(std::make_shared<IIcon>(this, m_nowParts, m_maker->GetNowVertexIndex()));
  s_info[m_nowParts].AddParts(m_maker, CalcPartsCenterPosition(indent, sizeW, sizeH), no, bInBounds, sizeW, sizeH);
}
void MenuParts::AddIcon(int indent, float x, float y, float z, int no, bool bInBounds, float size) {
  Vector3 pos = CalcPartsPosition(indent, x, y, z, size, size);
  m_list.push_back(std::make_shared<IIcon>(this, m_nowParts, m_maker->GetNowVertexIndex()));
  s_info[m_nowParts].AddParts(m_maker, pos, no, bInBounds, size);
}
void MenuParts::AddContiguousIcon(int indent, float x, float y, float z, int no, int noWidth, int noHeight, bool bInBounds, float size) {
  Vector3 pos = CalcPartsPosition(indent, x, y, z, (float)noWidth * size, (float)noHeight * size);
  m_list.push_back(IBase::s_dummy);
  s_info[m_nowParts].AddContiguousParts(m_maker, pos, no, noWidth, noHeight, bInBounds, size);
}
void MenuParts::AddSquare(Vector3 pos, int no, bool bInBounds, float sizeW, float sizeH) {
  m_list.push_back(IBase::s_dummy);
  s_info[m_nowParts].AddSquare(m_maker, pos, no, bInBounds, sizeW, sizeH);
}

void MenuParts::SetButtonIcon(short buttonId, short gamepad) {
  m_buttonId = buttonId; m_gamepad = gamepad; m_se = (gamepad & 0xA10) != 0;
  m_button = std::make_shared<IButton>(m_maker->GetNowMaterial());
  m_list.push_back(m_button);
}
void MenuParts::SetButton(short buttonId, short gamepad) {
  m_buttonId = buttonId; m_gamepad = gamepad; m_se = (gamepad & 0xA10) != 0;
  m_button = std::make_shared<IButton>();
  m_list.push_back(m_button);
}
void MenuParts::SetButtonColliderOut(short buttonId, short gamepad) {
  m_buttonId = buttonId; m_gamepad = gamepad; m_se = (gamepad & 0xA10) != 0;
  m_button = std::make_shared<IButton>();
  m_isHit = [this](Ray& ray) {
    Collider* c = collider();
    return m_button->m_bEnable && !(c && c->Raycast(ray, &l_hit, FLT_MAX));
  };
}
void MenuParts::AddButton(Vector3 pos, float sizeW, float sizeH, short buttonId, short gamepad, short buttonType, Color color) {
  m_buttonId = buttonId; m_gamepad = gamepad; m_se = (gamepad & 0xA10) != 0;
  m_button = std::make_shared<IButton>(this, m_maker->GetNowMaterial(), color);
  m_list.push_back(m_button);
  s_info[m_nowParts].AddSquare(m_maker, pos, buttonType, true, sizeW, sizeH);
}
void MenuParts::AddButtonIcon(Vector3 pos, int no, float size, short buttonId, short gamepad) {
  m_buttonId = buttonId; m_gamepad = gamepad; m_se = (gamepad & 0xA10) != 0;
  m_button = std::make_shared<IButton>(m_maker->GetNowMaterial());
  m_list.push_back(m_button);
  s_info[m_nowParts].AddParts(m_maker, pos, no, true, size);
}

// `new GameObject().AddComponent<TextMesh>()` + the MeshRenderer Unity adds automatically (RequireComponent) -- ours may or may not add it.
MeshRenderer* MenuParts::EnsureMeshRenderer(GameObject* go) {
  MeshRenderer* mr = go->GetComponent<MeshRenderer>();
  return mr ? mr : go->AddComponent<MeshRenderer>();
}

GameObject* MenuParts::AddTextMesh(Vector3 pos, float size, TextAlignment alignemt, TextAnchor anchor, float lineSpace) {
  GameObject* go = new GameObject();
  TextMesh* textMesh = go->AddComponent<TextMesh>();
  MeshRenderer* component = EnsureMeshRenderer(go);
  textMesh->font = l_unityFont;
  textMesh->alignment = (int)alignemt;
  textMesh->anchor = (int)anchor;
  textMesh->lineSpacing = lineSpace * BIAS_FONT_LINE;
  textMesh->characterSize = size * BIAS_FONT_SIZE;
  textMesh->text = "";
  pos.z += BIAS_FONT_Y;
  component->setSharedMaterial(l_colorFontList[0]);
  component->enabled = false;
  m_uol.Adds(component, textMesh, go);
  m_textColorTargetBits |= (uint32_t)(1 << m_list.size());
  m_list.push_back((APPLY_HERO == 0) ? std::make_shared<IText>(textMesh) : std::make_shared<IText>(textMesh, go->transform, size * BIAS_FONT_SIZE));
  go->transform->SetParent(transform());
  go->transform->localPosition = pos;
  go->transform->Rotate(90.f, 0.f, 0.f);
  go->layer = 8;
  go->name = "Text";
  return go;
}

GameObject* MenuParts::AddTextMesh(int indent, float x, float y, float z, float size, TextAlignment alignemt, TextAnchor anchor, float lineSpace) {
  GameObject* go = new GameObject();
  TextMesh* textMesh = go->AddComponent<TextMesh>();
  MeshRenderer* component = EnsureMeshRenderer(go);
  textMesh->font = l_unityFont;
  textMesh->alignment = (int)alignemt;
  textMesh->anchor = (int)anchor;
  textMesh->lineSpacing = lineSpace * BIAS_FONT_LINE;
  textMesh->characterSize = size * BIAS_FONT_SIZE;
  textMesh->text = "";
  z += BIAS_FONT_Y;
  component->setSharedMaterial(l_colorFontList[0]);
  component->enabled = false;
  m_uol.Adds(component, textMesh, go);
  m_textColorTargetBits |= (uint32_t)(1 << m_list.size());
  m_list.push_back((APPLY_HERO == 0) ? std::make_shared<IText>(textMesh) : std::make_shared<IText>(textMesh, go->transform, size * BIAS_FONT_SIZE));
  go->transform->SetParent(transform());
  go->transform->setPosition(eIndent::CalcScreenPosition(indent, x, y, z));
  go->transform->Rotate(90.f, 0.f, 0.f);
  go->layer = 8;
  go->name = "Text";
  return go;
}

static void AddButtonCollider(GameObject* gameObject) {
  Bounds bounds = OwnMesh(gameObject->GetComponent<MeshFilter>())->bounds;
  BoxCollider* boxCollider = gameObject->AddComponent<BoxCollider>();
  Vector3 size = bounds.size();
  size.y = 1.f;
  boxCollider->size = size;
  boxCollider->center = bounds.center;
}

void MenuParts::Build(int indent, Vector3 pos, Transform* parent) {
  m_maker->Build(m_uol, gameObject, indent);
  delete m_maker; m_maker = nullptr;
  if (parent != nullptr) {
    transform()->SetParent(parent);
    transform()->localPosition = pos;
  } else {
    transform()->setPosition(pos);
  }
  if (m_button != nullptr) AddButtonCollider(gameObject);
}

void MenuParts::Build(int indent, int screenIndent, float x, float y, float z, Transform* parent) {
  Vector3 position = eIndent::CalcScreenPosition(screenIndent, x, y, z);
  m_maker->Build(m_uol, gameObject, indent);
  delete m_maker; m_maker = nullptr;
  if (parent != nullptr) position.y += parent->position().y;
  transform()->setPosition(position);
  transform()->SetParent(parent);
  if (m_button != nullptr) AddButtonCollider(gameObject);
}

std::shared_ptr<MenuParts::IBar> MenuParts::AddBar(int no, int barNo, int backNo, float x, float y, float z, float w, float adjustX) {
  int num = m_maker->GetNowVertexIndex();
  Vector3 vector(x, y, z);
  s_info[m_nowParts].AddBar(m_maker, vector, no, true, w);
  w += (float)(s_info[m_nowParts].m_dps * 2) - adjustX;
  vector.x += adjustX * 0.5f;
  vector.y -= 0.5f;
  m_maker->SetVertex(vector, w, (float)s_info[m_nowParts].m_dps);
  s_info[m_nowParts].SetUV(MeshMaker::s_uv, barNo);
  m_maker->Apply(false, num);
  if (backNo >= 0) {
    vector.y -= 0.5f;
    m_maker->SetVertex(vector, w, (float)s_info[m_nowParts].m_dps);
    s_info[m_nowParts].SetUV(MeshMaker::s_uv, backNo);
    m_maker->Apply(false, num);
    num += 4;
  }
  auto result = std::make_shared<IBar>(this, num);
  m_list.push_back(result);
  return result;
}

void MenuParts::VariableWindow(int no, float sizeW, float sizeH, int indent) {
  m_maker->Clear();
  s_info[m_nowParts].AddSquare(m_maker, Vector3::zero, no, true, sizeW, sizeH);
  m_maker->ReBuild(gameObject, indent);
}

MeshMaker* MenuParts::NewMaker(int partsNo, bool bClone) {
  MeshMaker* mk = new MeshMaker(s_info[partsNo].GetMaterial(bClone));
  if (bClone) mk->m_owned.push_back(mk->m_materials[0]);
  return mk;
}

MenuParts* MenuParts::CreateIconTexture(int indent, float x, float y, float z, float size) {
  MenuParts* menuParts = (new GameObject())->AddComponent<MenuParts>();
  Material* material = new Material(l_partsShader);
  menuParts->m_list.push_back(std::make_shared<ITexture>(material));
  menuParts->m_uol.Adds(menuParts, material);
  menuParts->m_maker = new MeshMaker(material);
  menuParts->m_maker->SetVertex(menuParts->CalcPartsPosition(indent, x, y, z, size, size), size, size);
  menuParts->m_maker->SetUV(0.f, 0.f, 1.f, 1.f);
  menuParts->m_maker->Apply(true);
  return menuParts;
}

void MenuParts::AddChangeIconTexture(int indent, float x, float y, float z, float size, bool bInBounds) {
  Material* material = new Material(l_partsShader);
  m_list.push_back(std::make_shared<ITexture>(material));
  m_maker->DivTriangles(material);
  m_maker->m_owned.push_back(material);
  m_maker->SetVertex(CalcPartsPosition(indent, x, y, z), size, size);
  m_maker->SetUV(0.f, 0.f, 1.f, 1.f);
  m_maker->Apply(bInBounds);
}

MenuParts* MenuParts::CreateBuildVariableWindow(int no, float sizeW, float sizeH, int indent, bool bCloneMaterial) {
  MenuParts* menuParts = (new GameObject())->AddComponent<MenuParts>();
  menuParts->m_maker = NewMaker(menuParts->m_nowParts = 4, bCloneMaterial);
  menuParts->AddSquare(Vector3::zero, no, true, sizeW, sizeH);
  menuParts->m_maker->Build(menuParts->m_uol, menuParts->gameObject, indent);
  return menuParts;
}

MenuParts* MenuParts::CreateWindow0(float width, float height) {
  MenuParts* menuParts = (new GameObject())->AddComponent<MenuParts>();
  menuParts->m_maker = new MeshMaker(s_info[menuParts->m_nowParts = 4].GetMaterial(false));
  menuParts->AddSquare(Vector3::zero, 0, true, width, height);
  return menuParts;
}

MenuParts* MenuParts::CreateButton(float width, float height, short buttonId, short gamepad) { return CreateButton(width, height, buttonId, gamepad, 0, Color::white); }

MenuParts* MenuParts::CreateButton(float width, float height, short buttonId, short gamepad, short buttonType, Color color) {
  MenuParts* menuParts = (new GameObject())->AddComponent<MenuParts>();
  menuParts->m_maker = NewMaker(menuParts->m_nowParts = 7, true);
  menuParts->AddButton(Vector3::zero, width, height, buttonId, gamepad, buttonType, color);
  return menuParts;
}

MenuParts* MenuParts::CreateButtonIcon(int partsNo, int no, float size, short buttonId, short gamepad) {
  MenuParts* menuParts = (new GameObject())->AddComponent<MenuParts>();
  menuParts->m_maker = NewMaker(menuParts->m_nowParts = partsNo, true);
  menuParts->AddButtonIcon(Vector3::zero, no, size, buttonId, gamepad);
  return menuParts;
}

MenuParts* MenuParts::CreateBuildTextMenu(int indent, float x, float y, float z, Transform* parent, float size, TextAlignment alignemt, TextAnchor anchor, float lineSpace) {
  Vector3 position = eIndent::CalcScreenPosition(indent, x, y, z + BIAS_FONT_Y);
  GameObject* gameObject = new GameObject();
  MenuParts* menuParts = gameObject->AddComponent<MenuParts>();
  TextMesh* textMesh = gameObject->AddComponent<TextMesh>();
  MeshRenderer* component = EnsureMeshRenderer(gameObject);
  textMesh->font = l_unityFont;
  textMesh->alignment = (int)alignemt;
  textMesh->anchor = (int)anchor;
  textMesh->lineSpacing = lineSpace * BIAS_FONT_LINE;
  textMesh->characterSize = size * BIAS_FONT_SIZE;
  textMesh->text = "";
  component->setSharedMaterial(l_colorFontList[0]);
  component->enabled = false;
  menuParts->m_uol.Adds(component, textMesh, gameObject);
  menuParts->m_textColorTargetBits |= (uint32_t)(1 << menuParts->m_list.size());
  menuParts->m_list.push_back((APPLY_HERO == 0) ? std::make_shared<IText>(textMesh) : std::make_shared<IText>(textMesh, gameObject->transform, size * BIAS_FONT_SIZE));
  if (parent != nullptr) position.y += parent->position().y;
  gameObject->transform->setPosition(position);
  gameObject->transform->SetParent(parent);
  gameObject->transform->Rotate(90.f, 0.f, 0.f);
  gameObject->layer = 8;
  return menuParts;
}

void MenuParts::InitFrameParts(MenuParts* val, GameObject* gameObject, int frameNo, int indent, float x, float y, float z, bool bUseCollider, bool bDynamicMesh) {
  MeshFilter* meshFilter = gameObject->AddComponent<MeshFilter>();
  if (bUseCollider) {
    MeshCollider* meshCollider = gameObject->AddComponent<MeshCollider>();
    val->m_uol.Adds(meshCollider, meshFilter, gameObject->AddComponent<MeshRenderer>(), gameObject);
    val->m_list.push_back(std::make_shared<IFrame>(bDynamicMesh, meshFilter, meshCollider));
  } else {
    val->m_uol.Adds(meshFilter, gameObject->AddComponent<MeshRenderer>(), gameObject);
    val->m_list.push_back(std::make_shared<IFrame>(bDynamicMesh, meshFilter));
  }
  gameObject->transform->setPosition(eIndent::CalcScreenPosition(indent, x, y, z));
  gameObject->layer = 8;
  gameObject->renderer()->setSharedMaterial(s_info[4].GetMaterial(false));
  gameObject->renderer()->enabled = false;
  if (frameNo >= 0) val->m_list[0]->Change(frameNo);
}

MenuParts* MenuParts::AttachMenuParts(GameObject* target, int indent, float x, float y, float z) {
  MenuParts* menuParts = target->AddComponent<MenuParts>();
  target->transform->setPosition(eIndent::CalcScreenPosition(indent, x, y, z));
  menuParts->m_uol.Add(target);
  return menuParts;
}

MenuParts* MenuParts::CreateFromTexture(const std::string& filename, bool bFitWidth, float haspect) {
  GameObject* gameObject = new GameObject();
  MenuParts* menuParts = gameObject->AddComponent<MenuParts>();
  Texture2D* texture2D = Resources::Load<Texture2D>(filename);
  Material* material = new Material(l_pictureShader);
  MeshMaker* meshMaker = new MeshMaker(material);
  float tw = texture2D ? (float)texture2D->width : 450.f, th = texture2D ? (float)texture2D->height : 450.f;
  float num = tw;
  float num2 = th * haspect;
  if (bFitWidth) {
    num = num * 450.f / tw;
    num2 = num2 * 450.f / tw;
  }
  material->mainTexture = texture2D;
  menuParts->m_maker = meshMaker;
  menuParts->m_uol.Adds(gameObject, material);
  meshMaker->SetVertex(Vector3::zero, num, num2);
  meshMaker->SetUV(0.f, 1.f - haspect, 1.f, haspect);
  meshMaker->Apply(true);
  return menuParts;
}

MenuParts* MenuParts::CreateFromTextureWindow(Texture2D* t, int window_w, int window_h) {
  GameObject* gameObject = new GameObject();
  MenuParts* menuParts = gameObject->AddComponent<MenuParts>();
  Material* material = new Material(l_pictureShader);
  MeshMaker* meshMaker = new MeshMaker(material);
  float sizeW = (float)window_w * 450.f / (float)window_w;
  float sizeH = (float)window_h * 450.f / (float)window_w;
  material->mainTexture = t;
  menuParts->m_maker = meshMaker;
  menuParts->m_uol.Adds(gameObject, material);
  meshMaker->SetVertex(Vector3::zero, sizeW, sizeH);
  meshMaker->SetUV(0.f, 0.f, 1.f, -1.f);
  meshMaker->Apply(true);
  return menuParts;
}

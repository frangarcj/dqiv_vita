// game/MenuParts.hpp -- OWNER: WP4a.  Port of MenuParts.cs: the UI primitive every screen is built from (a GameObject whose MeshFilter holds
// 9-slice frames / icon quads / button squares on the XZ plane, plus TextMeshes), its IBase "changers" (the setters the MenuPortItem tables bind
// to item ids) and the touch hit-test.   C# source: re/cs_out/Assembly-CSharp/MenuParts.cs
//
// C# -> C++ (see README conventions; additions):
//   MenuParts.IBase & subclasses: nested classes, held by std::shared_ptr (`IBasePtr`); delegates -> std::function.
//   `ref Ray` -> `Ray&`.  TextAlignment/TextAnchor: enum classes below (TextMesh stores them as int).
//   static class members keep their C# names (l_fontShader, s_info, l_unityFont, BIAS_FONT_SIZE...).
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "game/FRAME_ENUM.hpp"
#include "game/GetLangages.hpp"
#include "game/MeshMaker.hpp"
#include "game/MeshList.hpp"
#include "game/eIndent.hpp"
#include "miniunity.hpp"
using namespace miniunity;

enum class TextAlignment : int { Left = 0, Center = 1, Right = 2 };
enum class TextAnchor : int { UpperLeft = 0, UpperCenter = 1, UpperRight = 2, MiddleLeft = 3, MiddleCenter = 4, MiddleRight = 5, LowerLeft = 6, LowerCenter = 7, LowerRight = 8 };

class MenuParts : public Component {
public:
  // ---- font / language parameters ----
  struct LangParam {
    const char* fontName; float size, line, dif_y; int nameScale;
    void Apply() const;
  };

  // ---- texture + atlas tables ----
  struct TexInfo {
    std::string m_file; Material* m_material = nullptr;
    explicit TexInfo(const char* file) : m_file(file) {}
    void Init(Shader* shader);
  };

  struct PartsInfo {
    int m_texNo, m_startX, m_startY, m_pixelSize, m_dps;
    Material* m_material = nullptr;
    float m_uvsize = 0, m_startV = 0; int m_widthNum = 1, m_startNo = 0;
    PartsInfo(int texNo, int startX, int startY, int pixelSize, int dps) : m_texNo(texNo), m_startX(startX), m_startY(startY), m_pixelSize(pixelSize), m_dps(dps) {}
    void Init(Material* material);
    Texture2D* GetTexture() { return dynamic_cast<Texture2D*>(m_material->mainTexture); }
    Material* GetMaterial(bool bClone);
    void SetUV(Vector2* dst, int no, int offset = 0);
    void SetUV(Vector2* dst, int no, int noWidth, int noHeight, int offset = 0);
    void AddParts(MeshMaker* maker, Vector3 pos, int no, bool bInBounds);
    void AddParts(MeshMaker* maker, Vector3 pos, int no, bool bInBounds, float size);
    void AddParts(MeshMaker* maker, Vector3 pos, int no, bool bInBounds, float sizeW, float sizeH);
    void AddContiguousParts(MeshMaker* maker, Vector3 pos, int no, int noWidth, int noHeight, bool bInBounds, float size = 1.f);
    void AddSquare(MeshMaker* maker, Vector3 pos, int no, bool bInBounds, float sizeW, float sizeH);
    void AddBar(MeshMaker* maker, Vector3 pos, int no, bool bInBounds, float sizeW);
    Vector3 CalcSquareSize(float sizeW, float sizeH) { return Vector3(sizeW + (float)(m_dps * 2), 0.f, sizeH + (float)(m_dps * 2)); }
    void SetPartsOffset(Material* m, int no);
  };

  // ---- IBase: the "changers" bound to item ids ----
  class IBase {
  public:
    using IntChanger = std::function<void(int)>;
    using StringChanger = std::function<void(const std::string&)>;
    static std::shared_ptr<IBase> s_dummy;
    IntChanger m_intChanger; StringChanger m_stringChanger;
    bool m_bEnable = true; short m_sprm = 0;
    IBase();
    virtual ~IBase() {}
    void Change(int i) { m_intChanger(i); }
    void Change(const std::string& s) { m_stringChanger(s); }
  };
  using IBasePtr = std::shared_ptr<IBase>;

  class IButton : public IBase {
  public:
    static constexpr int c_textColorChange = 1, c_reversed = 2;
    int m_flag = 0;
    IButton(MenuParts* parts, Material* material, Color baseColor);
    explicit IButton(Material* material);
    IButton();
    void AddChangeGrayMaterials(const std::vector<Material*>& mats) { m_shaderMats.insert(m_shaderMats.end(), mats.begin(), mats.end()); }
  private:
    Color m_baseColor = Color::white;
    std::vector<Material*> m_shaderMats;
  };

  class IIcon : public IBase { public: IIcon(MenuParts* parts, int partsNo, int vertexIndex); };
  class IText : public IBase {
  public:
    explicit IText(TextMesh* textMesh);
    IText(TextMesh* textMesh, Transform* transform, float fsize);
  };
  class ITextColorChanger : public IBase { public: explicit ITextColorChanger(MenuParts* parts); };
  class IFrame : public IBase {
  public:
    IFrame(bool bDynamicMesh, MeshFilter* meshFilter, MeshCollider* collider);
    IFrame(bool bDynamicMesh, MeshFilter* meshFilter);
  };
  class IBar : public IBase {
  public:
    IBar(MenuParts* menuParts, int vtxIndex);
  private:
    MenuParts* m_parts; int m_vtxIndex; float m_zero = 0, m_width = 0; bool m_inited = false;
    void normalFunc(int value);
    void firstFunc(int value);
  };
  class ITexture : public IBase { public: explicit ITexture(Material* material); };

  using FuncIsHit = std::function<bool(Ray&)>;

  // ---- constants ----
  static constexpr int ICON_ITEM = 0, ICON_CHAR = 1, ICON_MENU = 3, PARTS_WINDOW0 = 4, ICON_CURSOR = 5, ICON_FONT = 6, PARTS_BUTTON = 7, ICON_TRIANGLE = 8, ICON_BTL_BAR = 9,
                       ICON_ALLOW = 10, ICON_HOPNUM = 11, ICON_ITEM_NUM = 12, ICON_STATUS = 13, ICON_SUGOROKU_ARROW = 14;
  static constexpr int TEXT_COLOR_WHITE = 0, TEXT_COLOR_YELLOW = 1, TEXT_COLOR_RED = 2, TEXT_COLOR_GRAY = 3, TEXT_COLOR_BLACK = 4, TEXT_COLOR_CYAN = 5, TEXT_COLOR_GREEN = 6;
  static constexpr int BUTTON_STATE_NORMAL = 0, BUTTON_STATE_PUSHED = 1, BUTTON_STATE_UNVISIBLE = 2, BUTTON_STATE_GRAY = 3, BUTTON_STATE_REVERSE = 4, BUTTON_STATE_REVERSE2 = 5, BUTTON_STATE_UNREVERSE = 6;

  // ---- statics ----
  static std::string FONT_NAME;
  static float BIAS_FONT_SIZE, BIAS_FONT_LINE, BIAS_FONT_Y, NAME_BASE_SCALE;
  static int APPLY_HERO;
  static PartsInfo s_info[15];
  static Shader *l_fontShader, *l_grayShader, *l_reverseShader, *l_pictureShader, *l_partsShader;
  static Font* l_unityFont;
  static Color s_colorList[7];
  static std::vector<MeshList> l_frameData;

  // ---- instance state ----
  int m_nowParts = 0;
  MeshMaker* m_maker = nullptr;
  FuncIsHit m_isHit;
  UnityOBJList m_uol;
  short m_buttonId = -1, m_gamepad = 0;
  bool m_se = false, m_externalToggle = false;
  uint32_t m_textColorTargetBits = 0;
  std::vector<IBasePtr> m_list;
  std::shared_ptr<IButton> m_button;

  ~MenuParts() override { delete m_maker; }

  static void SystemInit();
  static void SetRiseupMaterial(Material* m, int partsNo, int no);
  static void SetTextColor(Renderer* a, int color);
  void SetEnableButtonTextColorChange() { m_button->m_flag |= 1; }
  void SetTextColor(int color);
  bool defaultIsHit(Ray& ray);
  void Awake() override;
  void OnDestroy() override;
  void DontDestroyOnLoad() { m_uol.DontDestroyOnLoad(); }
  void SetVisible(bool bShow);
  void SetPush(bool bPushed);
  void SetTouchScreenButton(short buttonId, short gamePad);
  void ChangeParts(int partsNo, bool bCloneMaterial = false, bool bForceDivTriangles = false);
  Vector3 GetVertex(int index) { return MeshMaker::s_vtx[index]; }
  Vector3 CalcBoundsPosition(int indent, float x = 0.f, float y = 0.f, float z = 0.f) { return m_maker->CalcBoundsPosition(indent, x, y, z); }
  Vector3 CalcPartsCenterPosition(int indent, float dx = 1.f, float dy = 1.f);
  Vector3 CalcPartsPosition(int indent, float x, float y, float z, float dx = 1.f, float dy = 1.f);

  void AddIcon(Vector3 pos, int no, bool bInBounds);
  void AddIcon(int indent, int no, bool bInBounds);
  void AddIcon(Vector3 pos, int no, bool bInBounds, float size);
  void AddIcon(int indent, int no, bool bInBounds, float size);
  void AddIcon(Vector3 pos, int no, bool bInBounds, float sizeW, float sizeH);
  void AddIcon(int indent, int no, bool bInBounds, float sizeW, float sizeH);
  void AddIcon(int indent, float x, float y, float z, int no, bool bInBounds, float size = 1.f);
  void AddContiguousIcon(int indent, float x, float y, float z, int no, int noWidth, int noHeight, bool bInBounds, float size);
  void AddSquare(Vector3 pos, int no, bool bInBounds, float sizeW, float sizeH);
  void SetButtonIcon(short buttonId, short gamepad);
  void SetButton(short buttonId, short gamepad);
  void SetButtonColliderOut(short buttonId, short gamepad);
  void AddButton(Vector3 pos, float sizeW, float sizeH, short buttonId, short gamepad, short buttonType, Color color);
  void AddButtonIcon(Vector3 pos, int no, float size, short buttonId, short gamepad);
  GameObject* AddTextMesh(Vector3 pos, float size, TextAlignment alignemt = TextAlignment::Left, TextAnchor anchor = TextAnchor::UpperLeft, float lineSpace = 1.f);
  GameObject* AddTextMesh(int indent, float x, float y, float z, float size, TextAlignment alignemt = TextAlignment::Left, TextAnchor anchor = TextAnchor::UpperLeft, float lineSpace = 1.f);
  void Build(int indent, Vector3 pos, Transform* parent = nullptr);
  void Build(int indent, int screenIndent, float x, float y, float z, Transform* parent = nullptr);
  std::shared_ptr<IBar> AddBar(int no, int barNo, int backNo, float x, float y, float z, float w, float adjustX);
  void VariableWindow(int no, float sizeW, float sizeH, int indent);
  void AddChangeIconTexture(int indent, float x, float y, float z, float size, bool bInBounds);

  // ---- factories ----
  static MenuParts* CreateIconTexture(int indent, float x, float y, float z, float size);
  static MenuParts* CreateBuildVariableWindow(int no, float sizeW, float sizeH, int indent, bool bCloneMaterial);
  static Vector3 CalcSquareSize(int partsNo, float sizeW, float sizeH) { return s_info[partsNo].CalcSquareSize(sizeW, sizeH); }
  template <class T = MenuParts> static T* Create(int partsNo, bool bCloneMaterial = false) {
    T* val = (new GameObject())->AddComponent<T>();
    val->m_maker = NewMaker(val->m_nowParts = partsNo, bCloneMaterial);
    return val;
  }
  static MenuParts* CreateWindow0(float width, float height);
  static MenuParts* CreateButton(float width, float height, short buttonId, short gamepad);
  static MenuParts* CreateButton(float width, float height, short buttonId, short gamepad, short buttonType, Color color);
  static MenuParts* CreateButtonIcon(int partsNo, int no, float size, short buttonId, short gamepad);
  static MenuParts* CreateBuildTextMenu(int indent, float x, float y, float z, Transform* parent, float size, TextAlignment alignemt = TextAlignment::Left,
                                        TextAnchor anchor = TextAnchor::UpperLeft, float lineSpace = 1.f);
  template <class T = MenuParts> static T* CreateBuildFrameParts(int frameNo, int indent, float x, float y, float z, bool bUseCollider = false, bool bDynamicMesh = false) {
    GameObject* gameObject = new GameObject();
    T* val = gameObject->AddComponent<T>();
    InitFrameParts(val, gameObject, frameNo, indent, x, y, z, bUseCollider, bDynamicMesh);
    return val;
  }
  static MenuParts* AttachMenuParts(GameObject* target, int indent, float x = 0.f, float y = 0.f, float z = 0.f);
  static MenuParts* CreateBuildDummy(int indent, float x = 0.f, float y = 0.f, float z = 0.f) { return AttachMenuParts(new GameObject(), indent, x, y, z); }
  static MenuParts* CreateFromTexture(const std::string& filename, bool bFitWidth, float haspect = 1.f);
  static MenuParts* CreateFromTextureWindow(Texture2D* t, int window_w, int window_h);
  // `go.AddComponent<TextMesh>()` also gives a MeshRenderer in Unity (RequireComponent); this returns it, adding one if the engine did not.
  static MeshRenderer* EnsureMeshRenderer(GameObject* go);

protected:
  // typed access helper (C# `component.GetComponent<T>()` on this)
  template <class T> T* GetComponent() { return gameObject->GetComponent<T>(); }
private:
  static MeshMaker* NewMaker(int partsNo, bool bClone);   // MeshMaker over s_info[partsNo]'s material (a clone owned by the part when bClone)
  static void InitFrameParts(MenuParts* val, GameObject* gameObject, int frameNo, int indent, float x, float y, float z, bool bUseCollider, bool bDynamicMesh);
  static Material* l_colorFontList[7];
  static RaycastHit l_hit;
  static LangParam s_langParam[5];
  static TexInfo l_texInfo[2];
};

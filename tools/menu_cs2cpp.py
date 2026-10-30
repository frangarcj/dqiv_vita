#!/usr/bin/env python3
"""tools/menu_cs2cpp.py -- WP4 helper: first-pass C# -> C++ translation of MenuPortItemTable.cs methods (decompiled C#, very regular).
Usage: tools/menu_cs2cpp.py Make_ItemList Sub_ItemButton ...   (prints C++ to stdout; names of static methods in MenuPortItemTable.cs)
It only does the mechanical rewrites (types, ->, ::, lambdas, literals, named args); read the result and fix what the compiler complains about
(GameObject.transform vs Component.transform(), foreach, arrays, out/ref).  See the recipe in game/MenuPortItemTable.hpp."""
import re, sys
SRC = '/Users/frangar/Fun/dqiv/work/re/cs_out/Assembly-CSharp/MenuPortItemTable.cs'
STATIC = ['MenuParts','eIndent','Vector3','Vector2','Color','Mathf','MenuMessage','TextAlignment','TextAnchor','FRAME_ENUM','GetLangages','Sound','Global','Time','Screen',
          'Quaternion','MainLoop','TouchPad','Fade','Resources','Object','MenuPortItemTable','HoppingNumber','MeshMaker','Boot','Application','Input','MenuPortItem','MenuPort','Camera','Debug',
          'SceneCamera','Chara','NativeCall','Mesh','Rect','TextureFormat','Shader','Matrix4x4','Bounds','PrimitiveType','Array','Math','Texture2D','UnityEngine']
PTR_TYPES = ['MenuParts','MenuPortItem','Transform','GameObject','TextMesh','Renderer','MeshRenderer','MeshFilter','Mesh','Material','Texture2D','Texture','Camera','Collider','MeshCollider',
             'BattleBar','MonsterMarkList','MonsterNamePlate','ShakeFrame','BattleUpCursor','HoppingNumber','InputName','ItemNumCTRL','ICityMap','IBetText','StatusIconChanger','TapToStart','Font','MenuPort']
def methods(text):
    out = {}
    lines = text.split('\n')
    i = 0
    while i < len(lines):
        m = re.match(r'\t(public|private) static (\S+) (\w+)\((.*)\)\s*$', lines[i])
        if m and i + 1 < len(lines) and lines[i+1] == '\t{':
            j = i + 2
            while lines[j] != '\t}': j += 1
            out.setdefault(m.group(3), []).append((m.group(2), m.group(4), lines[i+2:j]))
            i = j
        i += 1
    return out
def conv_type(t):
    t = t.strip()
    if t == 'string': return 'std::string'
    if t in PTR_TYPES: return t + '*'
    if t.endswith('[]') and t[:-2] in PTR_TYPES: return 'std::vector<' + t[:-2] + '*>'
    return t
def conv_params(p, defaults=True):
    res = []
    for a in [x.strip() for x in p.split(',') if x.strip()]:
        m = re.match(r'(\S+) (\w+)(\s*=\s*(.*))?$', a)
        t, n, _, d = m.groups()
        d = (' = ' + conv_expr(d)) if (d and defaults) else ''
        res.append(f'{conv_type(t)} {n}{d}')
    return ', '.join(res)
def conv_expr(s):
    s = re.sub(r'\bnull\b', 'nullptr', s)
    s = re.sub(r'\bbool\.', 'bool.', s)
    return re.sub(r'(?<![\w.])(\d+)f\b', r'\1.f', s)
def conv_line(l):
    # literals
    l = re.sub(r'(?<![\w.])(\d+)f\b', r'\1.f', l)
    l = re.sub(r'\bnull\b', 'nullptr', l)
    l = re.sub(r'\bnew (Vector3|Vector2|Color|Rect|Bounds)\(', r'\1(', l)
    l = l.replace('string.Empty', '""')
    # named arguments
    l = re.sub(r'\b(bInBounds|bExternalToggle|bUseCollider|bDynamicMesh|bCloneMaterial|bFix|state|bFitWidth|bNoButton|bUp|bCheckYN|bShow|bPushed|value|bSubText|bForceButton|bForceDivTriangles):\s*', '', l)
    l = re.sub(r'(?<=[(,])\s*([a-z]\w*):(?!:)\s+', ' ', l)
    # lambdas
    l = re.sub(r'\(\s*(short|int|float) (\w+)\s*\)\s*=>', r'[=](\1 \2) mutable', l)
    l = re.sub(r'\bdelegate\(ref Ray (\w+)\)', r'[=](Ray& \1) mutable', l)
    l = re.sub(r'\bdelegate\s*\{', '[=](Ray&) mutable {', l)
    # static class access
    for c in STATIC:
        l = re.sub(r'\b' + c + r'\.(?=[A-Za-z_])', c + '::', l)
    # declarations of pointer types
    for t in ['StatusIconChanger','ItemNumCTRL','ICityMap','IBetText','BattleBar','IText','IButton','IIcon','IFrame','IBar']:
        l = re.sub(r'\bnew ' + t + r'\(', 'std::make_shared<' + t + '>(', l)
    for t in PTR_TYPES:
        l = re.sub(r'^(\s*)' + t + r' (\w+)( =|;)', r'\1' + t + r'* \2\3', l)
        l = re.sub(r'\bnew ' + t + r'\(', 'new ' + t + '(', l)
    l = re.sub(r'^(\s*)(\w+)\[\] (\w+) = new \2\[(\w+)\];', lambda m: f'{m.group(1)}{m.group(2)}* {m.group(3)}[{m.group(4)}] = {{}};', l)
    l = re.sub(r'^(\s*)\w+\* (\w+) = std::make_shared<', r'\1auto \2 = std::make_shared<', l)
    l = re.sub(r'\bstring (\w+)', r'std::string \1', l)
    l = re.sub(r'\buint (\w+)', r'uint32_t \1', l)
    # property-ish member access
    l = l.replace('.gameObject.active = ', '->gameObject->setActive(')
    l = re.sub(r'->gameObject->setActive\((.*);', r'->gameObject->setActive(\1);', l)
    l = re.sub(r'\.gameObject\.active\b', '->gameObject->active()', l)
    l = re.sub(r'\.gameObject\.', '->gameObject->', l)
    l = re.sub(r'\.gameObject\b', '->gameObject', l)
    l = re.sub(r'\.transform\.(parent|position)\s*=\s*([^;]+);', lambda m: f'.transform.__{m.group(1)}({m.group(2)});', l)
    # value-type members keep '.', everything else -> '->'
    VAL = r'(x|y|z|w|r|g|b|a|f|size|center|min|max|extents|width|height)'
    l = re.sub(r'(?<=[\w\)\]])\.(?!' + VAL + r'\b(?!\())(?=[A-Za-z_])', '->', l)
    l = re.sub(r'->(m_list|m_partsList|m_buttonList|m_uol)->Count\b', r'->\1.size()', l)
    l = re.sub(r'->(m_list|m_partsList|m_buttonList|m_uol)\[', r'->\1[', l)
    l = re.sub(r'->(m_list|m_partsList|m_buttonList|m_uol)->', r'->\1.', l)
    l = re.sub(r'->(anchor|alignment) = (Text\w+::\w+);', r'->\1 = (int)\2;', l)
    l = re.sub(r'->(m_list)\[(\w+)\]->', r'->\1[\2]->', l)
    l = re.sub(r'->transform\b(?!\()', '->transform()', l)
    l = l.replace('->gameObject->transform()', '->gameObject->transform')
    l = re.sub(r'->(renderer|collider|camera|active)\b(?!\()', r'->\1()', l)
    l = re.sub(r'__(parent|position)\(([^)]*)\)', lambda m: ('SetParent(' if m.group(1) == 'parent' else 'setPosition(') + m.group(2) + ')', l)
    l = l.replace('->transform()->__parent(', '->transform()->SetParent(').replace('->transform()->__position(', '->transform()->setPosition(')
    l = l.replace('->parent = ', '->SetParent(')
    l = re.sub(r'->m_intChanger\(', '->m_intChanger(', l)
    l = re.sub(r'\bforeach \((\w+) (\w+) in (\w+)\)', r'for (\1* \2 : \3)', l)
    l = re.sub(r'\bshort num(\d*) = 0; num\1 < ', lambda m: m.group(0), l)
    return l
def conv_method(name, info):
    rt, params, body = info
    sig = {'MenuPortItem': 'MenuPortItem*', 'MenuParts': 'MenuParts*', 'Transform': 'Transform*', 'Renderer': 'Renderer*', 'GameObject': 'GameObject*', 'Texture2D': 'Texture2D*', 'void': 'void', 'float': 'float'}.get(rt, rt)
    out = [f'{sig} MenuPortItemTable::{name}({conv_params(params, False)}) {{']
    lines = [conv_line(b[1:]) if b.startswith('\t') else conv_line(b) for b in body]
    text = '\n'.join(lines)
    for g in set(re.findall(r'GameObject\* (\w+)', text + ' ' + params.replace('GameObject ', 'GameObject* '))):
        text = re.sub(r'\b' + g + r'->transform\(\)', g + '->transform', text)
    for t in ['Vector3','Vector2','Color','MenuPort::Param','Bounds','Rect','Quaternion','Ray','std::string','Matrix4x4']:
        for g in set(re.findall(r'(?:^|[\s(,])' + re.escape(t) + r' (\w+)\b', text + ' ' + params)):
            text = re.sub(r'\b' + g + r'->', g + '.', text)
    text = re.sub(r'(position|rotation|lossyScale)\(\)->', r'\1().', text)
    text = re.sub(r'->position = ([^;]+);', r'->setPosition(\1);', text)
    text = re.sub(r'->(position|rotation|lossyScale|eulerAngles)\b(?!\()', r'->\1()', text)
    text = re.sub(r'(\w+(?:->\w+)*)->active\(\) = ([^;]+);', r'\1->setActive(\2);', text)
    text = re.sub(r'(AddTextMesh\([^;]*\))->transform\(\)', r'\1->transform', text)
    text = re.sub(r'(name_button_param\[\w+\])->', r'\1.', text)
    out.append(text)
    out.append('}')
    return '\n'.join(out)
def decl(name, info):
    rt, params, body = info
    sig = {'MenuPortItem': 'MenuPortItem*', 'MenuParts': 'MenuParts*', 'Transform': 'Transform*', 'Renderer': 'Renderer*', 'GameObject': 'GameObject*', 'Texture2D': 'Texture2D*'}.get(rt, rt)
    return f'  static {sig} {name}({conv_params(params)});'
if __name__ == '__main__':
    m = methods(open(SRC).read())
    if sys.argv[1] == '--decls':
        for n in sys.argv[2:]:
            for inf in m[n]: print(decl(n, inf))
        sys.exit(0)
    for n in sys.argv[1:]:
        if n not in m: print(f'// !! {n} not found'); continue
        for inf in m[n]: print(conv_method(n, inf)); print()

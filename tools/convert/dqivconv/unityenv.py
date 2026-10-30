"""Access to the Unity 4.7 serialized files + global object registry / PPtr resolution."""
import os
import UnityPy
from . import blob as B

UNITY_ROOT = os.environ.get("DQIV_UNITY_ROOT", "/Users/frangar/Fun/dqiv/work/extract/unity")   # tools/make_data.py sets it to its work dir
SCENE_FILES = ["mainData", "level0"]            # scenes exported as prefabs scene/<name>
SHARED_FILES = ["sharedassets0.assets", "sharedassets1.assets"]
TOPLEVEL = set(SCENE_FILES + SHARED_FILES)

# Unity class names we turn into standalone pack objects
EXPORT_KIND = {"Texture2D": B.K_TEX, "Mesh": B.K_MESH, "Material": B.K_MAT, "Shader": B.K_SHADER,
               "TextAsset": B.K_TEXT, "Font": B.K_FONT, "AnimationClip": B.K_ANIM}
# classes silently dropped (documented in the report)
SKIP_CLASSES = {"Avatar": "FBX importer humanoid/generic avatar; unused by the C# (no Animator use)",
                "Animator": "FBX importer default Animator; game uses legacy Animation",
                "MonoScript": "script metadata (only class names are kept in MonoBehaviour records)",
                "PreloadData": "editor preload table", "OcclusionCullingSettings": "scene setting",
                "HaloManager": "scene setting", "LightmapSettings": "scene setting", "NavMeshSettings": "scene setting"}

def file_path(name):
    if name in TOPLEVEL:
        return os.path.join(UNITY_ROOT, name)
    return os.path.join(UNITY_ROOT, "obb_data", name)

def load_sf(name):
    env = UnityPy.load(file_path(name))
    return list(env.files.values())[0]

def list_files():
    """Names of every serialized file we know about (obb_data guids + shared + scenes)."""
    names = [n for n in os.listdir(os.path.join(UNITY_ROOT, "obb_data")) if n not in TOPLEVEL]
    names.sort()
    return SHARED_FILES + SCENE_FILES + names

def ext_names(sf):
    return [e.path for e in sf.externals]

class Registry:
    """gid assignment. gid 0 is reserved for null."""
    def __init__(self):
        self.ids = {}            # (fname, pid) -> gid
        self.kind = [0]          # gid -> kind
        self.where = [None]      # gid -> (fname, pid)
        self.by_file = {}        # fname -> [(pid, gid, kind)]
        self.shader_names = {}   # gid -> declared shader name
        self.files = set()
        self.scene_gid = {}      # scene name -> gid

    def add(self, fname, pid, kind):
        key = (fname, pid)
        g = self.ids.get(key)
        if g is not None:
            return g
        g = len(self.kind)
        self.ids[key] = g
        self.kind.append(kind)
        self.where.append(key)
        self.by_file.setdefault(fname, []).append((pid, g, kind))
        return g

class Resolver:
    """PPtr -> gid for objects of one serialized file."""
    def __init__(self, reg, fname, sf):
        self.reg, self.fname = reg, fname
        self.ext = ext_names(sf)
        self.unresolved = []

    def target(self, pptr):
        fid, pid = pptr["m_FileID"], pptr["m_PathID"]
        if pid == 0:
            return None, 0
        if fid == 0:
            return self.fname, pid
        if fid - 1 >= len(self.ext):
            return "?bad", pid
        return self.ext[fid - 1], pid

    def ref(self, pptr, want_kind=None):
        """Return gid, or BUILTIN_BIT|pid for Unity built-ins, or 0 for null/missing."""
        fn, pid = self.target(pptr)
        if fn is None:
            return 0
        g = self.reg.ids.get((fn, pid))
        if g is not None:
            if want_kind and self.reg.kind[g] != want_kind:
                self.unresolved.append("%s:%d wrong kind %d (want %d)" % (fn, pid, self.reg.kind[g], want_kind))
                return 0
            return g
        if fn in self.reg.files:
            self.unresolved.append("%s:%d not exported (missing/unsupported class)" % (fn, pid))
            return 0
        return B.BUILTIN_BIT | (pid & 0x7FFFFFFF)

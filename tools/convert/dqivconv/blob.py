"""Little-endian blob writer shared by all converters.

Every blob is: <fixed header (first 8 bytes: u32 magic, u32 total_size)> <body> <string table>.
String references are u32 byte offsets from the start of the blob (0 == empty / none).
"""
import struct

# object kinds (also used in index.bin)
K_TEX, K_MESH, K_MAT, K_SHADER, K_PREFAB, K_ANIM, K_TEXT, K_FONT = 1, 2, 3, 4, 5, 6, 7, 8
KIND_NAMES = {K_TEX: "Texture", K_MESH: "Mesh", K_MAT: "Material", K_SHADER: "Shader",
              K_PREFAB: "Prefab", K_ANIM: "AnimClip", K_TEXT: "TextAsset", K_FONT: "Font"}
# data files (index into index.bin data file table)
DF_META, DF_MESH, DF_TEX, DF_BLOB = 0, 1, 2, 3
DF_NAMES = ["meta.pak", "mesh.pak", "tex.pak", "blob.pak"]
KIND_DF = {K_TEX: DF_TEX, K_MESH: DF_MESH, K_MAT: DF_META, K_SHADER: DF_META, K_PREFAB: DF_META,
           K_ANIM: DF_META, K_TEXT: DF_BLOB, K_FONT: DF_BLOB}

def fourcc(s):
    return struct.unpack("<I", s.encode("ascii"))[0]

MAGIC = {K_TEX: fourcc("DQTX"), K_MESH: fourcc("DQMS"), K_MAT: fourcc("DQMT"), K_SHADER: fourcc("DQSH"),
         K_PREFAB: fourcc("DQPF"), K_ANIM: fourcc("DQAN"), K_TEXT: fourcc("DQTA"), K_FONT: fourcc("DQFN")}

NULL_ID = 0
BUILTIN_BIT = 0x80000000   # ref to a Unity built-in resource: BUILTIN_BIT | pathID (not in pack)

class W:
    """Append-only little-endian writer with deferred string table."""
    def __init__(self):
        self.b = bytearray()
        self._patches = []   # (pos, string)

    def __len__(self):
        return len(self.b)

    def u8(self, v): self.b += struct.pack("<B", v & 0xFF)
    def u16(self, v): self.b += struct.pack("<H", v & 0xFFFF)
    def u32(self, v): self.b += struct.pack("<I", v & 0xFFFFFFFF)
    def i32(self, v): self.b += struct.pack("<i", v)
    def i16(self, v): self.b += struct.pack("<h", v)
    def f32(self, *v): self.b += struct.pack("<%df" % len(v), *v)
    def raw(self, data): self.b += data
    def zeros(self, n): self.b += bytes(n)
    def align(self, n):
        r = len(self.b) % n
        if r: self.b += bytes(n - r)

    def str(self, s):
        """u32 placeholder -> offset of NUL-terminated utf-8 string in the blob's string table."""
        if s is None or s == "":
            self.u32(0)
            return
        self._patches.append((len(self.b), s))
        self.u32(0)

    def set_u32(self, pos, v):
        struct.pack_into("<I", self.b, pos, v & 0xFFFFFFFF)

    def finish(self):
        self.align(4)
        table = {}
        for pos, s in self._patches:
            off = table.get(s)
            if off is None:
                off = len(self.b)
                self.b += s.encode("utf-8", "replace") + b"\0"
                table[s] = off
            struct.pack_into("<I", self.b, pos, off)
        self.align(16)
        struct.pack_into("<I", self.b, 4, len(self.b))
        return bytes(self.b)

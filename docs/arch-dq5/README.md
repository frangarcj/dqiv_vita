# DQV Unity-cut: architecture + porting plan (index)

| file | content |
|---|---|
| `01_FRAME_LOOP.md` | per-frame order (Update/LateUpdate/render), boot/scene flow, `NativeManager.Update` algorithm, Global.Flag bits, 30 Hz timing, level0/mainData camera dump, native entry-point call list |
| `02_PACKET_PROTOCOL.md` | exact int/float/string/short packet formats both sides: 41 `NativeManager` commands, input stream, MenuPort mailbox, text-canvas texture handshake, DSSA/BSSA (DS GX emulation) -> mesh packets, particles, FieldSet |
| `03_MINI_UNITY.md` + `include/miniunity.hpp` | API census, Unity 4.7 semantics, rendering model, TextMesh/bitmap fonts, backend structure |
| `04_SHADERS.md` + `shaders/` | all 53 shaders (ShaderLab + GLES dumps, `INDEX.txt`), fragment-op table, C# users, vitaGL plan (FFP + 7 CG programs) |
| `05_PORTING_MAP.md` | class-by-class map, dropped classes + faked results, 6 work packages + interfaces, milestones, converter contract |
| `06_INPUT_AND_LAYOUT.md` | portrait layout options (408x544 default), touch mapping, physical buttons -> pad bits, pointer cursor |
| `07_LANDSCAPE.md` | landscape 960x544 feasibility, DS two-screen heritage, DQIV-Recompiled mapping, designs A/B/C, prototype (branch `land`), `land_shots/` |
| `decomp/` | Ghidra output used as evidence: `native_a.c` (GameInit/GameUpdate/Unity* packet fns/TextureWriter/FieldSet/MenuPortInit), `native_g3.c` (Dssa_G3_*, MG_G3_*, bssa_draw*, UnityMenuLoop), `menuport.c` (MenuPortCTRL/MenuPort), `xrefs.txt` (who calls UnitySetPacket/UnityGetPacket/UnityGetFloatPacket...), `emit1.c` (unity_load_fld) |
| `resources_paths.txt` | the full Resources.Load container (34,954 entries: path -> PPtr) from `mainData` |
| `../scripts/ListXref.java`, `listxref.sh` | Ghidra helper: list callers of a symbol (`./arch/listxref.sh arch dq5core <out> <name...>`) |

Cross references for native internals: `/Users/frangar/Fun/dqv/refs/DQIV-DECOMP` (DQ4 DS decomp, same engine): `src/main/fld/FLDObject.cpp` (the DS original of the C# `FldObject`), `src/main/dss/` (DSSA), `config/eur/arm9/symbols.txt` (names for `fld::FLDObject`, `args::DSSAObject`, `ardq::MenuManager`, `cmn::GameManager`, `ardq::TextAPI`...).

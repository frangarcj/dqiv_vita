# 02 - Native packet protocol (libdq5core <-> presentation layer)

Reader side = `NativeManager.cs`, `MenuPortSystem.cs`/`MenuPort.cs`, `ParticleSystem.cs`, `Field.cs`, `MenuMessage.cs`, `Sound.cs`.
Writer side = Ghidra of `libdq5core.so` (decompiled: `arch/decomp/native_a.c` `native_g3.c` `menuport.c` `emit1.c`, caller census `arch/decomp/xrefs.txt`).
Native writer API (all ARM, demangled): `UnitySetPacket(int)`, `UnitySetPacket(int,int)` (op + 1 int), `UnitySetPacket(int* newCursor)` (set), `UnityGetPacket(int op)` (writes op, returns ptr to payload, cursor NOT advanced),
`UnityGetPacket(int op,int n)` (writes op, advances by n ints), `UnityGetFloatPacket(int n)` (reserve n floats), `UnityGetFloatPacket()` / `UnitySetFloatPacket(float*)` (get/set float cursor), `UnitySetString(const char*)`.
Native keeps **raw pointers** to the arrays given in `GameInit`/`ParticleSystemInit`/`MenuPortInit` (they must be static, never freed, never moved).

## A. Buffers and sizes (allocated by the host, passed once)

| name | type/size | direction | role |
|---|---|---|---|
| `m_input` | `int[1024]` | host -> native | event stream, terminated by a negative int. `GameInit` sets `m_input[0] = -1` |
| `m_output` | `int[8192]` | native -> host | `[0]` = Global.Flag (round trip), then command stream, terminated by `-1` |
| `m_output_float` | `float[8192]` | native -> host | float operands, consumed in order by the commands |
| `m_string` | `char*[32]` | native -> host | C strings (map name...). Only `unity_load_fld` uses it (slot 0) |
| MenuPort `unityBuf` | `short[128]` | host -> native | ring-less linked-list mailbox (see D) |
| MenuPort `nativeBuf` | `short[2048]` | native -> host | same format |
| particles | `Vec3 posScale[2]`, `int idx[1176]`, `Vec3 vtx[784]`, `Vec2 uv[784]`, `Color32 col[784]` | native -> host | 196 quads |
| `FieldSet` | `int buf[]` in, `Vec2 dst[4000]` out | host calls native | world-map tile UVs |

Three **separate cursors** (ints, floats, strings): a command takes its ints from `m_output` and its floats from `m_output_float` independently (that is why e.g. `cmd_chara` can read float operands for bit 11 and int operands for bit 14 in any order).
Per-frame framing (identical on both sides): host writes `out[0]=flag` before; native resets cursors to `out+1`, `outF+0`, runs, writes `out[0]=flag'`, and a terminating `-1`.

## B. Host -> native input stream (`m_input`)
Written by `NativeManager.SetInt()` in `NativeManager.Update` before `GameUpdate`; read by native at start of `GameUpdate` until a negative value:

| op | ints following | meaning |
|---|---|---|
| 0 | `x*4096`, `y*4096`, `-z*4096` (fx32 20.12) | real camera world position after `SceneCamera.Update` (only sent the frame after a `cmd_camera`; native `g_camera_pos`) |
| 1 | `id` | animation finished. `id < 0x1000`: FLD common-animation object id -> `FLDObject::SetCommonAnimationEnd(id)`; else `ModelObject::setMotionEnd(id & 0xFF)` (fbx model slot) |
| -1 | - | end |

`GameUpdate(short pad_press, ushort pad_toggle)`: see 01 §2. Return: void.

## C. Native -> host command stream (`m_output`, 41 opcodes, dispatch table `m_cmd_tbl`)

Notation: `I` = int from m_output, `F` = float from m_output_float, `S` = string `m_string[idx]`. Bits are tested with `btst(v, mask)`.
"Emitters" = native functions (found by xref to `UnitySetPacket/UnityGetPacket/UnityGetFloatPacket`) - opcode attribution is by name/semantics except where marked **[verified]** (opcode constant seen in decompilation).
Mesh/field helpers use the DS "GX" immediate-mode emulation (section E).

| op | name | operands (exact order) | host action | native emitters |
|---|---|---|---|---|
| 0 | fld_load | `I v` ; `S=m_string[v&0xFF]`; mode = `v>>8` | town/dungeon/battle-stage FLD load: `Global.InitCamera(mode&1)`; mode==2 -> `SceneCamera.SetTargetCamera(false)`; `CharaTerm()`; `m_fld.Term()` if loaded; `m_fld.Load("map/"+S)`; mode==1 -> `m_fld.SetDispScale(6,6,6)`; `BackColor.SetColor(map.bg_color)`; `Chara.SetSleepNear(0,0)`; `Chara.SetPriorityShader()`; `Cleaner.Cleaning()` | `unity_load_fld` **[verified]** (builds the path string; mode 2 for some map ids; also emits op 12 flags 0x22/0x20) |
| 1 | fld_color | - | `m_fld.calculateExecSepia()` (re-apply sepia to FLD materials) | `ardq::FldStage::setSepiarEnable` |
| 2 | fld_alpha | `I v` | `SetMapObjAlpha(v&0xFFFF, (v>>16)&0xFF, v>>24)` (objId, alpha 0..31, keepPriority) | `FLDObject::SetMapObjAlpha` |
| 3 | fld_pos | `I objId`, `F x,y,z` | `SetMapObjPos(objId, Vec3)` | `FLDObject::SetMapObjPosFX32` |
| 4 | fld_rot | `I objId`, `F rx,ry` | `SetMapObjRot(objId, (rx,ry,0))` | `FLDObject::AddMapObjRotFX32 / SetMapUidRotFX32` |
| 5 | fld_common_anim | `I v` | `SetCommonAnimation(v&0xFFFF, v>>16)`; if `(v>>16) >= 2` add `v&0xFFFF` to anim_list (end notice via input op 1) | `FLDObject::SetCommonAnimation` |
| 6 | fld_event_anim | `I v` | `v<16384`: `SetEventAnimation(v)`; else `SetCameraNo(v&0x3FFF, 0)` | `ardq::FldStage::eventAnim`, `FLDObject::SetCameraNo` |
| 7 | fld_on_off | `I v` | `SetMapObjOnOff(v&0xFFFF, v>>16)` | `FLDObject::SetMapObjOnOff` |
| 8 | fld_etc | `I v` | bit30 (`0x40000000`): `Chara.SetPriorityShader(v&0xFFFF)`; bit29: `m_fld.FixProblems(v&0xFFFF)` (per-map fixups: trans<->transz shader swaps); else `m_fld_etc = v` (0 = attach chara[0] to map obj 38) | FldStage / Town code |
| 9 | back_color | `F` r,g,b (up) , `F` r,g,b (down) | `BackColor.SetColor(up,down)` (gradient quad colors) | `UnitySetMapBackColor` **[verified]** (DS 5-bit colors / 31) |
| 10 | fade | `I v` [+ `F r,g,b` if bit11] | layer = bit14 (16384) ? BG fade plane (1) : front (0); frames = `v & 0x7FF`; bit12 (4096) = fade OUT else fade IN; bit11 (2048) custom color, bit13 (8192) white, else black | `UnitySetFade` **[verified]** (op 10, sets Flag |= 1) |
| 11 | camera | `I flags` ; `F` x3 if &1 (pos), x3 if &2 (target), x3 if &4 (angleDeg, order x,y,z) | `SceneCamera.SetPosition/SetTarget/SetAngleDeg`; `m_camera_flag = 1` (=> `SceneCamera.Update()` this frame and echo of camera pos next frame) | `ar::Camera::send_unity` |
| 12 | camera_sub | `I flags`; `F` in order: &1 near, &2 far, &4 fov2 (deg, clamped 0..78), &16 yOffset, &32 lookDownAdjust; &128 no float: `SetTargetCamera(false)` | SceneCamera setters | `unity_load_fld` **[verified]** (0x22 = far + lookdown for some maps), `MapEventCamera::initialize`, `BookCamera`, `CasinoPoker` |
| 13 | chara_load | `I slot`, `I no` | `no>=0`: `m_chara[slot] = new Chara(no)`; if `no & 0x1000`: `m_billboard_request=false; SetFieldMode(); SetVisible(false)`. `no<0`: release. `Chara(no)`: texture `CharaTex/h%03d` of `no&0xFFF`; `(no>>16)&0xF` = pattern rows; `(no>>20)&7` = size type (0..7, shared quad meshes); bit 23 (`0x800000`) selects shader `CharaEx` instead of `Chara` | `ardq::DisplayCharacter::setup`, `SpriteCharacter::setup`, `BillboardCharacter` |
| 14 | chara | `I v`; slot = v&0xFF. In this order: bit 2048 -> `F scale`; 4096 -> `F r,g,b,a` color; 8192 -> `F x,y,z` position (and `billboard_request=true`); 16384 -> `I p` pattern (x=p&0xFF, y=p>>8); 32768 -> `F x,y,z` field position | `SetScale/SetColor/SetPosition/SetPattern/SetFieldPosition` | `DisplayCharacter::draw`, `SpriteCharacter::draw` |
| 15 | chara_on_off | `I slot`, `I vis` | `SetVisible(vis!=0)` | `setDisplayEnable` |
| 16 | chara_shadow | `I v`; slot v&0xFF; bits 768 -> visible=(v&256); bits 3072 -> separate=(v&1024); 4096 -> `F x,y,z` shadow pos; 8192 -> `F alpha`; 16384 -> `F x,y,z` field shadow pos | Chara shadow API | `setShadowFlag`, `setShadowOnlyFlag`, `draw` |
| 17 | chara_sleep_pose | `I v`; slot; bits 768 -> sleep=(v&256); bit 1024 -> `SetSleepDir(v>>12)` | lying-down pose billboards | `DisplayCharacter::setSleepFlag` |
| 18 | chara_sleep_near | `I defNear`, `I offset` (fx32/4096; 0 => defaults 0.27 / 0.194) | `Chara.SetSleepNear` | town code |
| 19 | chara_pose | `I v` | `chara[v&0xFF].SetTexture(v>>8)` | `BillboardCharacter::changePose` |
| 20 | rise_icon_load | `I slot`, `I no` | slot<16: create/release `Riseup(no)` (no>=2000 -> shader Chara & texture no-2000); slot>=15 also `TouchPad.SetPadBlink(no>=0)` | `BillboardItem::draw/cleanup` |
| 21 | rise_icon | `I slot`, `F x,y,z`, `F alpha` | position; `material.color = (1,1,1,alpha)`; billboard | `BillboardItem::draw` |
| 22 | mesh_load | `I slot`, `I no` | `no>=0`: `new MeshChara(no, no<1000 ? 1 : 3)` texture `DssaTex/m%03d`, color white; `no<0` release. slots 0..39 (Update hides 0..23 each frame) | `args::DSSAObject::send_unity_load`, `send_unity_ortho_load` |
| 23 | **mesh** | `I slot`, `F x,y,z`, `I N`, N x { `F u,v,x,y,z` ; every 4th vertex (v index&3==0) also `I alpha8` }, then `I idx[N*6/4]` | sets MeshChara position; builds 4-vertex quads: vtx(x,y,z), uv(u,v), color32 (255,255,255,alpha of its quad); triangles = idx; `SetVisible(true)`. Indices per quad are `(0,3,2, 0,2,1)+4q`. **[verified]** | `Dssa_G3_EndMesh`, `MG_G3_Flush` (op 0x17) (E) |
| 24 | mesh_on_off | `I slot`, `I vis` | | DSSAObject |
| 25 | mesh_shader | `I v`: slot=v&0xFF, shader = v>>8 | index into `{DssaChara, DssaMonster0, DssaMonster1, DssaCharaTransparent, DssaNormal, Chara, BssaCharaTransparent, BssaEffectTransparent}` | `DSSACharacter::SetShader/SetFrontShader` |
| 26 | mesh_color | `I slot`, `F r,g,b` | material.color=(r,g,b,1) **[verified op 0x1a]** | `MG_G3_Flush`: DS 15-bit color `uVar17 & 0x1f/31` ... |
| 27 | mesh_tex_offset | `F u`, `F v`, `I slot` (**floats first, slot last; separate streams so order is irrelevant**) | material `_MainTex` offset | `Dssa_G3_SetTexOffset` users |
| 28 | mesh_ortho | `I slot` | `m_dssa[slot].m_chara.layer = 11` (rendered by the DSSA ortho camera) | `send_unity_ortho_load` |
| 29 | field_load | `I n` | `n>0`: terminate any FLD, `Global.InitCamera(0)` if camera_mode!=0, `Field.Init()` (world map); else `Field.Release()`; both: `CharaTerm(); Cleaner.Cleaning()` | `fld::FieldData::setup/cleanup` |
| 30 | field | `I startX`, `I startY`, `I width`, `I len`, `I tiles[len]`, `F x`, `F y` | `Field.SetPosition(x,y)`; `Field.SetIndexList(startX,startY,width,tiles)` -> host calls native `FieldSet(width|scaleflag, len/width, tiles, uvOut)` and uploads UVs | `fld::FieldData::drawBlock` |
| 31 | field_symbol | `I v`: if `v<32768`: ctrl=v&0xFF, pic=v>>8, `F x,y,alpha` (creates/updates symbol, visible); else if bit 4096: `F scale`, `F prio(y)`; else visible=(v&16384) | world-map symbols (ships, towers, volcano animated) | `fld::FieldData::drawSymbol/draw_symbol/setDispSymbol` |
| 32 | field_scale | `I n`: n>=0 -> `Field.SetFloatingObject(chara[n].m_chara)`; n<0 -> `F scale` -> `Field.SetScale(scale)` | zoom world map | `FieldData::setOffset`, `drawSunkenFirmament` |
| 33 | fbx_load | `I slot`, `I no` | `m_model[slot] = new FbxObject(no)` (Resources `fbx/c%04d` prefab, rotation (0,180,0)); `no<0` release. 16 slots | `ModelObject::setup/cleanup` |
| 34 | fbx | `I v`: slot v&0xFF; 768 -> visible=(v&256); 2048 -> `F x,y,z` scale; 4096 -> `F r,g,b,a`; 8192 -> `F x,y,z` pos; 16384 -> `F x,y,z` euler rot; 32768 -> `I seq` PlayAnimation("Take %03d", seq+1) + register end-notice id `slot|0x1000` | skinned model | `ModelObject::draw` |
| 35 | map_color_rate | `F r,g,b` | `m_fld_load ? m_fld.SetMaterialColor(c) : Field.SetColor(c)` (time-of-day tint) | `UnitySetMapColorRate` **[verified]** (op 0x23) |
| 36 | screen_effect | `I v` (+ `F`s for spotlight, read via `NativeManager.GetFloatEx`) | `Capture.ScreenEffect(v)`: `v&0xFF`: 3 Tabi(+0x8000 variant), 5 DarkLamp, 6 Riremito, 7 Spotlight (bits 0x100 create [`F x,y,z` + torch flag 0x800], 0x200 side color [`F`x4], 0x400 light color [`F`x4], 0x1000 visible, `(v>>16)&0xFFFF` radius, 0x8000 release), 8 Sepia(0x8000 release), default = BattleEffect | `window::ScreenEffectManager::effectStart`, `TownStageEffectManager` |
| 37 | se_play | `I no` | `Sound.PlaySE(no)` (file `sound/SE%04d.akb`) | `SoundManager::playSe`, `Sound::sePlayDirect` |
| 38 | se_stop | `I no` | `Sound.StopSE(no)` | `SoundManager::stopSeWithIndex` |
| 39 | bgm | `I v` | `v<16384`: `Sound.PlayBGM(v)` (flags 0x2000 layer2, 0x1000 restart, 0xF00 fade ms*100, low 8 bits track -> `sound/BGM%02d.akb`); else `Sound.StopBGM(v&0x3FFF)` | `SoundManager::play/stop/stopBgmForce`, `Sound::setBgmVolume` |
| 40 | particle | `I v`; fn = v&0xFF | 0 init (town layer 0), 1 init (field, layer 9), 2 update+triangles (count = `v>>8` triangle indices; also scale), 3 update (pos+verts+uv+colors only), 4 release. Data in the particle arrays (F) | `CommonParticleEmitter::setup/execute/cleanup`, `TownSystemParticle` |

Notes
* The string stream: native does `**stringCursorPtr = const char*` (`UnitySetString`, returns index 0). In the port, `m_string[0]` is the only used slot. Strings are in native static storage (valid until the next `GameUpdate`).
* **Unknown/contradictory opcode attribution** is limited to column "native emitters"; host semantics are exact (they are the reference C#).
* Order matters: `GameUpdate` issues commands in the order of native draw calls: `fld_load` before `camera`, chara loads before chara updates, etc. The host executes them immediately in order (no batching); the visual state is composed afterwards.
* Frame invariants: every `MeshChara` slot 0..23 is hidden at frame start and re-shown only by `cmd_mesh` (op 23) -> **DSSA meshes are fully re-sent every frame** (they are immediate-mode draws). The rest (`Chara`, fld objects, field) are retained-mode with deltas.

## D. MenuPort (UI mailbox) protocol - `short[]` buffers

Two buffers with identical layout (`MenuPortCTRL`, native `Init(short size, short* buf)` at 0x1bd428):
```
buf[0 .. N-1]      head[port]   : offset of first unread message of that port (0 = empty)          N = 32 ports
buf[N .. 2N-1]     tail[port]   : offset of the "next" slot after the last message (0 if none)
buf[2N]  (=buf[64]) cursor      : next free offset; Init sets it to 2N+1 = 65
message at offset o :  buf[o] = len;  buf[o+1 .. o+len] = payload (shorts);  buf[o+1+len] = next offset (0 = last)
Write (BeginWrite/EndWrite, same as C# MenuPortSystem.Send):
   o = cursor;  link: buf[ (head[p]!=0 && tail[p]!=0) ? tail[p] : p ] = o;   buf[o] = len; payload...;
   tail[p] = o+1+len; buf[o+1+len] = 0; cursor = o+len+2
Read (MenuPort.Receive / MenuPortSystem.Receive): o = head[p]; if 0 -> none; head[p] = buf[o+1+buf[o]]; message payload = buf[o+1 ..]
```
* Host->native buffer is 128 shorts and native compacts it each frame (`MenuPort::Defrag`, called from `UnityMenuLoop`); native->host buffer 2048 shorts is compacted by native too (`MenuPortCTRL::Defrag`, `MenuPort::Defrag` 0x1bd7fc). The host **never defragments**: it only appends (`Send`) and advances head pointers (`Receive`). So host code must be a verbatim port of `MenuPortSystem.Send/Receive` and `MenuPort.Param`.
* Helpers: `Param[i]` = `short`; `GetInt(i)` = `(int)(short)p[2i] | ((int)(short)p[2i+1] << 16)` (**bug-compatible: low half is sign-extended**); `GetFloat(i)` = little-endian float from two shorts; `ConvertString()` = UTF-16 units p[0..len); `Send(int[])` splits ints to (lo,hi) shorts; `Send(byte[])` packs 2 bytes per short (little endian).
* **Port 0 = system channel** (native -> host), payload `[port, cmd, ...]`:

| cmd | meaning |
|---|---|
| 1 | open port `p`: `[p,1,category,type]` -> `MenuPort.CreateMenuPort(p, category, type)`: category 1 = `MenuPortItemTable.CreateMenuPortItem(type)` (68 screen builders; native names `gMI_*` in .data, e.g. `gMI_Root`, `gMI_Battle*`, `gMI_Shop*`, `gMI_StartMenu*`, `gMI_NameEntry`, `gMI_Config`), 2 = `MenuMessage` (message window singleton), 3 = `EndrollPort`, 4 = `NetConnection`, 5 = `SlimeTouchPort`. Native side `MenuPort::Open(cb, category, type)` assigns the first free slot 1..31 and keeps a callback `cb(user)` (`ceremony::Unity_Callback` etc.), invoked from `UnityMenuLoop` when its port has messages |
| 2 | close port `p` |
| 3 | `[p,3,visible]` `SetVisible` |
| 4 | `[0,4,v]` SE volume = `Sound.SetVolumeSE(v/25 - 1)` (`MenuPortSendSEVolume`) |
| 5 | BGM volume = `Sound.SetVolumeBGM(v/25 - 1)` |
| 8 | `[0,8,pos,size]` -> `TouchPad.SetPosPadSize(pos,size)` (pad position 0 left,1 center-left,2 right,3 center-right; size 0..2) - **ignore with physical buttons** |
| 9 | `[0,9,mode]` -> `TouchPad.SetInputMode(mode)` (0 normal field, 1/2 sugoroku, 3 scope, 4 special) - **ignore the visuals, but keep `l_inputMode` because it decides which on-screen buttons exist (only matters for touch)** |
| 999 | `ApplicationQuit.Create()` (quit dialog) |

* Per-port payload semantics (what each C# port class interprets) are defined by those classes' `NoticeReceive/Update` (they are the spec; port verbatim). Key ones:
  * **MenuPortItem** (category 1): msgs `[cmd, ...]`: 1 setup(bPad, bCancel, bActive), 2 item grid (w,h,num), 3 directPad, 4 active item index (cursor highlight), 5 draw request, 6 enable input, 7 stop (deactivate parts), 8 ignore-draw, 10 set item int param `(id,no,lo,hi)`, 11 set item string param `(id,no,sprm)` + next message = UTF-16 string (`Receive().ConvertString()`; color tags `@c0..@c6` -> `<color=...>` rich text; **TextMesh richText must support `<color=yellow|red|gray|black|cyan|...>`**, see 03 §TextMesh), 12 set item code, 13 cancel draw. Host -> native: `Send(1, buttonId, gamepadCode)` on button release (OUC_Input = 1), plus `TouchPad.m_externalToggle |= gamepadCode`.
  * **MenuMessage** (category 2): native->host `[0,type]` window type = index into `MenuMessage.canvas_data` (0 default centred talk window with frame 192; 1 and 2 = frames 193/194 anchored indent 0 with dify 758/692; 3 = like 2 plus yes/no height reservation `bCheckYN`), `[1,on]` page-feed cursor, `[2,n]` shake n times, `[3,y]` canvas V scroll (`(ushort)y % 512 / 512`), `[4,flag]` + string = speaker name (English only; creates a TextMesh). At creation host->native: `SendInts(glTexId, TextureFormat.Alpha8(=1), 512, 512, aspect>0.74 ? 1 : 0)`.
  * **SlimeTouchPort / EndrollPort / NetConnection**: specific; see 05 (stub NetConnection).
* **Text canvas texture** (the only cross-boundary texture): host creates `Texture2D(512,512,Alpha8)`, sends its **native GL texture name** to the port; native `TextureWriter::Create(id,?,w=512,h=512)` stores `GL_ALPHA(0x1906)`, `GL_UNSIGNED_BYTE`; `TextCanvas::Print/PutChar` rasterise glyphs into a CPU A8 buffer (own font, `TextCanvas::LoadFonts`) and mark one of 8 `TextureWriter` slots dirty (1 = whole image, 2 = row range [y0,y1)); `UnityRenderEvent(1)` -> `TextCanvasTransfer` (0xe7e40) does for each dirty slot `glBindTexture(GL_TEXTURE_2D,id)` + `glTexSubImage2D(GL_TEXTURE_2D,0,0,y,512,h,GL_ALPHA,GL_UNSIGNED_BYTE,ptr)` + `glGetError()`. => engine requirements: `Texture2D::nativeId()` is the real GL name; the texture must have storage allocated (`glTexImage2D(...GL_ALPHA... NULL)`) as 512x512 A8 with sampler REPEAT in V (scroll) and nearest/linear as Unity default (bilinear); leave unit 0 active.

## E. DS "GX" immediate-mode emulation -> packets (DSSA / BSSA)
Native sprite animation formats: **DSSA** (`args::DSSAObject`, 3D-ish sprite parts, monsters/effects in battle and cutscenes) and **BSSA** (`bssa_draw`, `bssa_draw_3D`, `bssa_draw_prim`: mini-games, slime race, card games, bg_manage, town sugoroku). They issue GX-style calls on a software matrix stack; **no GL is involved**, the only output is op 22/23/26 packets:

* `Dssa_G3_BeginMesh(slot, x,y,z fx32)` (0xfa224): `p = UnityGetPacket(0x17)`; `p[0]=slot`; int cursor -> `p+2` (p[1] = vertex count, filled at end); float packet: `x/4096*s, y/4096*s, z/4096*s` position (3 floats, `s` = const).
* `Dssa_G3_SetParts(u0,v0,u1,v1)` (0xfa350): emits **one quad = 4 vertices x 5 floats `(u,v,x,y,z)`** (20 floats): UVs = base offset + `uv*texBaseSize` (`Dssa_G3_SetTexBaseSize` sets 1/w,1/h; `SetTexOffset` sets base), positions = current matrix (fx32 4.12 -> float, `MtxFx` stack 4x4 at `*(…)`) applied to the unit quad corners; vertex counter += 4. `Dssa_G3_SetAlpha(a)`: 5-bit DS alpha -> 8 bit (`a<31 ? a*8 : 255`) pushed to the INT stream (one per quad).
* `Dssa_G3_SetDualParts` duplicates the last quad with an uv offset (dual texturing emulation).
* `Dssa_G3_EndMesh()` (0xfa2b8): writes N into p[1], fills index list `(i, i+3, i+2, i, i+2, i+1)` per quad, advances int and float cursors (`UnitySetPacket(int*)`, `UnitySetFloatPacket`).
* `MG_G3_*` (software 4x4 matrix stack, `Vtx` multiplies the matrix: `*(int*)` fx32) feed `MG_G3_Flush()` (0x1992cc): for each of up to 10 distinct mesh slots used since the last flush, emit op 0x17 (position floats `(0,0,-0.0)`; vertices are already in final space) + all queued quads (same 20 floats/quad), then if a tint (DS 15-bit color with bit15 set) is pending emit op 0x1a with `r,g,b = (c&31)/31 ...`.
* BSSA draw entry points: `bssa_draw(BSSA_DATA*, anim, x, y, z, scale)` (2D: translate `(x,y,(i+1)*1.0)`, scale, per-part rotate via sin/cos table, `SetupSoftwareSpriteCamera`) and `bssa_draw_3D` (multiplies a view matrix `MtxFx44` + `MultMtx44`); both call `bssa_draw_prim` per part which calls `MG_G3_Begin/TexCoord/Vtx/SetAlpha/End`.
**Consequence for the Vita renderer**: the host does not need Unity meshes for these: `cmd_mesh` provides ready-to-draw world/screen-space quads with per-quad alpha. The port may (and should, for speed) draw them straight from the packet arrays: build one dynamic vertex array per slot (N verts, uv, white*alpha) and issue a single `glDrawElements` with the material state of `MeshChara` (shader id from op 25, color from op 26, texture `DssaTex/mNNN`, tex scale (1,-1)/offset (0,1) default + op 27). Visible = it was emitted this frame.
UV convention: `MeshChara` sets `_MainTex` scale `(1,-1)`, offset `(0,1)`: uv'=(u, 1-v) before the packet's own UV (so DS top-left origin textures work with GL bottom-left).

## F. Particles (op 40) native arrays
`ParticleSystemInit` (0x139c30): stores the 5 pointers; fills `col[]` with (255,255,255,0); fills `idx` for 196 quads: `(4i, 4i+3, 4i+1, 4i+1, 4i+3, 4i+2)`. Native emitter (`cmn::CommonParticleEmitter::execute`, `twn::TownSystemParticle`) updates `posScale[0]` (world pos), `posScale[1]` (scale), vertices (4 per particle), uv, colors, then emits op 40. Host: one `GameObject` with dynamic mesh, `Custom/Particle` (tex x vertex color, alpha blend, ZWrite off, queue Transparent+400), texture `etc/particles`, layer 0 (town) or 9 (field). Render only the first `v>>8` triangle indices (op fn 2) - fn 3 keeps the previous count.

## G. Misc exported entry points
* `FieldSet(width | 0x10000000 if scaling, rows, int* tileIds, Vec2* uvOut)` (0xf57b4): for each tile id: col = id & 0x3F, row = (id & 0xFFF) >> 6, orient = (id & 0xF000) >> 9 indexes a 16-entry corner-permutation table; scale `1/64`-ish const, bias `{b0, 0.0078125, b2}` chosen by `devInfo&1` (**PowerVR half-texel bias**) and by the zoom flag; writes 8 floats (4 UVs) per tile at `uvOut[tileIndex*4]`, 25 tiles per row stride (`+0x19`). Call it unchanged; mesh grid 25 x 40 quads (`Field.Init`), each quad 16 world units.
* `DevInit` -> `InitFileLoader(obbPath)`: opens OBB central directory (or uses the table from `LoadFileList`), `Backup_SetSavePath(savePath)`. Saves: `data1..9.dat` in `savePath`.
* `FileReadAllBytes(rel)` -> `ReadFileAlloc`: case-insensitive name lookup in the OBB table (names lowercase, no `assets/` prefix); returns malloc'd memory; free with `FreeFileMemory`.
* `GetMessageCanvas(void* dst, size_t n)`: memcpy of the TextCanvas CPU buffer (unused by C#).
* `DebugSetMapName/DebugSetValue/DebugGetValue/DebugCollSetMesh/UnityDebugFlag*`: debug only (set `g_UnityDebug` = 0; all C# callers are in debug classes). `IsLoadNativeCode()` returns 1 after JNI_OnLoad.

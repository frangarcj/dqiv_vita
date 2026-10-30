// game/Levels.hpp -- OWNER: WP1/WP3.  Scene builders standing in for Unity's scene loading (01 §1.1, §6).
// Today they build the objects by code from the documented parameters; target: instantiate the pack scenes `scene/maindata` / `scene/level0`.
#pragma once
namespace Levels {
void Build(const char* name);          // registered with Engine::SetLevelBuilder: "mainData" or "dq4sp" (level0)
void BuildMainData();                  // Main Camera (ortho 400, solid black) + Boot
void BuildLevel0();                    // MenuCamera + TouchPad, MainCamera + MainLoop (virtual-pad meshes are not built)
}

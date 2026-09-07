#pragma once

class BaseGameObject;
struct IndexedPoint;
class BaseMap;

using TFrameCallBackType = void(*)(BaseGameObject*, u32& pointIdx, const IndexedPoint&, ResourceManagerWrapper& resMan, BaseMap& map);

extern TFrameCallBackType gAbe_Anim_Frame_Fns[5];
extern TFrameCallBackType gSlig_Anim_Frame_Fns[4];
extern TFrameCallBackType gSlog_Anim_Frame_Fns[2];
extern TFrameCallBackType gFlyingSlig_Anim_Frames_Fns[3];
extern TFrameCallBackType gFleech_Anim_Frame_Fns[3];

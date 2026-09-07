#pragma once

class BaseGameObject;
struct IndexedPoint;
class ResourceManagerWrapper;
class BaseMap;

namespace AO {
using TFrameCallBackType = void(*)(BaseGameObject*, u32& pointIdx, const IndexedPoint&, ResourceManagerWrapper& resMan, BaseMap& map);

extern TFrameCallBackType gAbe_Anim_Frame_Fns[];
extern TFrameCallBackType gSlig_Anim_Frame_Fns[];
extern TFrameCallBackType gSlog_Anim_Frame_Fns[];
extern TFrameCallBackType gZBall_Anim_Frame_Fns[];
} // namespace AO

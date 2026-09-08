#pragma once

enum class CameraPos : s16;
class ScopedSeq;
class ResourceManagerWrapper;
class BaseMap;

namespace relive
{
    class Path_TLV;
}

void Start_Sounds_For_Objects_In_Near_Cameras(BaseMap& map);

void SND_Init_Ambiance();
void SND_Reset_Ambiance();

void Start_Sounds_for_TLV(CameraPos direction, relive::Path_TLV* pTlv, ResourceManagerWrapper& resMan, BaseMap& map);
void Start_Slig_sounds(CameraPos direction);
void Stop_slig_sounds(CameraPos direction);

struct Sound_Ambiance final
{
    ScopedSeq* mScopedSeq;
};

struct Sound_Ambiance_Array final
{
    Sound_Ambiance mArray[8];
};
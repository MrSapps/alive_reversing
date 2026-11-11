#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

// it reads the slurg spawned count from the TLV but never updates it.
// Also it hasn't got quiksave support.
// https://github.com/AliveTeam/alive_reversing/issues/1526

namespace relive {
struct Path_SlurgSpawner;
}

struct Slurg_Spawner_Path_Data final
{
    s16 mSpawnInterval;
    s16 mMaxSlurgs;
    s16 mSwitchId;
};

struct SlurgSpawnerSaveState final : public SaveStateBase
{
    SlurgSpawnerSaveState()
        : SaveStateBase(ReliveTypes::eSlurgSpawner, sizeof(*this))
    { }
    s16 mSpawnCounter;
    u8 mSpawnedCount;
};

class SlurgSpawner final : public BaseGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const SlurgSpawnerSaveState& state);

    SlurgSpawner(relive::Path_SlurgSpawner* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    
    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

private:
    Guid mTlvInfo;
    relive::Path_SlurgSpawner* mSlurgTlv = nullptr;
    Slurg_Spawner_Path_Data mSlurgPathData = {};
    s16 mSpawnCounter = 0;
    u8 mSpawnedCount = 0;
};

#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"
#include "../relive_lib/FixedPoint.hpp"

struct SlogSpawnerSaveState final : public SaveStateBase
{
    SlogSpawnerSaveState()
        : SaveStateBase(ReliveTypes::eSlogSpawner, sizeof(*this))
    { }
    s16 mSpawnedSlogsCount;
    s32 mSpawnTimer;
};

class SlogSpawner final : public BaseGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const SlogSpawnerSaveState& state);

    SlogSpawner(relive::Path_SlogSpawner* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;

private:
    Guid mTlvInfo;
    s16 mSpawnedSlogsCount = 0;
    FP mXPos = {};
    FP mYPos = {};
    s32 mSpawnTimer = 0;
    relive::reliveScale mScale = relive::reliveScale::eFull;
    s16 mMaxSlogs = 0;
    s16 mMaxSlogsAtATime = 0;
    relive::Path_SlogSpawner::StartDirection mStartDirection = relive::Path_SlogSpawner::StartDirection::eRight;
    s16 mSlogSpawnInterval = 0;
    s16 mSpawnerSwitchId = 0;
    bool mListenToSligs = false; // AE only
    s16 mChaseDelay = 0; // AE only
};

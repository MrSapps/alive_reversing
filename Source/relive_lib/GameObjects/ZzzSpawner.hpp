#pragma once

#include "BaseGameObject.hpp"
#include "../SaveStateBase.hpp"
#include "../FixedPoint.hpp"

namespace relive
{
    struct Path_ZzzSpawner;
}

struct ZzzSpawnerSaveState final : public SaveStateBase
{
    ZzzSpawnerSaveState()
        : SaveStateBase(ReliveTypes::eZzzSpawner, sizeof(*this))
    { }
    s32 mTimer;
};

class ZzzSpawner final : public BaseGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const ZzzSpawnerSaveState& state);

    ZzzSpawner(relive::Path_ZzzSpawner* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~ZzzSpawner();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

private:
    FP mXPos = {};
    FP mYPos = {};
    FP mSpriteScale = {};
    Guid mTlvInfo;
    s16 mSwitchId = 0;
    s32 mTimer = 0;
    s16 mZzzInterval = 0;
};

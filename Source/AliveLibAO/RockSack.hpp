#pragma once

#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class SerializedObjectData;

namespace relive {
struct Path_RockSack;
}

namespace AO {

struct RockSackSaveState final : public SaveStateBase
{
    RockSackSaveState()
        : SaveStateBase(ReliveTypes::eRockSack, sizeof(*this))
    { }
    FP mXPos = {};
    FP mYPos = {};
    s16 mCurrentPath = 0;
    EReliveLevelIds mCurrentLevel = EReliveLevelIds::eNone;
    FP mSpriteScale = {};
    Scale mScale = Scale::Fg;
    Guid mTlvId;
    bool mHasBeenHit = false;
    s16 mRockAmount = 0;
    bool mPlayWobbleSound = false;
    bool mForceWobbleSound = false;
    FP mTlvVelX = {};
    FP mTlvVelY = {};
};

class RockSack final : public ::BaseAliveGameObject
{
public:
    RockSack(relive::Path_RockSack* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map);
    ~RockSack();

    void LoadAnimations();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map);

private:
    Guid mTlvId;
    bool mHasBeenHit = false;
    s16 mRockAmount = 0;
    bool mPlayWobbleSound = false;
    bool mForceWobbleSound = false;
    FP mTlvVelX = {};
    FP mTlvVelY = {};
};
}

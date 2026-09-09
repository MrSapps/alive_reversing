#pragma once

#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class SerializedObjectData;

namespace relive
{
    struct Path_MeatSack;
}

namespace AO
{

struct MeatSackSaveState final : public SaveStateBase
{
    MeatSackSaveState()
        : SaveStateBase(ReliveTypes::eMeatSack, sizeof(*this))
    { }
    FP mXPos = {};
    FP mYPos = {};
    s16 mCurrentPath = 0;
    EReliveLevelIds mCurrentLevel = EReliveLevelIds::eNone;
    FP mSpriteScale = {};
    Scale mScale = Scale::Fg;
    Guid mTlvId;
    bool mHasBeenHit = false;
    s16 mMeatAmount = 0;
    bool mPlayWobbleSound = false;
    FP mTlvVelX = {};
    FP mTlvVelY = {};
};

class MeatSack final : public ::BaseAliveGameObject
{

public:
    MeatSack(relive::Path_MeatSack* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map);
    ~MeatSack();

    void LoadAnimations();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map);

    Guid mTlvId;
    bool mHasBeenHit = false;
    s16 mMeatAmount = 0;
    bool mPlayWobbleSound = false;
    FP mTlvVelX = {};
    FP mTlvVelY = {};
};

} // namespace AO
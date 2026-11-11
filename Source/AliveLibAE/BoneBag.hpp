#pragma once

#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

namespace relive
{
    struct Path_BoneBag;
}

struct BoneBagSaveState final : public SaveStateBase
{
    BoneBagSaveState()
        : SaveStateBase(ReliveTypes::eBoneBag, sizeof(*this))
    { }
    bool mHasBeenHit;
    bool mPlayWobbleSound;
    bool mForcePlayWobbleSound;
    s16 mBoneAmount;
};

class BoneBag final : public BaseAliveGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const BoneBagSaveState& state);

    BoneBag(relive::Path_BoneBag* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~BoneBag();

    void LoadAnimations();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

private:
    Guid mTlvInfo;
    bool mHasBeenHit = false;
    bool mPlayWobbleSound = true;
    bool mForcePlayWobbleSound = true;
    FP mTlvVelX = {};
    FP mTlvVelY = {};
    s16 mBoneAmount = 0;
};

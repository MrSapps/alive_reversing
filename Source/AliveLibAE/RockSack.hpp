#pragma once

#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

namespace relive
{
    struct Path_RockSack;
}

struct RockSackSaveState final : public SaveStateBase
{
    RockSackSaveState()
        : SaveStateBase(ReliveTypes::eRockSack, sizeof(*this))
    { }
    bool mHasBeenHit;
    s16 mRockAmount;
    bool mPlayWobbleSound;
    bool mForceWobbleSound;
};

class RockSack final : public BaseAliveGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const RockSackSaveState& state);

    RockSack(relive::Path_RockSack* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~RockSack();

    void LoadAnimations();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;

private:
    Guid mTlvId;
    bool mHasBeenHit = false;
    s16 mRockAmount = 0;
    bool mPlayWobbleSound = false;
    bool mForceWobbleSound = false;
    FP mTlvVelX = {};
    FP mTlvVelY = {};
};

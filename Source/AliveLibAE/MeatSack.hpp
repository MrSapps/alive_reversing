#pragma once

#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

namespace relive
{
    struct Path_MeatSack;
}

struct MeatSackSaveState final : public SaveStateBase
{
    MeatSackSaveState()
        : SaveStateBase(ReliveTypes::eMeatSack, sizeof(*this))
    { }
    bool mHasBeenHit;
    s16 mMeatAmount;
    bool mPlayWobbleSound;
};

class MeatSack final : public BaseAliveGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const MeatSackSaveState& state);

    MeatSack(relive::Path_MeatSack* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~MeatSack();

    void LoadAnimations();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;

private:
    Guid mTlvId;
    bool mHasBeenHit = false;
    s16 mMeatAmount = 0;
    bool mPlayWobbleSound = false;
    FP mTlvVelX = {};
    FP mTlvVelY = {};
};

#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "../relive_lib/Primitives.hpp"

class BaseAliveGameObject;

struct InvisibleEffectSaveState final : public SaveStateBase
{
    InvisibleEffectSaveState()
        : SaveStateBase(ReliveTypes::eInvisibleEffect, sizeof(*this))
    { }
    s32 mState;
    u8 mFadeSteps;
    Guid mTargetId;
    s32 mOldRenderMode;
    bool mSemiTrans;
    bool mBlending;
    bool mIsInvisible;
};

class InvisibleEffect final : public BaseGameObject
{
public:
    // For a save: nothing is looked up, the target is in the save
    struct RestoreTag final
    { };
    InvisibleEffect(RestoreTag, ResourceManager& resMan, BaseMap& map);

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    enum class InvisibleState : u16
    {
        eSetBlendMode1_0 = 0,
        eSetInvisibile_1 = 1,
        eBecomeInvisible_2 = 2,
        eUnknown_3 = 3,
        eBecomeVisible_4 = 4,
        eClearInvisibility_5 = 5,
        eSetDead_6 = 6
    };

    explicit InvisibleEffect(BaseAliveGameObject* pTarget, ResourceManager& resMan, BaseMap& map);
    ~InvisibleEffect();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

    void InstantInvisibility();
    void BecomeVisible();
    void ClearInvisibility();
    void BecomeInvisible();

private:
    InvisibleState mState = InvisibleState::eSetBlendMode1_0;

    // How far the colours have faded
    u8 mFadeSteps = 0;

    Guid mTargetId;
    relive::TBlendModes mOldRenderMode = relive::TBlendModes::eBlend_0;
    bool mSemiTrans = false;
    bool mBlending = false;
    bool mIsInvisible = false;
};

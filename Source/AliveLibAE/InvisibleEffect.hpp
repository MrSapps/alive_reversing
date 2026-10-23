#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/Primitives.hpp"

class BaseAliveGameObject;

class InvisibleEffect final : public BaseGameObject
{
public:
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

    explicit InvisibleEffect(BaseAliveGameObject* pTarget, ResourceManagerWrapper& resMan, BaseMap& map);
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

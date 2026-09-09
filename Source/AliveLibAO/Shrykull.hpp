#pragma once

#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class ZapLine;
class SerializedObjectData;

namespace AO {

struct ShrykullSaveState final : public SaveStateBase
{
    ShrykullSaveState()
        : SaveStateBase(ReliveTypes::eShrykull, sizeof(*this))
    { }
    FP mXPos = {};
    FP mYPos = {};
    FP mSpriteScale = {};
    Scale mScale = Scale::Fg;
    bool bFlipX = false;
    s32 mCurrentFrame = 0;
    u16 mFrameChangeCounter = 0;
    s16 mState = 0;
    s32 mZapIntervalTimer = 0;
    s32 mFlashTimer = 0;
    Guid mZapLineId;
    Guid mZapTargetId;
    bool mCanElectrocute = false;
    bool mResetRingTimer = false;
};

class Shrykull final : public ::BaseAliveGameObject
{
public:
    Shrykull(ResourceManagerWrapper& resMan, BaseMap& map);
    ~Shrykull();

    void LoadAnimations();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual void VOnThrowableHit(BaseGameObject*) override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map);

    bool CanElectrocute(BaseGameObject* pObj) const;
    bool CanKill(BaseAnimatedWithPhysicsGameObject* pObj);

    enum class State : s16
    {
        eTransform_0 = 0,
        eZapTargets_1 = 1,
        eDetransform_2 = 2,
        eFinish_3 = 3,
        eKillTargets_4 = 4,
    };
    State mState = State::eTransform_0;
    s32 mZapIntervalTimer = 0;
    s32 mFlashTimer = 0;
    Guid mZapLineId;
    Guid mZapTargetId;
    bool mCanElectrocute = false;
    bool mResetRingTimer = false;
};

} // namespace AO

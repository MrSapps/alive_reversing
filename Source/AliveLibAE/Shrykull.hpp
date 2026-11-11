#pragma once

#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

struct ShrykullSaveState final : public SaveStateBase
{
    ShrykullSaveState()
        : SaveStateBase(ReliveTypes::eShrykull, sizeof(*this))
    { }
    s32 mState;
    s32 mZapIntervalTimer;
    s32 mFlashTimer;
    Guid mZapLineId;
    Guid mZapTargetId;
    bool mCanElectrocute;
    bool mResetRingTimer;
};

class Shrykull final : public BaseAliveGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    Shrykull(ResourceManager& resMan, BaseMap& map);
    ~Shrykull();

    void LoadAnimations();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual void VOnThrowableHit(BaseGameObject*) override; 

private:
    static bool CanElectrocute(BaseGameObject* pObj);
    static bool CanKill(BaseAnimatedWithPhysicsGameObject* pObj);

private:
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

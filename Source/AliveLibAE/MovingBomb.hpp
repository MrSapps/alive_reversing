#pragma once

#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

namespace relive
{
    struct Path_MovingBomb;
}

struct MovingBombSaveState final : public SaveStateBase
{
    MovingBombSaveState()
        : SaveStateBase(ReliveTypes::eMovingBomb, sizeof(*this))
    { }
    s32 mState;
    s32 mTimer;
    FP mSpeed;
    s32 mChannelMask;
    bool mPersistOffscreen;
};

class MovingBomb final : public BaseAliveGameObject
{
public:
    virtual std::vector<NotStateMemory> VNotStateMemory() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const MovingBombSaveState& state);

    MovingBomb(relive::Path_MovingBomb* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~MovingBomb();
    
    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VOnThrowableHit(BaseGameObject* pFrom) override;
    virtual bool VTakeDamage(BaseGameObject* pFrom) override;
    virtual void VScreenChanged() override;

    // The TLV it was made from
    const Guid& TlvId() const
    {
        return mTlvId;
    }

private:
    void BlowUp();
    void FollowLine();
    s16 HitObject();

private:
    enum class States : s16
    {
        eTriggeredByAlarm_0 = 0,
        eTriggeredBySwitch_1 = 1,
        eMoving_2 = 2,
        eStopMoving_3 = 3,
        eWaitABit_4 = 4,
        eToMoving_5 = 5,
        eBlowingUp_6 = 6,
        eKillMovingBomb_7 = 7
    };
    States mState = States::eTriggeredByAlarm_0;
    Guid mTlvId;
    s32 mTimer = 0;
    FP mSpeed = {};
    s16 mStartMovingSwitchId = 0;
    s16 mMinStopTime = 0;
    s16 mMaxStopTime = 0;
    s32 mChannelMask = 0;
    bool mPersistOffscreen = false;
};

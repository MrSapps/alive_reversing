#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../SaveStateBase.hpp"
#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/Animation.hpp"

namespace relive
{
    struct Path_TimedMine;
}

struct TimedMineSaveState final : public SaveStateBase
{
    TimedMineSaveState()
        : SaveStateBase(ReliveTypes::eTimedMine, sizeof(*this))
    { }
    bool mSlappedMine;
    u16 mTicksUntilExplosion;
    u32 mExplosionTimer;
    AnimationSaveState mTickAnim;
    u32 mOldGnFrame;
    u32 mSingleTickTimer;
    bool mStickToLiftPoint;
};

class TimedMine final : public BaseAliveGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const TimedMineSaveState& state);

    TimedMine(relive::Path_TimedMine* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~TimedMine();

    void LoadAnimations();

    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VScreenChanged() override;
    virtual bool VTakeDamage(BaseGameObject* pFrom) override;
    virtual void VOnThrowableHit(BaseGameObject* pFrom) override;
    virtual void VOnAbeInteraction() override;

private:
    void StickToLiftPoint();
    void InitTickAnimation();
public:
    bool mSlappedMine = false;
    u16 mTicksUntilExplosion = 0;
    Guid mTlvInfo;
    u32 mExplosionTimer = 0;
    Animation mTickAnim = {};
    u32 mOldGnFrame = 0;
    u32 mSingleTickTimer = 0;
    bool mStickToLiftPoint = false;
};

#pragma once

#include "BaseAnimatedWithPhysicsGameObject.hpp"
#include "../SaveStateBase.hpp"
#include "../data_conversion/relive_tlvs.hpp"

struct BackgroundAnimationSaveState final : public SaveStateBase
{
    BackgroundAnimationSaveState()
        : SaveStateBase(ReliveTypes::eBackgroundAnimation, sizeof(*this))
    { }
    s32 mSoundChannelsMask;
};

class BackgroundAnimation final : public BaseAnimatedWithPhysicsGameObject
{
public:
    virtual std::vector<NotStateMemory> VNotStateMemory() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const BackgroundAnimationSaveState& state);

    BackgroundAnimation(relive::Path_BackgroundAnimation* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~BackgroundAnimation();

    virtual void VScreenChanged() override;
    virtual void VStopAudio() override;
    virtual void VUpdate() override;

private:
    Guid mTlvId;
    FP mObjectXPos = {};
    FP mObjectYPos = {};
    relive::Path_BackgroundAnimation::BgAnimSounds mSoundEffect = relive::Path_BackgroundAnimation::BgAnimSounds::eNone;
    s32 mSoundChannelsMask = 0;
};

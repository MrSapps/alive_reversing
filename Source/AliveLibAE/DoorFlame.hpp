#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

namespace relive
{
    struct Path_DoorFlame;
}

struct DoorFlameSaveState final : public SaveStateBase
{
    DoorFlameSaveState()
        : SaveStateBase(ReliveTypes::eDoorFlame, sizeof(*this))
    { }
    s16 mFrameCount;
    s32 mState;
    s16 mRandom;
    s32 mSoundsMask;
    Guid mFireBackgroundGlowId;
    Guid mFlameSparksId;
    // The sparks of the flame (the object with the id above), false for a save from the original game's
    bool mSparksSaved;
    bool mSparksRender;
    FP mSparksStartXPos;
    FP mSparksStartYPos;
    struct Spark final
    {
        FP mX;
        FP mY;
        FP mOffX;
        FP mOffY;
        s16 mRandom64;
        s16 mVisible;
        FP mSpriteScale;
        AnimationSaveState mAnim;
    };
    Spark mSparks[6];
};

class DoorFlame final : public BaseAnimatedWithPhysicsGameObject
{
public:
    virtual std::vector<NotStateMemory> VNotStateMemory() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const DoorFlameSaveState& state);

    DoorFlame(relive::Path_DoorFlame* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~DoorFlame();

    virtual void VUpdate() override;
    virtual void VStopAudio() override;
    virtual void VScreenChanged() override;

private:
    Guid mTlvInfo;
    s16 mFrameCount = 0;
    s16 mSwitchId = 0;
    enum class States : s16
    {
        eDisabled_0 = 0,
        eEnabled_1 = 1,
    };
    States mState = States::eDisabled_0;
    s16 mRandom = 0;
    s32 mSoundsMask = 0;
    Guid mFireBackgroundGlowId = Guid{};
    Guid mFlameSparksId;
};

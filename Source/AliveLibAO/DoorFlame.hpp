#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/Animation.hpp"
#include "../relive_lib/SaveStateBase.hpp"

namespace relive
{
    struct Path_DoorFlame;
}

namespace AO {

class FireBackgroundGlow;
class FlameSparks;

// The flame has a glow and sparks, which it makes and which are kept in its save
struct DoorFlameSaveState final : public SaveStateBase
{
    DoorFlameSaveState()
        : SaveStateBase(ReliveTypes::eDoorFlame, sizeof(*this))
    { }
    s32 mState;
    s16 mRandom;
    Guid mFireBackgroundGlowId;
    Guid mFlameSparksId;
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
    bool mHasGlow;
    FP mGlowXPos;
    FP mGlowYPos;
    FP mGlowXOff;
    FP mGlowYOff;
};

class DoorFlame final : public BaseAnimatedWithPhysicsGameObject
{
public:
    DoorFlame(relive::Path_DoorFlame* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~DoorFlame();

    virtual void VUpdate() override;
    virtual void VStopAudio() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    // The flame the camera makes (from its TLV) is put back once the camera has made it
    static void RestoreFromSaveState(const DoorFlameSaveState& state);

private:
    Guid mTlvInfo;
    s16 mSwitchId = 0;
    u32 mFrameCount = 0;
    enum class States : u16
    {
        eDisabled_0 = 0,
        eEnabled_1 = 1
    };
    States mState = States::eDisabled_0;
    s16 mRandom = 0;
    s32 mSoundsMask = 0;
    Guid mFireBackgroundGlowId = {};
    Guid mFlameSparksId = {};
};

} // namespace AO

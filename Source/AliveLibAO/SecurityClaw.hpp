#pragma once

#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/DynamicArray.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class SerializedObjectData;

namespace relive
{
    struct Path_SecurityClaw;
}

namespace AO {

class Claw final : public BaseAnimatedWithPhysicsGameObject
{
public:
    Claw(ResourceManagerWrapper& resMan, BaseMap& map);

    void VScreenChanged() override;
};

class MotionDetector;

enum class SecurityClawStates : s16
{
    eInit_0,
    eIdle_1,
    eDoZapEffects_2,
    eAnimateClaw_DoFlashAndSound_3
};

struct SecurityClawSaveState final : public SaveStateBase
{
    SecurityClawSaveState()
        : SaveStateBase(ReliveTypes::eSecurityClaw, sizeof(*this))
    { }
    Guid mTlvInfo;
    SecurityClawStates mState = SecurityClawStates::eInit_0;
    s32 mTimer = 0;
    s16 mAlarmSwitchId = 0;
    s16 mAlarmDuration = 0;
    FP mClawX = {};
    FP mClawY = {};
    u8 mAngle = 0;
    s32 mOrbSoundChannels = 0;
    bool mDetectorComeBack = false;
    PSX_Point mTlvTopLeft = {};
    PSX_Point mTlvBottomRight = {};
    bool mAnimLoaded = false;
    u32 mMotionDetectorArrayCount = 0;
    Guid mMotionDetectorArray[10] = {};
};

class SecurityClaw final : public ::BaseAliveGameObject
{
public:
    SecurityClaw(relive::Path_SecurityClaw* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map);
    ~SecurityClaw();

    void LoadAnimations();

    virtual void VScreenChanged() override;
    virtual bool VTakeDamage(BaseGameObject* pFrom) override;
    virtual void VUpdate() override;
    virtual void VOnThrowableHit(BaseGameObject* pFrom) override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map);

    Guid mTlvInfo;
    SecurityClawStates mState = SecurityClawStates::eInit_0;
    s32 mTimer = 0;
    s16 mAlarmSwitchId = 0;
    s16 mAlarmDuration = 0;
    FP mClawX = {};
    FP mClawY = {};
    u8 mAngle = 0;
    s32 mOrbSoundChannels = 0;
    bool mDetectorComeBack = false;
    Guid mClawId;
    PSX_Point mTlvTopLeft = {};
    PSX_Point mTlvBottomRight = {};

    // TODO: Refactor this
    bool mAnimLoaded = false;
    u32 mMotionDetectorArrayCount = 0;
    Guid mMotionDetectorArray[10] = {};
};

} // namespace AO

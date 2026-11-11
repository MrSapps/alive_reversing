#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"
#include "../relive_lib/SaveStateBase.hpp"

enum class DrillStates : s16
{
    eRestartCycle_0 = 0,
    eGoingDown_1 = 1,
    eGoingUp_2 = 2,
};

struct DrillSaveState final : public SaveStateBase
{
    DrillSaveState()
        : SaveStateBase(ReliveTypes::eDrill, sizeof(*this))
    { }
    Guid mDrillTlvId;
    s32 mOffTimer;
    DrillStates mState;
    FP mXYOff;
    // Made from the switch the moment it was loaded; false for a save without it
    bool mToggleStartStateSaved = false;
    bool mToggleStartState_StartOn = false;
};

class Drill final : public ::BaseAnimatedWithPhysicsGameObject
{
public:
    Drill(relive::Path_Drill* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~Drill();

    void LoadAnimations();
    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual std::vector<NotStateMemory> VNotStateMemory() override;
    virtual void VScreenChanged() override;
    virtual void VStopAudio() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;

    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    // Where it is in its stroke, for the bot. Parked it hurts nothing; out and back it kills what its rectangle
    // (KillRect) touches.
    enum class Stroke
    {
        eParked,
        eOut,  // the offset shrinks to 0
        eBack, // the offset grows to Travel(), then it parks
    };
    Stroke GetStroke() const;
    // Whether it starts another stroke once its pause is over, as the switch is now
    bool WillStart() const;
    // The speed of the stroke it is in, or of the next one when parked
    FP StrokeSpeed() const;
    FP Offset() const
    {
        return mXYOff;
    }
    s32 Travel() const
    {
        return mDrillDistance;
    }
    // Frames until a parked one may start
    s32 PauseLeft() const;
    relive::Path_Drill::DrillDirection Direction() const
    {
        return mDrillDirection;
    }
    PSX_RECT KillRect();
    const Guid& TlvId() const
    {
        return mTlvInfo;
    }

private:
    void EmitSparks();
    s16 DamageTouchingObjects();

private:
    DrillStates mState = DrillStates::eRestartCycle_0;
    s16 mDrillDistance = 0;
    s16 mDrillSwitchId = 0;
    relive::Path_Drill::DrillDirection mDrillDirection = relive::Path_Drill::DrillDirection::eDown;
    s16 mOnMinPauseTime = 0;
    s16 mOnMaxPauseTime = 0;
    s16 mOffMinPauseTime = 0;
    s16 mOffMaxPauseTime = 0;
    Guid mTlvInfo;
    s32 mOffTimer = 0;
    s32 mAudioChannelsMask = 0;
    FP mAdjustedXPos = {};
    FP mAdjustedYPos = {};
    FP mInitialSpeed = {};
    FP mCurrentSpeed = {};
    FP mOffSpeed = {};
    FP mXYOff = {};
    bool mStartOff = false;
    bool mToggleStartState_StartOn = false;
    bool mUseId = false;
    bool mToggle = false;
    bool mSpeedChange = false;
    bool mStartPosIsBottom = false;
};

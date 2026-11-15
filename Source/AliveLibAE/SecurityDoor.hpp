#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "GameSpeak.hpp"

namespace relive
{
    struct Path_SecurityDoor;
}

enum class SecurityDoorStates : s16
{
    eInactive_0 = 0,
    eSuccessChime_1 = 1,
    eSayingHi_2 = 2,
    eListeningForHi_3 = 3,
    eWaitingToSayPassword_4 = 4,
    ePreparingToSayPassword_5 = 5,
    eSayingPassword_6 = 6,
    ePausing_7 = 7,
    eListeningForPassword_9 = 9,
    eCheckingIfPasswordMatches_10 = 10,
    eSuccess_11 = 11,
    eFailure_12 = 12,
    eLaughAtFailure_13 = 13,
};

struct SecurityDoorSaveState final : public SaveStateBase
{
    SecurityDoorSaveState()
        : SaveStateBase(ReliveTypes::eSecurityDoor, sizeof(*this))
    { }
    s32 mState;
    s16 mCodeLength;
    s16 mMaxIdx;
    s16 mBufferStartIdx;
    s32 mTimer;
    s16 mMaxIdx2;
    s32 mPasswordBuffer[16];
};

class SecurityDoor final : public ::BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const SecurityDoorSaveState& state);

    SecurityDoor(relive::Path_SecurityDoor* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~SecurityDoor();

    void LoadAnimations();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;
    virtual GameSpeakListener* VListener() override
    {
        return &mListener;
    }
    // What it is doing now (read only: for the bot, which can't hear it)
    SecurityDoorStates State() const
    {
        return mState;
    }

private:
    s16 IsPlayerNear();

private:
    Guid mTlvId;
    SecurityDoorStates mState = SecurityDoorStates::eInactive_0;
    s16 mSwitchId = 0;
    s32 mCodeConverted = 0;
    s16 mCodeLength = 0;
    GameSpeakListener mListener;
    GameSpeakEvents mPasswordBuffer[16] = {};
    s16 mMaxIdx = 0; // TODO: better name
    s16 mBufferStartIdx = 0;
    PSX_Point mTlvTopLeft = {};
    PSX_Point mTlvBottomRight = {};
    s32 mTimer = 0;
    s16 mMaxIdx2 = 0; // TODO: better name
};

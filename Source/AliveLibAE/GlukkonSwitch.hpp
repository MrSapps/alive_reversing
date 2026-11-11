#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "GameSpeak.hpp"

namespace relive
{
    struct Path_GlukkonSwitch;
}

struct GlukkonSwitchSaveState final : public SaveStateBase
{
    GlukkonSwitchSaveState()
        : SaveStateBase(ReliveTypes::eGlukkonSwitch, sizeof(*this))
    { }
    s32 mState;
    s32 mTimer;
};

class GlukkonSwitch final : public ::BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const GlukkonSwitchSaveState& state);

    GlukkonSwitch(relive::Path_GlukkonSwitch* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~GlukkonSwitch();

    void LoadAnimations();

    virtual void VUpdate() override;

    virtual GameSpeakListener* VListener() override

    {

        return &mListener;

    }
    virtual void VScreenChanged() override;

private:
    bool PlayerNearMe() const;

private:
    Guid mTlvId;
    enum class State : s16
    {
        eWaitUntilGlukkonNearby,
        eSuccessFinish,
        eSayHey,
        eWaitForHeyInput,
        eSayWhat,
        eWaitForDoItInput,
        eSuccess,
        eListeningTimedOut,
        eFail
    };
    State mState = State::eWaitUntilGlukkonNearby;
    s16 mOkSwitchId = 0;
    s16 mFailSwitchId = 0;
    GameSpeakListener mListener;
    PSX_Point mTlvTopLeft = {};
    PSX_Point mTlvBottomRight = {};
    s32 mTimer = 0;
};

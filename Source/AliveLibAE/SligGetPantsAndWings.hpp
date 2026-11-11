#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

enum class State : s16
{
    eWaiting = 0,
    eActive = 1,
    eFinished = 2,
};

// TODO: HACK - pass in the scale directly at 100%.
// Either Path_Slig or Path_FlyingSlig with another TLV type value.
// Both of these start with a scale value which is the only thing this object reads.
struct SligGetPantsAndWingsSaveState final : public SaveStateBase
{
    SligGetPantsAndWingsSaveState()
        : SaveStateBase(ReliveTypes::eSligGetPantsOrWings, sizeof(*this))
    { }
    s32 mState;
    s32 mTransformTimer;
};

class SligGetPantsAndWings final : public ::BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const SligGetPantsAndWingsSaveState& state);

    SligGetPantsAndWings(relive::Path_TLV* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~SligGetPantsAndWings();

    void LoadAnimations();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

private:
    State mState = State::eWaiting;
    Guid mTlvInfo;
    s32 mTransformTimer = 0;
};

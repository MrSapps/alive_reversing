#pragma once

#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class SerializedObjectData;

namespace relive
{
    struct Path_BackgroundGlukkon;
}

namespace AO {

struct BackgroundGlukkonSaveState final : public SaveStateBase
{
    BackgroundGlukkonSaveState()
        : SaveStateBase(ReliveTypes::eBackgroundGlukkon, sizeof(*this))
    { }
    Guid mTlvId;
    s16 mState = 0;
    s32 mSpeakPauseTimer = 0;
};

class BackgroundGlukkon final : public ::BaseAliveGameObject
{
public:
    BackgroundGlukkon(relive::Path_BackgroundGlukkon* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map);
    ~BackgroundGlukkon();

    void LoadAnimations();

    virtual void VScreenChanged() override;
    virtual bool VTakeDamage(BaseGameObject* pFrom) override;
    virtual void VUpdate() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map);

    Guid mTlvId;
    enum class State
    {
        eToSetSpeakPauseTimer,
        eSetSpeakPauseTimer,
        eRandomizedLaugh,
        eAfterLaugh_SetSpeakPauseTimer,
        eKilledByShrykull
    };
    State mState = State::eToSetSpeakPauseTimer;
    s32 mSpeakPauseTimer = 0;
};

} // namespace AO

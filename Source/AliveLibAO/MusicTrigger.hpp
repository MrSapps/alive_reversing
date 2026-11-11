#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "MusicController.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"

namespace AO {

struct MusicTriggerSaveState final : public SaveStateBase
{
    MusicTriggerSaveState()
        : SaveStateBase(ReliveTypes::eMusicTrigger, sizeof(*this))
    { }
    Guid mTlvId;
    bool mHasSwitchId;
    bool mSetMusicToNoneOnDtor;
    bool mUnknown;
    s32 mCounter;
    s32 mMusicType;
    u16 mSwitchId;
};

class MusicTrigger final : public ::BaseGameObject
{
public:
    // For a save: nothing is started, that is in the save
    struct RestoreTag final
    { };
    MusicTrigger(RestoreTag, ResourceManager& resMan, BaseMap& map);

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);
    // The trigger the camera makes (from its TLV) is put back once the camera has made it
    static void RestoreFromSaveState(const MusicTriggerSaveState& state);

    MusicTrigger(relive::Path_MusicTrigger::MusicTriggerMusicType type, relive::Path_MusicTrigger::TriggeredBy triggeredBy, s32 switchId, s32 delay, ResourceManager& resMan, BaseMap& map);
    MusicTrigger(relive::Path_MusicTrigger* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);

    void Init(relive::Path_MusicTrigger::MusicTriggerMusicType type, relive::Path_MusicTrigger::TriggeredBy triggeredBy, u16 switchId, s16 delay);

    ~MusicTrigger();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;

    Guid mTlvId;
    bool mHasSwitchId = false;
    bool mSetMusicToNoneOnDtor = false;
    bool mUnknown = false;
    s32 mCounter = 0;
    MusicController::MusicTypes mMusicType = MusicController::MusicTypes::eType0;
    u16 mSwitchId = 0;
};

} // namespace AO

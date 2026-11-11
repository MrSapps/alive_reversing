#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "MusicController.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"
#include "../relive_lib/Psx.hpp"
#include "../relive_lib/SaveStateBase.hpp"

struct MusicTriggerSaveState final : public SaveStateBase
{
    MusicTriggerSaveState()
        : SaveStateBase(ReliveTypes::eMusicTrigger, sizeof(*this))
    { }
    Guid mTlvId;
    bool mTriggeredByTouching;
    bool mTriggeredByTimer;
    s32 mCounter;
    // What a trigger made by code (not a TLV) was made with
    s32 mMusicType;
    bool mSetMusicToNoneOnDtor;
    s32 mUpdateDelay;
};

class MusicTrigger final : public BaseGameObject
{
public:
    MusicTrigger(relive::Path_MusicTrigger* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    MusicTrigger(relive::Path_MusicTrigger::MusicTriggerMusicType musicType, relive::Path_MusicTrigger::TriggeredBy triggeredBy, s32 delay, ResourceManager& resMan, BaseMap& map);
    ~MusicTrigger();

    void Init(relive::Path_MusicTrigger::MusicTriggerMusicType musicType, relive::Path_MusicTrigger::TriggeredBy triggeredBy, s16 delay);
    
    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

private:
    Guid mTlvId;
    bool mTriggeredByTouching = false;
    bool mTriggeredByTimer = false;
    bool mSetMusicToNoneOnDtor = false;
    MusicController::MusicTypes mMusicType = MusicController::MusicTypes::eNone_0;
    s32 mCounter = 0;
    PSX_Point mTlvTopLeft = {};
    PSX_Point mTlvBottomRight = {};
};

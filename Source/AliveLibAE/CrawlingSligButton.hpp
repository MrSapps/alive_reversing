#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"

enum class SwitchOp : s16;

struct CrawlingSligButtonSaveState final : public SaveStateBase
{
    CrawlingSligButtonSaveState()
        : SaveStateBase(ReliveTypes::eCrawlingSligButton, sizeof(*this))
    { }
    bool mInUse;
};

class CrawlingSligButton final : public ::BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const CrawlingSligButtonSaveState& state);

    CrawlingSligButton(relive::Path_CrawlingSligButton* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~CrawlingSligButton();

    void LoadAnimations();

    virtual void VUpdate() override;
    void UseButton();

private:
    Guid mTlvId;
    s16 mSwitchId = 0;
    relive::reliveSwitchOp mAction = relive::reliveSwitchOp::eSetTrue;
    relive::Path_CrawlingSligButton::ButtonSounds mOnSound = relive::Path_CrawlingSligButton::ButtonSounds::None;
    relive::Path_CrawlingSligButton::ButtonSounds mOffSound = relive::Path_CrawlingSligButton::ButtonSounds::None;
    s16 mSoundDirection = 0;
    bool mInUse = false;
};

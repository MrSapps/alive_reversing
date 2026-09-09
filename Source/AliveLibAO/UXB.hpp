#pragma once

#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/Sfx.hpp"
#include "../relive_lib/SaveStateBase.hpp"

namespace relive
{
    struct Path_UXB;
}

class SerializedObjectData;

namespace AO {

enum class UXBState : u16
{
    eDelay = 0,
    eActive = 1,
    eExploding = 2,
    eDeactivated = 3
};

struct UXBSaveState final : public SaveStateBase
{
    UXBSaveState()
        : SaveStateBase(ReliveTypes::eUXB, sizeof(*this))
    { }
    Guid mTlvInfo;
    u32 mNextStateTimer = 0;
    UXBState mCurrentState = UXBState::eDelay;
    UXBState mStartingState = UXBState::eDelay;
    u16 mPatternIndex = 0;
    u16 mRedBlinkCount = 0;
    u16 mIsRed = 0;
};

class UXB final : public ::BaseAliveGameObject
{
public:
    UXB(relive::Path_UXB* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map);
    ~UXB();

    void LoadAnimations();

    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VScreenChanged() override;
    virtual void VOnAbeInteraction() override;
    virtual void VOnThrowableHit(BaseGameObject* pFrom) override;
    virtual bool VTakeDamage(BaseGameObject* pFrom) override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map);

private:
    void InitBlinkAnim(Animation* pAnimation);
    void PlaySFX(relive::SoundEffects sfxIdx);
    bool IsColliding();

private:
    UXBState mCurrentState = UXBState::eDelay;
    UXBState mStartingState = UXBState::eDelay;
    Guid mTlvInfo;
    u32 mNextStateTimer = 0;
    Animation mFlashAnim;
    u16 mPatternLength = 0;
    u16 mPatternIndex = 0;
    u16 mPattern = 0;
    u16 mRedBlinkCount = 0;
    u16 mIsRed = 0;
};

} // namespace AO

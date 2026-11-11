#pragma once

#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

namespace relive
{
    class Path_SecurityOrb;
}

struct SecurityOrbSaveState final : public SaveStateBase
{
    SecurityOrbSaveState()
        : SaveStateBase(ReliveTypes::eSecurityOrb, sizeof(*this))
    { }
    Guid mTlvId;
    s16 mState;
    s32 mTimer;
    FP mHealth;
};

class SecurityOrb final : public BaseAliveGameObject
{
public:
    SecurityOrb(relive::Path_SecurityOrb* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~SecurityOrb();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual std::vector<NotStateMemory> VNotStateMemory() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);
    virtual void VOnThrowableHit(BaseGameObject* /*pFrom*/) override
    {
        // Empty
    }
    virtual bool VTakeDamage(BaseGameObject* pFrom) override;

private:
    Guid mTlvInfo;
    enum class States : s16
    {
        eIdle_0,
        eDoZapEffects_1,
        eDoFlashAndSound_2
    };
    States mState = States::eIdle_0;
    s32 mTimer = 0;
    s32 mSoundChannelsMask = 0;
};

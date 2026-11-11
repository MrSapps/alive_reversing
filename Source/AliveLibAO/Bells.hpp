#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

namespace AO {

enum class BellSize : s16
{
    eBig = 0,
    eMedium = 1,
    eSmall = 2,
};

struct BellsSaveState final : public SaveStateBase
{
    BellsSaveState()
        : SaveStateBase(ReliveTypes::eBells, sizeof(*this))
    { }
    bool mSmashing;
    s32 mBellPitch;
    bool mDoScreenWave;
    s8 mSoundPitchFactor;
    s32 mSoundCooldownTimer;
};

class Bells final : public BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    Bells(BellSize bellSize, FP xpos, FP ypos, FP scale, ResourceManager& resMan, BaseMap& map);

    void LoadAnimations();

    virtual void VUpdate() override;

    void PlaySounds();
    bool CanSmash();
    void Ring();

    bool mSmashing = false;
    enum class BellPitch : u16
    {
        eLowPitch = 0,
        eMediumPitch = 1,
        eHighPitch = 2
    };
    BellPitch mBellPitch = BellPitch::eLowPitch;
    bool mDoScreenWave = false;
    s8 mSoundPitchFactor = 0;
    s32 mSoundCooldownTimer = 0;
};

} // namespace AO

#pragma once

#include "BaseAnimatedWithPhysicsGameObject.hpp"
#include "../SaveStateBase.hpp"

enum class BoomMachinePipeStates
{
    eInactive,
    eAlreadyUsed,
    eDropGrenadeAnimation,
    eDropGrenade
};

struct BoomMachinePipeSaveState final : public SaveStateBase
{
    BoomMachinePipeSaveState()
        : SaveStateBase(ReliveTypes::eBoomMachinePipe, sizeof(*this))
    { }
    s32 mState;
    s32 mTimer;
    s16 mGrenadeCount;
};

class BoomMachinePipe final : public ::BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    BoomMachinePipe(FP xpos, FP ypos, FP scale, s16 numGrenades, ResourceManager& resMan, BaseMap& map);

    void LoadAnimations();
    void DropGrenadeAnimation();
    void AlreadyUsed();

private:
    virtual void VUpdate() override;

    BoomMachinePipeStates mState = BoomMachinePipeStates::eInactive;
    s32 mTimer = 0;
    s16 mGrenadeCount = 0;
};

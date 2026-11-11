#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/FixedPoint.hpp"
#include "../relive_lib/SaveStateBase.hpp"

struct SlapLockWhirlWindSaveState final : public SaveStateBase
{
    SlapLockWhirlWindSaveState()
        : SaveStateBase(ReliveTypes::eSlapLock_OrbWhirlWind, sizeof(*this))
    { }
    s16 mSwitchId;
    // False for a save from the original game's, which has only the switch: it is set and the object is not made again
    bool mFullState;
    FP mDoorX;
    FP mDoorY;
    FP mDoorSpriteScale;
    Guid mOrbWhirlWindId;
    s16 mState;
    s32 mTimer;
};

class SlapLockWhirlWind final : public BaseGameObject
{
public:
    SlapLockWhirlWind(s16 doorNumber, s16 switchId, FP xpos, FP ypos, FP scale, ResourceManager& resMan, BaseMap& map);
    
    virtual void VUpdate() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManager& resMan, BaseMap& map);

    s16 SwitchId() const;

    // For a save: nothing is looked for or made, it is all in the save
    struct RestoreTag final
    { };
    SlapLockWhirlWind(RestoreTag, ResourceManager& resMan, BaseMap& map);

private:
    FP mDoorX = {};
    FP mDoorY = {};
    FP mDoorSpriteScale = {};
    Guid mOrbWhirlWindId;
    s16 mState = 0;
    s32 mTimer = 0;
    s16 mSwitchId = 0;
};

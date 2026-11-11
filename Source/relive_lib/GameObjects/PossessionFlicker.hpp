#pragma once

#include "BaseGameObject.hpp"
#include "../SaveStateBase.hpp"

class BaseAliveGameObject;

struct PossessionFlickerSaveState final : public SaveStateBase
{
    PossessionFlickerSaveState()
        : SaveStateBase(ReliveTypes::ePossessionFlicker, sizeof(*this))
    { }
    Guid mTargetObjId;
    s32 mTimeToFlicker;
    s16 mNewRed;
    s16 mNewGreen;
    s16 mNewBlue;
    s16 mOldRed;
    s16 mOldGreen;
    s16 mOldBlue;
};

class PossessionFlicker final : public BaseGameObject
{
public:
    // For a save: nothing is applied to the target, that is in the save
    struct RestoreTag final
    { };
    PossessionFlicker(RestoreTag, ResourceManager& resMan, BaseMap& map);

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

    PossessionFlicker(BaseAliveGameObject* pToApplyFlicker, s32 duration, s32 r, s32 g, s32 b, ResourceManager& resMan, BaseMap& map);
    ~PossessionFlicker();

    const Guid& ObjectId() const;
private:
    Guid mTargetObjId;
    s32 mTimeToFlicker = 0;

    s16 mNewRed = 0;
    s16 mNewGreen = 0;
    s16 mNewBlue = 0;

    s16 mOldRed = 0;
    s16 mOldGreen = 0;
    s16 mOldBlue = 0;
};

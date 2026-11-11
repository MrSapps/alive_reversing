#pragma once

#include "BaseGameObject.hpp"
#include "PaletteOverwriter.hpp"
#include "../SaveStateBase.hpp"

class BaseAliveGameObject;
class PalleteOverwriter;

struct ElectrocuteSaveState final : public SaveStateBase
{
    ElectrocuteSaveState()
        : SaveStateBase(ReliveTypes::eElectrocute, sizeof(*this))
    { }
    Guid mTargetObjId;
    s16 mTargetRed;
    s16 mTargetGreen;
    s16 mTargetBlue;
    s16 mPalOverwritersCount;
    s32 mState;
    bool mExtraOverwriter;
    bool mKillTarget;
    bool mRestoreSprite;
    // The overwriters are in the list of objects in AO (saved on their own, known here by id), but held by this in AE
    bool mHasOverwriter[3];
    Guid mOverwriterIds[3];
    PalOverwriterData mOverwriters[3];
};

class Electrocute final : public BaseGameObject
{
public:
    Electrocute(BaseAliveGameObject* pTargetObj, bool bExtraOverwriter, bool bKillTarget, ResourceManager& resMan, BaseMap& map);
    ~Electrocute();

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    virtual void VPostRestore() override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

    void Stop();
    bool IsLastOverwriterFinished();

public:
    Guid mTargetObjId;
    s16 mTargetRed = 0;
    s16 mTargetGreen = 0;
    s16 mTargetBlue = 0;

private:
    // For a save: nothing is looked up, the target and the overwriters may not be made yet
    struct RestoreTag final
    { };
    Electrocute(RestoreTag, ResourceManager& resMan, BaseMap& map);

    void KillPalOverwriters();
    bool DeElectrocuteTarget(bool dealDamage);
    void RestoreSprite(BaseAliveGameObject& target);

    bool mExtraOverwriter = false;
    bool mKillTarget = false;
    s16 mPalOverwritersCount = 0;
    PalleteOverwriter* mPalOverwriters[3] = {};
    bool mRestoreSprite = false;
    enum class States
    {
        eSetNewColour,
        eAlphaFadeout,
        eHandleDamage,
        eKillElectrocute
    };
    States mState = States::eSetNewColour;
    // Only between a restore and VPostRestore
    Guid mRestoredOverwriterIds[3] = {};
};

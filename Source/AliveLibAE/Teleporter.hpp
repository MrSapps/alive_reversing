#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"

class Electrocute;

struct Relive_Path_Teleporter_Data
{
    s16 mTeleporterId;
    s16 mOtherTeleporterId;
    s16 mDestCamera;
    s16 mDestPath;
    EReliveLevelIds mDestLevel;
    s16 mSwitchId;
    relive::reliveScale mScale;
    relive::reliveScreenChangeEffects mWipeEffect;
    std::string mMovie1;
    std::string mMovie2;
    std::string mMovie3;
    s16 mElectricX;
    s16 mElectricY;
};

struct TeleporterSaveState final : public SaveStateBase
{
    TeleporterSaveState()
        : SaveStateBase(ReliveTypes::eTeleporter, sizeof(*this))
    { }
    s32 mSwitchState;
    s32 mState;
    bool mDestroySelf;
    Guid mElectrocuteId;
    bool mEffectsCreated;
};

class Teleporter final : public BaseGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const TeleporterSaveState& state);

    Teleporter(relive::Path_Teleporter* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~Teleporter();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

private:
    static Electrocute* Create_ElectrocuteEffect(ResourceManager& resMan, BaseMap& map);
    void SpawnRingSparks(Relive_Path_Teleporter_Data* pTlvData);

    Guid mTlvId;
    s32 mSwitchState = 0;

    enum class TeleporterState : s8
    {
        eWaitForSwitchOn_0 = 0,
        eIntoTeleporter_1 = 1,
        eTeleporting_2 = 2,
        eOutOfTeleporter_4 = 4,
    };
    TeleporterState mState = TeleporterState::eWaitForSwitchOn_0;
    bool mDestroySelf = false;
    Relive_Path_Teleporter_Data mTlvData = {};
    Guid mElectrocuteId;
    bool mEffectsCreated = false;
};

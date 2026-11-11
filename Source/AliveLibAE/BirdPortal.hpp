#pragma once

#include "../relive_lib/data_conversion/relive_tlvs.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "../relive_lib/GameObjects/IBirdPortal.hpp"

enum class CameraSwapEffects : s16;

struct BirdPortalSaveState final : public SaveStateBase
{
    BirdPortalSaveState()
        : SaveStateBase(ReliveTypes::eBirdPortal, sizeof(*this))
    { }
    PortalStates mState;
    u8 mMudCountForShrykull;
    Guid mTlvInfo;
    // False for saves made before the idle portal was saved: it then starts over, making new doves
    bool mIdleSaved;
    bool mDovesExist;
    s32 mTimer;
    // The doves' ids when saved, see BaseGameObject::RefreshId
    Guid mDoveIds[6];
    BirdPortalBaseState mBase;
};

enum class Event : s16;

class BirdPortal final : public IBirdPortal
{
public:
    BirdPortal(relive::Path_BirdPortal* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~BirdPortal();
    
    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    virtual void VPostRestore() override;

    virtual void VGiveShrykull(s16 bPlaySound) override;
    virtual void VExitPortal() override;
    virtual void VGetMapChange(EReliveLevelIds* level, u16* path, u16* camera, CameraSwapEffects* screenChangeEffect, FmvIds* fmvIds) override;

    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManager& resMan, BaseMap& map);
    // The portal the camera makes (from its TLV) is put back once the camera has made it
    static void RestoreFromSaveState(const BirdPortalSaveState& state);

private:
    s16 IsScaredAway();
    void KillTerminators();
    Event GetEvent();
};

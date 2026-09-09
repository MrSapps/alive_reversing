#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/DynamicArray.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"
#include "../relive_lib/GameObjects/IBirdPortal.hpp"
#include "../relive_lib/SaveStateBase.hpp"

enum class EReliveLevelIds : s16;
enum class Event : s16;
class SerializedObjectData;

namespace AO {

struct BirdPortalSaveState final : public SaveStateBase
{
    BirdPortalSaveState()
        : SaveStateBase(ReliveTypes::eBirdPortal, sizeof(*this))
    { }
    PortalStates mState = PortalStates::CreatePortal_0;
    u8 mMudCountForShrykull = 0;
    Guid mTlvInfo;
};

class BirdPortal final : public IBirdPortal
{
public:
    BirdPortal(relive::Path_BirdPortal* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map);
    ~BirdPortal();

    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;

    virtual void VGiveShrykull(s16 bPlaySound) override;
    virtual void VExitPortal() override;
    virtual void VGetMapChange(EReliveLevelIds* level, u16* path, u16* camera, CameraSwapEffects* screenChangeEffect, u16* movieId) override;

    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map);

private:
    s16 IsScaredAway();
    void KillTerminators();
    Event GetEvent();
};

} // namespace AO

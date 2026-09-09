#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "ResourceManagerWrapper.hpp"

class SerializedObjectData;

namespace AO {

struct Path_Slig;

struct SligSpawnerSaveState final : public SaveStateBase
{
    SligSpawnerSaveState()
        : SaveStateBase(ReliveTypes::eSligSpawner, sizeof(*this))
    { }
    Guid mTlvId;
    u16 mSligSpawnerSwitchId = 0;
};

class SligSpawner final : public ::BaseGameObject
{
public:
    SligSpawner(relive::Path_TLV* pTlv, relive::Path_Slig* pTlvData, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map);
    ~SligSpawner();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map);

private:
    Guid mTlvInfo;
    u16 mSligSpawnerSwitchId = 0;
    s16 mSpawnerFlags = 0;
    relive::Path_TLV mPathTlv = {};
};

} // namespace AO

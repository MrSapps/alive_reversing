#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"

namespace AO {

struct BellSongSaveState final : public SaveStateBase
{
    BellSongSaveState()
        : SaveStateBase(ReliveTypes::eBellSong, sizeof(*this))
    { }
    s32 mType;
    s32 mCode;
    s16 mCodeLength;
    s32 mCodeIdx;
    bool mDone;
};

class BellSong final : public ::BaseGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    BellSong(relive::Path_BellsongStone::BellsongTypes type, u32 code, ResourceManager& resMan, BaseMap& map);

    virtual void VUpdate() override;

    s32 mCodeIdx = 0;
    bool mDone = false;
    relive::Path_BellsongStone::BellsongTypes mType = relive::Path_BellsongStone::BellsongTypes::eWhistle;
    s32 mCode = 0;
    s16 mCodeLength = 0;
};

} // namespace AO

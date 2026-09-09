#include "stdafx.h"
#include "QuikSaveTypes.hpp"
#include "BaseMap.hpp"
#include "BitField.hpp"
#include "logger.hpp"
#include "FatalError.hpp"
#include "data_conversion/relive_tlvs.hpp"

static void WriteFlags(SerializedObjectData& pSaveBuffer, const relive::Path_TLV* pTlv, const BitField8<relive::TlvFlags>& flags)
{
    pSaveBuffer.WriteU8(flags.Raw().all);
    pSaveBuffer.WriteU8(pTlv->mTlvSpecificMeaning);
}

static u32 SaveBlyData_CountOrSave(SerializedObjectData* pSaveBuffer, BaseMap& map)
{
    u32 flagsTotal = 0;

    for (auto& binaryPath : map.GetLoadedPaths())
    {
        for (auto& cam : binaryPath->GetCameras())
        {
            for (auto& pTlv : cam->mTlvs.mTlvs)
            {
                if (pTlv->mAttribute == relive::QuiksaveAttribute::eClearTlvFlags_1)
                {
                    if (pSaveBuffer)
                    {
                        BitField8<relive::TlvFlags> flags = pTlv->mTlvFlags;
                        if (flags.Get(relive::TlvFlags::eBit1_Created))
                        {
                            flags.Clear(relive::TlvFlags::eBit1_Created);
                            flags.Clear(relive::TlvFlags::eBit2_Destroyed);
                        }

                        WriteFlags(*pSaveBuffer, pTlv.get(), flags);
                    }
                    flagsTotal++;
                }
                else if (pTlv->mAttribute == relive::QuiksaveAttribute::eKeepTlvFlags_2)
                {
                    if (pSaveBuffer)
                    {
                        WriteFlags(*pSaveBuffer, pTlv.get(), pTlv->mTlvFlags);
                    }
                    flagsTotal++;
                }
                else
                {
                    // Type 0 ignored
                }
            }
        }
    }
    return flagsTotal;
}

void BaseMap::SaveQuicksaveBlyData(SerializedObjectData& pSaveBuffer)
{
    pSaveBuffer.WriteRewind();

    const u32 flagsCount = SaveBlyData_CountOrSave(nullptr, *this);
    pSaveBuffer.WriteU32(flagsCount);

    SaveBlyData_CountOrSave(&pSaveBuffer, *this);
}

void BaseMap::RestoreQuicksaveBlyData(SerializedObjectData& pSaveData)
{
    pSaveData.ReadRewind();

    const u32 flagsTotal = pSaveData.ReadU32();
    u32 readFlagsCount = 0;
    for (auto& binaryPath : GetLoadedPaths())
    {
        for (auto& cam : binaryPath->GetCameras())
        {
            for (auto& pTlv : cam->mTlvs.mTlvs)
            {
                if (pTlv->mAttribute == relive::QuiksaveAttribute::eClearTlvFlags_1 || pTlv->mAttribute == relive::QuiksaveAttribute::eKeepTlvFlags_2)
                {
                    const bool isLastTlv = pTlv->mTlvFlags.Get(relive::TlvFlags::eBit3_End_TLV_List);

                    pTlv->mTlvFlags.Raw().all = pSaveData.ReadU8();

                    // OG bug: the bly data can overwrite the end tlv list flag so we restore it
                    if (pTlv->mTlvFlags.Get(relive::TlvFlags::eBit3_End_TLV_List) != isLastTlv)
                    {
                        LOG_WARNING("Bly data load removed end list terminator flag, putting it back");
                        pTlv->mTlvFlags.Set(relive::TlvFlags::eBit3_End_TLV_List);
                    }

                    pTlv->mTlvSpecificMeaning = pSaveData.ReadU8();
                    readFlagsCount++;

                    // Note: We can't check for an exact match because some OG demo saves have flags
                    // that are not being read
                    if (readFlagsCount > flagsTotal)
                    {
                        ALIVE_FATAL("Save data contains %d sets of flags but read more than that", flagsTotal);
                    }
                }
            }
        }
    }
}

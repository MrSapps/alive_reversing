#include "stdafx.h"
#include "BasePath.hpp"
#include "BaseMap.hpp"
#include "data_conversion/relive_tlvs.hpp"

TlvIterator BasePath::Get_First_TLV_For_Offsetted_Camera(s16 cam_x_idx, s16 cam_y_idx)
{
    const s32 camY = cam_y_idx + mMap.mCamIdxOnY;
    const s32 camX = cam_x_idx + mMap.mCamIdxOnX;

    if (camX >= mMap.mCamsOnX || camX < 0 || camY >= mMap.mCamsOnY || camY < 0)
    {
        return TlvIterator::Invalid();
    }

    BinaryPath* pPathData = mMap.GetPathResourceBlockPtr(mMap.mCurrentPath);
    return pPathData->TlvsForCamera(camX, camY);
}

TlvIterator BasePath::TLV_First_Of_Type_In_Camera(ReliveTypes objectType, s16 camX)
{
    TlvIterator tlvIterator = Get_First_TLV_For_Offsetted_Camera(camX, 0);
    while (tlvIterator.GetTlv())
    {
        if (tlvIterator.GetTlv()->mTlvType == objectType)
        {
            return tlvIterator;
        }
        tlvIterator = tlvIterator.Next_TLV();
    }
    return TlvIterator::Invalid();
}

TlvIterator BasePath::TLV_Next_Of_Type(TlvIterator tlvIterator, ReliveTypes type)
{
    // Skip current which is already of type
    tlvIterator = tlvIterator.Next_TLV();
    while (tlvIterator.GetTlv())
    {
        // Got the next of type
        if (tlvIterator.GetTlv()->mTlvType == type)
        {
            return tlvIterator;
        }
        tlvIterator = tlvIterator.Next_TLV();
    }
    return TlvIterator::Invalid();
}

void BasePath::TLV_Reset(const Guid& tlvId, s16 hiFlags)
{
    Set_TLVData(tlvId, hiFlags, 0, 0);
}

void BasePath::TLV_Persist(const Guid& tlvId, s16 hiFlags)
{
    Set_TLVData(tlvId, hiFlags, 1, 0);
}

void BasePath::TLV_Delete(const Guid& tlvId, s16 hiFlags)
{
    Set_TLVData(tlvId, hiFlags, 0, 1);
}

void BasePath::Set_TLVData(const Guid& tlvId, s16 hiFlags, s8 bSetCreated, s8 bSetDestroyed)
{
    for (std::unique_ptr<BinaryPath>& pBinPath : mMap.GetLoadedPaths())
    {
        if (pBinPath)
        {
            relive::Path_TLV* pTlv = pBinPath->TlvById(tlvId).GetTlv();
            if (pTlv)
            {
                pTlv->mTlvFlags.Set(relive::TlvFlags::eBit2_Destroyed, bSetDestroyed & 1);
                pTlv->mTlvFlags.Set(relive::TlvFlags::eBit1_Created, bSetCreated & 1);

                if (hiFlags != -1)
                {
                    // Seems to be a blob per TLV specific bits
                    pTlv->mTlvSpecificMeaning = static_cast<u8>(hiFlags);
                }
                break;
            }
        }
    }
}

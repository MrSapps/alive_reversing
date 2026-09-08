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

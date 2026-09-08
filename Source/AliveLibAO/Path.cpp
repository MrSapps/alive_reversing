#include "stdafx_ao.h"
#include "Path.hpp"
#include "Map.hpp"
#include "../relive_lib/MapWrapper.hpp"
#include "../relive_lib/BinaryPath.hpp"
#include "../relive_lib/AmbientSound.hpp"

namespace AO {

Path::Path(Map& map, relive::Factory& factory) : BasePath(map, factory)
{

}

void Path::Free()
{
    mPathData = nullptr;
    BasePath::Free();
}

void Path::Init(const PathData* pPathData, EReliveLevelIds level, u16 pathId, s16 cameraId, BinaryPath* pBinaryPath)
{
    mPathData = pPathData;
    mLevelId = level;
    mPathId = pathId;
    mCameraId = cameraId;
    mBinaryPath = pBinaryPath;
}

TlvIterator Path::VTLV_Get_At_Of_Type(s16 xpos, s16 ypos, s16 width, s16 height, ReliveTypes typeToFind)
{
    s32 right = 0;
    s32 left = 0;
    if (xpos >= width)
    {
        right = width;
        left = xpos;
    }
    else
    {
        right = xpos;
        left = width;
    }

    s32 top = 0;
    s32 bottom = 0;
    if (ypos >= height)
    {
        top = height;
        bottom = ypos;
    }
    else
    {
        top = ypos;
        bottom = height;
    }

    const s32 grid_cell_y = top / mPathData->field_E_grid_height;
    const s32 grid_cell_x = (right / mPathData->field_C_grid_width);

    // Check within map bounds
    if (grid_cell_x >= mMap.mCamsOnX)
    {
        return TlvIterator::Invalid();
    }

    if (grid_cell_y >= mMap.mCamsOnY)
    {
        return TlvIterator::Invalid();
    }

    // Get the offset to where the TLV list starts for this camera cell
    BinaryPath* pBinPath = mMap.GetPathResourceBlockPtr(mMap.mCurrentPath);
    TlvIterator tlvIterator = pBinPath->TlvsForCamera(grid_cell_x, grid_cell_y);
    if (!tlvIterator.GetTlv())
    {
        return TlvIterator::Invalid();
    }

    while(tlvIterator.GetTlv())
    {
        auto pTlv = tlvIterator.GetTlv();
        if (pTlv->mTlvType == typeToFind
            && right <= pTlv->mBottomRightX
            && left >= pTlv->mTopLeftX
            && bottom >= pTlv->mTopLeftY
            && top <= pTlv->mBottomRightY)
        {
            return tlvIterator;
        }
        tlvIterator = tlvIterator.Next_TLV();
    }
    return tlvIterator;
}

TlvIterator Path::TLV_Get_At(TlvIterator tlvIterator, FP xpos, FP ypos, FP width, FP height)
{
    
    const auto xpos_converted = FP_GetExponent(xpos);
    const auto ypos_converted = FP_GetExponent(ypos);
    auto width_converted = FP_GetExponent(width);
    auto height_converted = FP_GetExponent(height);
    
    bool bContinue = true;
    if (xpos_converted < 0 || ypos_converted < 0)
    {
        bContinue = false;
    }

    if (width_converted < 0 || height_converted < 0)
    {
        width_converted = FP_GetExponent(xpos);
        height_converted = FP_GetExponent(ypos);
    }

    if (!tlvIterator.GetTlv())
    {
        const PathData* pPathData = mPathData;

        const auto camX = xpos_converted / pPathData->field_C_grid_width;
        const auto camY = ypos_converted / pPathData->field_E_grid_height;

        if (camX >= mMap.mCamsOnX || camY >= mMap.mCamsOnY)
        {
            return TlvIterator::Invalid();
        }

        if (camX < 0 || camY < 0)
        {
            return TlvIterator::Invalid();
        }

        BinaryPath* pBinPath = mMap.GetPathResourceBlockPtr(mMap.mCurrentPath);
        tlvIterator =  pBinPath->TlvsForCamera(camX, camY);
        if (!tlvIterator.GetTlv())
        {
            return TlvIterator::Invalid();
        }

        if (!bContinue || (xpos_converted <= tlvIterator.GetTlv()->mBottomRightX && width_converted >= tlvIterator.GetTlv()->mTopLeftX && height_converted >= tlvIterator.GetTlv()->mTopLeftY && ypos_converted <= tlvIterator.GetTlv()->mBottomRightY))
        {
            return tlvIterator;
        }
    }

    if (tlvIterator.GetTlv()->mTlvFlags.Get(relive::eBit3_End_TLV_List))
    {
        return TlvIterator::Invalid();
    }

    while (1)
    {
        tlvIterator = tlvIterator.Next_TLV();

        if (!bContinue || (xpos_converted <= tlvIterator.GetTlv()->mBottomRightX && width_converted >= tlvIterator.GetTlv()->mTopLeftX && height_converted >= tlvIterator.GetTlv()->mTopLeftY && ypos_converted <= tlvIterator.GetTlv()->mBottomRightY))
        {
            break;
        }

        if (tlvIterator.GetTlv()->mTlvFlags.Get(relive::eBit3_End_TLV_List))
        {
            return TlvIterator::Invalid();
        }
    }
    return tlvIterator;
}

void Path::Loader(s16 camX, s16 camY, relive::Factory::LoadMode loadMode, ReliveTypes typeToLoad)
{
    // Get TLVs for this cam
    BinaryPath* pPathRes = mMap.GetPathResourceBlockPtr(mMap.mCurrentPath);
    TlvIterator tlvIterator = pPathRes->TlvsForCamera(camX, camY);
    while(tlvIterator.GetTlv())
    {
        auto pTlv = tlvIterator.GetTlv();
        if (typeToLoad == ReliveTypes::eNone || typeToLoad == pTlv->mTlvType)
        {
            if (loadMode != relive::Factory::LoadMode::ConstructObject_0 || !(pTlv->mTlvFlags.Get(relive::TlvFlags::eBit1_Created) || pTlv->mTlvFlags.Get(relive::TlvFlags::eBit2_Destroyed)))
            {
                // Call the factory to construct the item
                mFactory.ConstructTLVObject(pTlv, pTlv->mId, loadMode, mMap.GetResourceManager(), mMap);

                if (loadMode == relive::Factory::LoadMode::ConstructObject_0)
                {
                    pTlv->mTlvFlags.Set(relive::TlvFlags::eBit1_Created);
                    pTlv->mTlvFlags.Set(relive::TlvFlags::eBit2_Destroyed);
                }
            }
        }

        // End of TLV list for current camera
        if (pTlv->mTlvFlags.Get(relive::TlvFlags::eBit3_End_TLV_List))
        {
            break;
        }
        tlvIterator = tlvIterator.Next_TLV();
    }
}

void Path::Start_Sounds_For_Objects_In_Camera(CameraPos direction, s16 cam_x_idx, s16 cam_y_idx)
{
    BinaryPath* pPathData = mMap.GetPathResourceBlockPtr(mMap.mCurrentPath);

    // TODO: Shouldn't really need to depend on these
    const s32 cam_global_left = mPathData->field_C_grid_width * cam_x_idx;
    const s32 cam_global_right = cam_global_left + mPathData->field_C_grid_width;

    const s32 cam_y_grid_top = mPathData->field_E_grid_height * cam_y_idx;
    const s32 cam_y_grid_bottom = cam_y_grid_top + mPathData->field_E_grid_height;

    for (auto& cam : pPathData->GetCameras())
    {
        // Enumerate the TLVs
        for (auto& pTlv : cam->mTlvs.mTlvs)
        {
            if (pTlv->mTopLeftX >= cam_global_left && pTlv->mTopLeftX <= cam_global_right)
            {
                if (pTlv->mTopLeftY >= cam_y_grid_top && pTlv->mTopLeftY <= cam_y_grid_bottom && (!pTlv->mTlvFlags.Get(relive::eBit1_Created) && !pTlv->mTlvFlags.Get(relive::eBit2_Destroyed)))
                {
                    Start_Sounds_for_TLV(direction, pTlv.get(), mMap.GetResourceManager(), mMap);
                }
            }

            if (pTlv->mTlvFlags.Get(relive::eBit3_End_TLV_List))
            {
                break;
            }
        }
    }
}

void Path::Reset_TLVs(u16 pathNum)
{
    BinaryPath* pPathRes = mMap.GetPathResourceBlockPtr(pathNum);
    for (auto& cam : pPathRes->GetCameras())
    {
        for (auto& pTlv : cam->mTlvs.mTlvs)
        {
            pTlv->mTlvFlags.Clear(relive::TlvFlags::eBit1_Created);
            pTlv->mTlvFlags.Clear(relive::TlvFlags::eBit2_Destroyed);
            if (pTlv->mTlvFlags.Get(relive::TlvFlags::eBit3_End_TLV_List))
            {
                break;
            }
        }
    }
}


} // namespace AO

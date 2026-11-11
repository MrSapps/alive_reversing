#include "stdafx.h"
#include "../ObjectIds.hpp"
#include "../SerializedObjectData.hpp"
#include "../WorldStatic.hpp"
#include "DeathGas.hpp"
#include "../Events.hpp"
#include "../../AliveLibAO/Abe.hpp"
#include "../../AliveLibAE/Abe.hpp"
#include "../PsxDisplay.hpp"
#include "../GameType.hpp"
#include "BaseMap.hpp"

struct Data_FP final
{
    FP data[2][5][5];
};

struct GasPolys final
{
    Poly_G4 polys[2][4][4];
};

struct Data_Byte final
{
    u8 data[2][5][5];
};

static Data_FP sDataX = {};
static Data_FP sDataY = {};

static GasPolys sGasPolys = {};
static Data_Byte sByte3 = {};
static Data_Byte sByte1 = {};
static Data_Byte sByte2 = {};

WorldStatic<s32> gDeathGasCount("DeathGas.gDeathGasCount", 0);

DeathGas::DeathGas(Layer layer, s32 amount, ResourceManager& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map),
    mLayer(layer),
    mAmount(amount)
{
    gDeathGasCount++;

    if (GetGameType() == GameType::eAo)
    {
        SetType(ReliveTypes::eFade); // wot moment
    }
    else
    {
        SetType(ReliveTypes::eMainMenuTransistion); // wot moment
    }

    gObjListDrawables->Push_Back(this);
    SetDrawable(true);

    for (s32 i = 0; i < 2; i++)
    {
        for (s32 k = 0; k < 4; k++)
        {
            for (s32 l = 0; l < 4; l++)
            {
                Poly_G4* pPoly = &sGasPolys.polys[i][k][l];
                pPoly->SetSemiTransparent(true);
                pPoly->SetBlendMode(relive::TBlendModes::eBlend_1);
            }
        }
    }

    for (s32 i = 0; i < 2; i++)
    {
        for (s32 j = 0; j < 5; j++)
        {
            for (s32 k = 0; k < 5; k++)
            {
                sByte1.data[i][j][k] = Math_NextRandom();
                sByte2.data[i][j][k] = (Math_NextRandom() & 3) + 2;
            }
        }
    }
}

DeathGas::DeathGas(RestoreTag, ResourceManager& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    if (GetGameType() == GameType::eAo)
    {
        SetType(ReliveTypes::eFade); // wot moment
    }
    else
    {
        SetType(ReliveTypes::eMainMenuTransistion); // wot moment
    }

    gObjListDrawables->Push_Back(this);
    SetDrawable(true);

    for (s32 i = 0; i < 2; i++)
    {
        for (s32 k = 0; k < 4; k++)
        {
            for (s32 l = 0; l < 4; l++)
            {
                Poly_G4* pPoly = &sGasPolys.polys[i][k][l];
                pPoly->SetSemiTransparent(true);
                pPoly->SetBlendMode(relive::TBlendModes::eBlend_1);
            }
        }
    }
}

DeathGas::~DeathGas()
{
    gObjListDrawables->Remove_Item(this);
    gDeathGasCount--;
}

void DeathGas::VScreenChanged()
{
    // TODO: remove after abe merge
    void* abePtr = GetGameType() == GameType::eAe ? static_cast<void*>(gAbe) : static_cast<void*>(AO::gAbe);
    if (mMap.LevelChanged() || mMap.PathChanged() || !abePtr)
    {
        SetDead(true);
    }
}

void DeathGas::VUpdate()
{
    if (EventGet(Event::kEventDeathReset))
    {
        SetDead(true);
    }

    if (!mDone)
    {
        mTotal += mAmount;

        if (mTotal > 255)
        {
            mTotal = 255;
        }
    }
}

void DeathGas::VRender(OrderingTable& ot)
{
    for (s32 i = 0; i < 2; i++)
    {
        for (s32 j = 0; j < 5; j++)
        {
            for (s32 k = 0; k < 5; k++)
            {
                sDataX.data[i][j][k] = FP_FromInteger(10) * Math_Cosine(sByte1.data[i][j][k]);
                sDataY.data[i][j][k] = FP_FromInteger(10) * Math_Sine(sByte1.data[i][j][k]);
                sByte1.data[i][j][k] += sByte2.data[i][j][k];
            }
        }
    }

    for (s32 i = 0; i < 5; i++)
    {
        for (s32 j = 0; j < 5; j++)
        {
            const FP cosVal = Math_Cosine(sByte1.data[0][i][j]);
            const FP sinVal = Math_Sine(sByte1.data[0][i][j]);
            const s16 cosMul10 = FP_GetExponent(FP_FromInteger(20) * cosVal);
            const s16 sinMul10 = FP_GetExponent(FP_FromInteger(20) * sinVal);

            s32 tableVal = 0;
            if ((i & 1) && (j & 1))
            {
                tableVal = std::abs(cosMul10) + 20;
            }
            else if (i & 1)
            {
                tableVal = std::abs(sinMul10) + 20;
            }
            else
            {
                tableVal = 20;
            }
            sByte3.data[0][i][j] = static_cast<u8>(tableVal);
        }
    }

    // Seems like a hacked copy paste job of the above with the sin value not being used and cos inverted ??
    for (s32 i = 0; i < 5; i++)
    {
        for (s32 j = 0; j < 5; j++)
        {
            const FP cosVal = Math_Cosine(sByte1.data[1][i][j]);
            const s16 cosMul10 = FP_GetExponent(FP_FromInteger(20) * cosVal);

            s32 tableVal = 0;
            if (!(i & 1) && (j & 1))
            {
                tableVal = std::abs(cosMul10) + 20;
            }
            else
            {
                tableVal = 20;
            }
            sByte3.data[1][i][j] = static_cast<u8>(tableVal);
        }
    }

    LOG("--- START ---");

    for (s32 i = 0; i < 2; i++)
    {
        for (s32 j = 0; j < 4; j++)
        {
            for (s32 k = 0; k < 4; k++)
            {
                Poly_G4* pPoly = &sGasPolys.polys[i][j][k];

                pPoly->SetRGB0(0, sByte3.data[i][j][k], 0);
                pPoly->SetRGB1(0, sByte3.data[i][j][k + 1], 0);

                if (j == 3)
                {
                    pPoly->SetRGB2(0, 0, 0);
                    pPoly->SetRGB3(0, 0, 0);
                }
                else
                {
                    pPoly->SetRGB2(0, sByte3.data[i][j + 1][k], 0);
                    pPoly->SetRGB3(0, sByte3.data[i][j + 1][k + 1], 0);
                }

                const s32 heightBase = (gPsxDisplay.mHeight + 56) / 4;

                const s32 height0 = ((j + 0) * heightBase) - 28 + (8 * k);
                const s32 height1 = ((j + 1) * heightBase) - 28 + (8 * k);

                const s32 width0 = ((gPsxDisplay.mWidth + 32) / 4) * (k + 0) - 16;
                const s32 width1 = ((gPsxDisplay.mWidth + 32) / 4) * (k + 1) - 16;


                s32 x0 = FP_GetExponent(sDataX.data[i][j][k]);
                s32 y0 = FP_GetExponent(sDataY.data[i][j][k]);

                s32 x1 = FP_GetExponent(sDataX.data[i][j][k + 1]);
                s32 y1 = FP_GetExponent(sDataY.data[i][j][k + 1]);

                s32 x2 = FP_GetExponent(sDataX.data[i][j + 1][k]);
                s32 y2 = FP_GetExponent(sDataY.data[i][j + 1][k]);

                s32 x3 = FP_GetExponent(sDataX.data[i][j + 1][k + 1]);
                s32 y3 = FP_GetExponent(sDataY.data[i][j + 1][k + 1]);

                x0 += width0;
                y0 += height0;

                x1 += width1;
                y1 += height0 + 8;

                x2 += width0;
                y2 += height1;

                x3 += width1;
                y3 += height1 + 8;

                const s32 yVal = (gPsxDisplay.mHeight + 28) * (255 - mTotal) / 255;

                pPoly->SetXY0( static_cast<s16>(x0), static_cast<s16>(y0 - yVal));
                pPoly->SetXY1( static_cast<s16>(x1), static_cast<s16>(y1 - yVal));
                pPoly->SetXY2( static_cast<s16>(x2), static_cast<s16>(y2 - yVal));
                pPoly->SetXY3( static_cast<s16>(x3), static_cast<s16>(y3 - yVal));

                LOG("adding death gas prim to OT: 0x%p", static_cast<void*>(pPoly));
                ot.Add(mLayer, pPoly);
            }
        }
    }

    if (mTotal >= 255)
    {
        mDone = true;
    }
}

void DeathGas::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    DeathGasSaveState data = {};
    data.mLayer = static_cast<s32>(mLayer);
    data.mAmount = mAmount;
    data.mTotal = mTotal;
    data.mDone = mDone;
    for (s32 i = 0; i < 50; i++)
    {
        data.mByte1[i] = sByte1.data[i / 25][(i / 5) % 5][i % 5];
    }
    for (s32 i = 0; i < 50; i++)
    {
        data.mByte2[i] = sByte2.data[i / 25][(i / 5) % 5][i % 5];
    }
    pSaveBuffer.Write(data);
}

void DeathGas::CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map)
{
    const DeathGasSaveState& state = *pData.ReadTmpPtr<DeathGasSaveState>();
    DeathGas* p = relive_new DeathGas(DeathGas::RestoreTag{}, resMan, map);
    if (!p)
    {
        return;
    }
    p->mLayer = static_cast<Layer>(state.mLayer);
    p->mAmount = state.mAmount;
    p->mTotal = state.mTotal;
    p->mDone = state.mDone;
    for (s32 i = 0; i < 50; i++)
    {
        sByte1.data[i / 25][(i / 5) % 5][i % 5] = state.mByte1[i];
    }
    for (s32 i = 0; i < 50; i++)
    {
        sByte2.data[i / 25][(i / 5) % 5][i % 5] = state.mByte2[i];
    }
}

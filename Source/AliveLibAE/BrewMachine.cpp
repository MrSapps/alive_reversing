#include "stdafx.h"
#include "../relive_lib/ObjectIds.hpp"
#include "../relive_lib/SerializedObjectData.hpp"
#include "BrewMachine.hpp"
#include "../relive_lib/Events.hpp"
#include "Map.hpp"
#include "Path.hpp"
#include "../relive_lib/GameObjects/ScreenManager.hpp"

BrewMachine::BrewMachine(relive::Path_BrewMachine* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map)
    : BaseAnimatedWithPhysicsGameObject(0, resMan, map),
    mTlvInfo(tlvId),
    mMaxBrewCount(pTlv->mBrewCount)
{
    SetType(ReliveTypes::eBrewMachine);

    mPal = mResMan.LoadPal(PalId::LedFont_Red);

    mFontContext.LoadFontType(FontType::LcdFont, mResMan);
    mFont.Load(3, mPal, &mFontContext);

    mLoadedAnims.push_back(mResMan.LoadAnimation(AnimId::BrewMachine_Button));
    Animation_Init(GetAnimRes(AnimId::BrewMachine_Button));

    SetApplyShadowZoneColour(false);
    GetAnimation().SetRenderLayer(Layer::eLayer_Well_23);

    const u8 savedBrewCount = pTlv->mTlvSpecificMeaning;
    if (savedBrewCount == 0)
    {
        mRemainingBrewCount = mMaxBrewCount;
    }
    else if (savedBrewCount > 30)
    {
        mRemainingBrewCount = 0;
    }
    else
    {
        mRemainingBrewCount = savedBrewCount;
    }

    mTextX = FP_GetExponent((FP_FromInteger(pTlv->mTopLeftX + 5) - gScreenManager->CamXPos()));
    mTextY = FP_GetExponent((FP_FromInteger(pTlv->mTopLeftY + 10) - gScreenManager->CamYPos()));
    mXPos = FP_FromInteger((pTlv->mTopLeftX + pTlv->mBottomRightX) / 2);
    mYPos = FP_FromInteger(pTlv->mTopLeftY);

    mBrewMachineCamera = mMap.mCurrentCamera;
}

std::vector<BaseGameObject::NotStateMemory> BrewMachine::VNotStateMemory()
{
    // The font and its palette are resources: ids of what was loaded, not state
    return {{&mFontContext, sizeof(mFontContext)}, {&mFont, sizeof(mFont)}, {&mPal, sizeof(mPal)}};
}

BrewMachine::~BrewMachine()
{
    mMap.TLV_Reset(mTlvInfo);
}

void BrewMachine::VUpdate()
{
    relive::Path_BrewMachine* pTlv = static_cast<relive::Path_BrewMachine*>(mMap.TLV_From_Offset_Lvl_Cam(mTlvInfo).GetTlv());
    if (mRemainingBrewCount > 0)
    {
        pTlv->mTlvSpecificMeaning = static_cast<u8>(mRemainingBrewCount);
    }
    else
    {
        pTlv->mTlvSpecificMeaning = 50;
    }

    if (EventGet(Event::kEventDeathReset))
    {
        SetDead(true);
    }
}

void BrewMachine::VRender(OrderingTable& ot)
{
    if (mMap.mCurrentCamera == mBrewMachineCamera)
    {
        char_type text[12] = {};
        sprintf(text, "%02d", mRemainingBrewCount);
        const s32 textWidth = mFont.MeasureTextWidth(text);
        s16 flickerAmount = 50;
        if (gDisableFontFlicker)
        {
            flickerAmount = 0;
        }

        mFont.DrawString(
            ot,
            text,
            mTextX,
            mTextY,
            relive::TBlendModes::eBlend_1,
            1,
            0,
            Layer::eLayer_BeforeWell_22,
            127,
            127,
            127,
            0,
            FP_FromInteger(1),
            mTextX + textWidth,
            flickerAmount);
    }

    BaseAnimatedWithPhysicsGameObject::VRender(ot);
}

void BrewMachine::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    BrewMachineSaveState data = {};
    data.mRemainingBrewCount = mRemainingBrewCount;
    data.mBrewMachineCamera = mBrewMachineCamera;
    data.mTextX = mTextX;
    data.mTextY = mTextY;
    pSaveBuffer.Write(data);
}

void BrewMachine::RestoreFromSaveState(const BrewMachineSaveState& state)
{
    BaseGameObject* pObj = sObjectIds.Find_Impl(state.mObjectId);
    if (!pObj || pObj->Type() != ReliveTypes::eBrewMachine)
    {
        return;
    }
    BrewMachine* p = static_cast<BrewMachine*>(pObj);
    p->mRemainingBrewCount = state.mRemainingBrewCount;
    p->mBrewMachineCamera = state.mBrewMachineCamera;
    p->mTextX = state.mTextX;
    p->mTextY = state.mTextY;
}

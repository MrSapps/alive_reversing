#include "stdafx.h"
#include "CreditsController.hpp"
#include "stdlib.hpp"
#include "Engine.hpp"
#include "Map.hpp"

s16 gCreditsControllerExists = false;

constexpr s32 kShowCreditScreenForTicks = 160;

CreditsController::CreditsController(relive::Path_TLV* /*pTlv*/, const Guid& /*tlvId*/, ResourceManagerWrapper& resMan, BaseMap& map)
   : BaseGameObject(true, 0, resMan, map)
{
    mCurrentCamera = GetMap().mCurrentCamera;
    mNextCameraTimer = MakeTimer(kShowCreditScreenForTicks);

    gCreditsControllerExists = true;
}

CreditsController::~CreditsController()
{
    gCreditsControllerExists = false;
}

void CreditsController::VUpdate()
{
    if (mNextCameraTimer <= static_cast<s32>(sGnFrame))
    {
        mCurrentCamera++;
        mNextCameraTimer = MakeTimer(kShowCreditScreenForTicks);

        if (GetMap().mCurrentPath == 2)
        {
            if (mCurrentCamera > 22)
            {
                mCurrentCamera = 1;
                GetMap().SetActiveCam(EReliveLevelIds::eCredits, 1, 1, CameraSwapEffects::eTopToBottom_3, 0, 0);
            }
            else
            {
                GetMap().SetActiveCam(EReliveLevelIds::eCredits, 2, mCurrentCamera, CameraSwapEffects::eTopToBottom_3, 0, 0);
            }
            GetMap().mFreeAllAnimAndPalts = true;
        }
        else
        {
            if (mCurrentCamera > 36)
            {
                GetMap().SetActiveCam(EReliveLevelIds::eMenu, 1, 1, CameraSwapEffects::eInstantChange_0, 0, 0);
            }
            else
            {
                GetMap().SetActiveCam(EReliveLevelIds::eCredits, 1, mCurrentCamera, CameraSwapEffects::eTopToBottom_3, 0, 0);
            }
            GetMap().mFreeAllAnimAndPalts = true;
        }
    }
}

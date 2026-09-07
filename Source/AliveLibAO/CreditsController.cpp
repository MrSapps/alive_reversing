#include "stdafx_ao.h"
#include "CreditsController.hpp"
#include "Map.hpp"
#include "Engine.hpp"
#include "../AliveLibAE/stdlib.hpp"

namespace AO {

s16 gCreditsControllerExists = false;

constexpr s32 kShowCreditScreenForTicks = 60;

CreditsController::CreditsController(relive::Path_CreditsController* /*pTlv*/, const Guid& /*tlvId*/, ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    mCurrentCamera = mMap.mCurrentCamera;
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
        if (mCurrentCamera > 24u)
        {
            mNextCameraTimer = MakeTimer(60);
            mCurrentCamera = 1;
            mMap.SetActiveCam(EReliveLevelIds::eCredits, 1, 1, CameraSwapEffects::eTopToBottom_3, 0, 0);
            gBreakGameLoop = true;
        }
        else
        {
            mMap.SetActiveCam(EReliveLevelIds::eCredits, 1, mCurrentCamera, CameraSwapEffects::eTopToBottom_3, 0, 0);
            mNextCameraTimer = MakeTimer(60);
        }
    }
}

} // namespace AO

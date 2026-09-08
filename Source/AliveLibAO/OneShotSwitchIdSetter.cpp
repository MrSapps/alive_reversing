#include "stdafx_ao.h"
#include "OneShotSwitchIdSetter.hpp"
#include "../relive_lib/SwitchStates.hpp"
#include "../AliveLibAE/stdlib.hpp"
#include "Map.hpp"

namespace AO {

OneShotSwitchIdSetter::~OneShotSwitchIdSetter()
{
    mMap.TLV_Reset(mTlvId);
}

OneShotSwitchIdSetter::OneShotSwitchIdSetter(Path_OneShotSwitchIdSetter* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    mTlvId = tlvId;

    if (!SwitchStates_Get(236))
    {
        for (auto id : pTlv->mIdsToClear)
        {
            SwitchStates_Do_Operation(id, relive::reliveSwitchOp::eSetTrue);
        }
        SwitchStates_Do_Operation(236, relive::reliveSwitchOp::eSetTrue);
    }
}

void OneShotSwitchIdSetter::VScreenChanged()
{
    SetDead(true);
}

} // namespace AO

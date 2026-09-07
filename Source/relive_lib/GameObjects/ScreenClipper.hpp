#pragma once

#include "BaseGameObject.hpp"
#include "../Primitives.hpp"
#include "../Layer.hpp"

class ScreenClipper final : public ::BaseGameObject
{
public:
    ScreenClipper(PSX_Point xy, PSX_Point wh, Layer layer, ResourceManagerWrapper& resMan, BaseMap& map);
    ~ScreenClipper();
    
    virtual void VRender(OrderingTable& ot) override;
    virtual void VScreenChanged() override;
    virtual void VUpdate() override;

    void Update_Clip_Rect(PSX_Point xy, PSX_Point wh);

private:
    Prim_ScissorRect mClipper = {};
    PSX_RECT mRect = {};

public:
    Layer mOtLayer = Layer::eLayer_0;
};

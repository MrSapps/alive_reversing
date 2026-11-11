#pragma once

#include "BaseGameObject.hpp"
#include "../SaveStateBase.hpp"
#include "../Primitives.hpp"
#include "../Layer.hpp"

struct ScreenClipperSaveState final : public SaveStateBase
{
    ScreenClipperSaveState()
        : SaveStateBase(ReliveTypes::eScreenClipper, sizeof(*this))
    { }
    s16 mRectX;
    s16 mRectY;
    s16 mRectW;
    s16 mRectH;
    s32 mOtLayer;
};

class ScreenClipper final : public ::BaseGameObject
{
public:
    // For a save: the rect and the layer are in the save
    struct RestoreTag final
    { };
    ScreenClipper(RestoreTag, ResourceManager& resMan, BaseMap& map);

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    ScreenClipper(PSX_Point xy, PSX_Point wh, Layer layer, ResourceManager& resMan, BaseMap& map);
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

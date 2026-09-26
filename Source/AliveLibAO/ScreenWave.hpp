#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/MapWrapper.hpp"
#include "../relive_lib/FixedPoint.hpp"
#include "../relive_lib/Layer.hpp"
#include "../relive_lib/Primitives.hpp"

enum class EReliveLevelIds : s16;

namespace AO {

// The ripple the bells send across the screen: a ring that grows out from where it starts,
// bending whatever is drawn under it
class ScreenWave final : public ::BaseGameObject
{
public:
    // width: how wide the ring is. speed: how far it grows each frame. radius: how far it goes,
    // or 0 to go until it has left the screen.
    ScreenWave(FP xpos, FP ypos, Layer layer, FP width, FP speed, s32 radius, ResourceManagerWrapper& resMan, BaseMap& map);
    ~ScreenWave();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;

private:
    // The ring is made of spokes going out from the centre, with points along each. Neighbouring
    // spokes' points make the quads, each drawing a piece of the frame from the source points onto
    // the destination points.
    static constexpr s32 kSpokes = 32;
    static constexpr s32 kPointsPerSpoke = 5;
    static constexpr s32 kQuadsPerSpoke = kPointsPerSpoke - 1;

    Layer mLayer = Layer::eLayer_0;
    FP mXPos = {};
    FP mYPos = {};
    EReliveLevelIds mLevel = {};
    s16 mPath = 0;

    // The centre, in screen coordinates
    s16 mScreenX = 0;
    s16 mScreenY = 0;

    FP mRadius = {};
    FP mSpeed = {};
    s16 mMaxRadius = 0;

    // Relative to the centre
    FP_Point mSource[kSpokes][kPointsPerSpoke] = {};
    FP_Point mDest[kSpokes][kPointsPerSpoke] = {};
    // How far each spoke's points move each frame
    FP_Point mVelocity[kSpokes] = {};

    Prim_ScreenWave mQuads[kSpokes][kQuadsPerSpoke];
};

} // namespace AO

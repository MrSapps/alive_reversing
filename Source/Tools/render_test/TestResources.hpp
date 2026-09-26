#pragma once

#include <memory>
#include <string>
#include <vector>
#include "../../relive_lib/ResourceManagerWrapper.hpp"
#include "../../relive_lib/data_conversion/rgb_conversion.hpp"

// Everything the render test draws, made in code so that it runs without any game data.
//
// The images are designed so that mistakes are easy to spot by eye: 1 pixel borders show up
// anything cut off or shifted, asymmetric glyphs show up wrong flips, and each sprite is half
// "always opaque" and half "semi-transparent" colours so blending mistakes stand out.
class TestResources final
{
public:
    // Also adds the animations real game objects load (zap lines, sparks) to resMan
    explicit TestResources(ResourceManagerWrapper& resMan);

    // Colour gradient with a 40 pixel grid, 1 pixel white border, coloured corner squares
    // (red top left, green top right, blue bottom left, white bottom right) and a 1 pixel
    // checkerboard in the middle
    CamResource mGridCam;

    // 8 vertical bands of known colours (black, grey 64, grey 128, white, red, green, blue,
    // gradient) to draw blended things over
    CamResource mBandsCam;

    // A scene with pillars, a wall and a well, and the FG1 layers that cut them back out so
    // objects can go behind them
    CamResource mFg1Cam;
    Fg1Resource mFg1;

    // 40x40, 8 frames: a white "F" (to check flips) on the left, red/green/blue on the
    // right, opaque in the top half and semi-transparent in the bottom half. A dark square
    // walks around the border, one step per frame.
    AnimResource mTestCard;

    // 16x16 soft dot, semi-transparent colours, for particles and stress tests
    AnimResource mParticle;

    // The test card's palette with the hues rotated, for palette swaps
    std::vector<std::shared_ptr<AnimationPal>> mPalettes;

    // A test card with its own texture, so the renderer has to upload it (texture churn test)
    AnimResource MakeUniqueTestCard() const;

    // Palette indices used by the generated sprites
    enum PaletteIndex : u8
    {
        eTransparent = 0,
        eWhite = 1,
        eNearBlack = 2,
        eOpaqueRed = 3,
        eOpaqueGreen = 4,
        eOpaqueBlue = 5,
        eSemiRed = 6,
        eSemiGreen = 7,
        eSemiBlue = 8,
        eMarker = 9,
        // 16 steps of a glow, dark to bright, all semi-transparent
        eGlowFirst = 16,
        eGlowLast = 31,
    };

    static RGBA32 Opaque(u8 r, u8 g, u8 b);
    static RGBA32 SemiTrans(u8 r, u8 g, u8 b);

private:
    std::shared_ptr<PngData> mTestCardPng;
    std::shared_ptr<AnimationAttributesAndFrames> mTestCardFrames;
};

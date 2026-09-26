#include "stdafx.h"
#include "Scene.hpp"
#include "TestResources.hpp"
#include "../../relive_lib/BaseMap.hpp"
#include "../../relive_lib/PsxDisplay.hpp"
#include "../../relive_lib/SwitchStates.hpp"
#include "../../relive_lib/data_conversion/guid.hpp"
#include "../../relive_lib/data_conversion/relive_tlvs.hpp"
#include "../../relive_lib/GameObjects/Fade.hpp"
#include "../../relive_lib/GameObjects/Flash.hpp"
#include "../../relive_lib/GameObjects/DeathGas.hpp"
#include "../../relive_lib/GameObjects/ScreenClipper.hpp"
#include "../../relive_lib/GameObjects/ScreenManager.hpp"
#include "../../relive_lib/GameObjects/ScreenShake.hpp"
#include "../../relive_lib/GameObjects/ZapLine.hpp"
#include "../../relive_lib/FG1.hpp"
#include "../../AliveLibAE/LaughingGas.hpp"
#include "../../AliveLibAE/MainMenuTransition.hpp"
#include "../../AliveLibAE/PsxRender.hpp"
#include "../../AliveLibAO/ScreenWave.hpp"
#include <cmath>

// Where scenes put things. The camera is on layer 1 and the HUD on 41 and 42.
static constexpr Layer kLayerBack = Layer::eLayer_BeforeWell_22;
static constexpr Layer kLayerMiddle = Layer::eLayer_Foreground_36;
static constexpr Layer kLayerFront = Layer::eLayer_Above_FG1_39;

// Scenes keep their content between the title (top line) and the explanation (bottom lines)
static constexpr s32 kContentTop = 14;

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

struct BlendChoice final
{
    const char* mLabel;
    bool mSemiTrans;
    relive::TBlendModes mMode;
};

// Each of the PSX's ways of drawing, in the order the blend mode scenes show them as rows
static const BlendChoice kBlendChoices[] = {
    {"Opaque", false, relive::TBlendModes::eBlend_0},
    {"B0", true, relive::TBlendModes::eBlend_0},
    {"B1", true, relive::TBlendModes::eBlend_1},
    {"B2", true, relive::TBlendModes::eBlend_2},
    {"B3", true, relive::TBlendModes::eBlend_3},
};

static void ApplyBlend(BasePrimitive& prim, const BlendChoice& blend)
{
    prim.SetSemiTransparent(blend.mSemiTrans);
    prim.SetBlendMode(blend.mMode);
}

static void ApplyBlend(Animation& anim, const BlendChoice& blend)
{
    anim.SetSemiTrans(blend.mSemiTrans);
    anim.SetBlendMode(blend.mMode);
}

static void SetColour(BasePrimitive& prim, u8 r, u8 g, u8 b)
{
    prim.SetRGB0(r, g, b);
    prim.SetRGB1(r, g, b);
    prim.SetRGB2(r, g, b);
    prim.SetRGB3(r, g, b);
}

static void SetBox(BasePrimitive& prim, s32 x, s32 y, s32 w, s32 h)
{
    prim.SetXYWH(static_cast<s16>(x), static_cast<s16>(y), static_cast<s16>(w), static_cast<s16>(h));
}

static void SetLine(BasePrimitive& line, s32 x0, s32 y0, s32 x1, s32 y1)
{
    line.SetXY0(static_cast<s16>(x0), static_cast<s16>(y0));
    line.SetXY1(static_cast<s16>(x1), static_cast<s16>(y1));
}

// A test card that stays on one frame unless it's asked to animate
static std::unique_ptr<SceneSprite> MakeCard(const AnimResource& res, s32 x, s32 y, bool animate = false)
{
    auto card = std::make_unique<SceneSprite>(res, x, y);
    card->mAnim.SetAnimate(animate);
    return card;
}

static void RenderAll(std::vector<std::unique_ptr<SceneSprite>>& sprites, OrderingTable& ot)
{
    for (auto& sprite : sprites)
    {
        sprite->Render(ot);
    }
}

// Game objects take world positions. These give the world position of a point on the
// screen, whichever game's camera placement is in use.
static FP WorldX(s32 screenX)
{
    return gScreenManager->CamXPos() + FP_FromInteger(PCToPsxX(screenX));
}

static FP WorldY(s32 screenY)
{
    return gScreenManager->CamYPos() + FP_FromInteger(screenY);
}

// Repeatable pseudo random numbers, so scenes look the same on every run and renderer
class SceneRandom final
{
public:
    explicit SceneRandom(u32 seed)
        : mState(seed)
    {
    }

    // 0 to max - 1
    s32 Next(s32 max)
    {
        mState = mState * 1664525u + 1013904223u;
        return static_cast<s32>((mState >> 8) % static_cast<u32>(max));
    }

private:
    u32 mState = 0;
};

// ----------------------------------------------------------------------------
// Reference frame
// ----------------------------------------------------------------------------

class ReferenceScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Reference frame";
    }

    const char* Expected() const override
    {
        return "Grid camera, a solid and a see-through grey box, three test cards, yellow text\n"
               "--auto draws this again after every scene: it must come out exactly the same";
    }

    bool IsStatic() const override
    {
        return true;
    }

    void Enter(SceneContext& ctx) override
    {
        SetBox(mSolid, 40, 40, 120, 60);
        SetColour(mSolid, 128, 128, 128);

        SetBox(mSeeThrough, 100, 70, 120, 60);
        SetColour(mSeeThrough, 128, 128, 128);
        ApplyBlend(mSeeThrough, kBlendChoices[1]);

        mCards.push_back(MakeCard(ctx.mRes.mTestCard, 320, 100));
        mCards.push_back(MakeCard(ctx.mRes.mTestCard, 400, 100));
        mCards.push_back(MakeCard(ctx.mRes.mTestCard, 480, 100));
        ApplyBlend(mCards[1]->mAnim, kBlendChoices[2]);
        mCards[2]->mAnim.SetFlipX(true);
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mGridCam);
        ot.Add(kLayerBack, &mSolid);
        ot.Add(kLayerMiddle, &mSeeThrough);
        RenderAll(mCards, ot);

        TextDrawer::Style yellow;
        yellow.r = 127;
        yellow.g = 127;
        yellow.b = 0;
        ctx.mText.Draw(ot, 320, 150, "REFERENCE", yellow);
    }

private:
    Poly_G4 mSolid;
    Poly_G4 mSeeThrough;
    std::vector<std::unique_ptr<SceneSprite>> mCards;
};

// ----------------------------------------------------------------------------
// Untextured primitives
// ----------------------------------------------------------------------------

class PolygonsScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Flat and gouraud polygons";
    }

    const char* Expected() const override
    {
        return "Top: red, green, blue boxes, then one shaded from red, green, blue and white corners\n"
               "Middle: yellow, shaded, thin and partly off screen triangles\n"
               "Bottom: 1, 2, 3, 4 and 8 pixel squares, each exactly as wide as the white ticks above it";
    }

    bool IsStatic() const override
    {
        return true;
    }

    void Enter(SceneContext& /*ctx*/) override
    {
        const u8 flat[3][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
        for (s32 i = 0; i < 3; i++)
        {
            SetBox(mBoxes[i], 20 + i * 150, kContentTop + 6, 130, 50);
            SetColour(mBoxes[i], flat[i][0], flat[i][1], flat[i][2]);
        }

        SetBox(mBoxes[3], 470, kContentTop + 6, 150, 50);
        mBoxes[3].SetRGB0(255, 0, 0);
        mBoxes[3].SetRGB1(0, 255, 0);
        mBoxes[3].SetRGB2(0, 0, 255);
        mBoxes[3].SetRGB3(255, 255, 255);

        const s32 triY = kContentTop + 66;
        SetTriangle(mTriangles[0], 20, triY + 60, 90, triY, 160, triY + 60);
        SetColour(mTriangles[0], 255, 255, 0);

        SetTriangle(mTriangles[1], 180, triY + 60, 250, triY, 320, triY + 60);
        mTriangles[1].SetRGB0(255, 0, 0);
        mTriangles[1].SetRGB1(0, 255, 0);
        mTriangles[1].SetRGB2(0, 0, 255);

        // Very thin: must still be drawn as a continuous sliver
        SetTriangle(mTriangles[2], 340, triY + 60, 470, triY, 342, triY + 60);
        SetColour(mTriangles[2], 255, 128, 0);

        // Mostly off the right hand edge
        SetTriangle(mTriangles[3], 560, triY, 760, triY + 30, 560, triY + 60);
        SetColour(mTriangles[3], 0, 255, 255);

        // Squares of exact sizes, with 1 pixel ticks marking their edges
        const s32 sizes[5] = {1, 2, 3, 4, 8};
        const s32 squareY = kContentTop + 150;
        for (s32 i = 0; i < 5; i++)
        {
            const s32 x = 40 + i * 60;
            SetBox(mSquares[i], x, squareY, sizes[i], sizes[i]);
            SetColour(mSquares[i], 255, 255, 255);

            // Ticks just left of and just right of where the square is
            SetBox(mTicks[i * 2], x - 1, squareY - 8, 1, 6);
            SetBox(mTicks[i * 2 + 1], x + sizes[i], squareY - 8, 1, 6);
            SetColour(mTicks[i * 2], 255, 255, 255);
            SetColour(mTicks[i * 2 + 1], 255, 255, 255);
        }
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawBackground(ot, 32, 32, 48);
        for (Poly_G4& box : mBoxes)
        {
            ot.Add(kLayerMiddle, &box);
        }
        for (Poly_G3& triangle : mTriangles)
        {
            ot.Add(kLayerMiddle, &triangle);
        }
        for (Poly_G4& square : mSquares)
        {
            ot.Add(kLayerMiddle, &square);
        }
        for (Poly_G4& tick : mTicks)
        {
            ot.Add(kLayerMiddle, &tick);
        }
    }

private:
    static void SetTriangle(Poly_G3& tri, s32 x0, s32 y0, s32 x1, s32 y1, s32 x2, s32 y2)
    {
        tri.SetXY0(static_cast<s16>(x0), static_cast<s16>(y0));
        tri.SetXY1(static_cast<s16>(x1), static_cast<s16>(y1));
        tri.SetXY2(static_cast<s16>(x2), static_cast<s16>(y2));
    }

    Poly_G4 mBoxes[4];
    Poly_G3 mTriangles[4];
    Poly_G4 mSquares[5];
    Poly_G4 mTicks[10];
};

class LinesScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Lines";
    }

    const char* Expected() const override
    {
        return "Left: star of 1 pixel lines, dark in the middle getting brighter outwards\n"
               "Middle: a white outline drawn with lines, exactly on the edge of the blue box\n"
               "Right: a zig zag polyline, and 1 pixel lines 1, 2 and 3 pixels apart";
    }

    bool IsStatic() const override
    {
        return true;
    }

    void Enter(SceneContext& /*ctx*/) override
    {
        const s32 centreX = 110;
        const s32 centreY = 112;
        for (s32 i = 0; i < kStarLines; i++)
        {
            const f32 angle = 6.2831853f * i / kStarLines;
            SetLine(mStar[i], centreX, centreY, centreX + static_cast<s32>(90 * std::cos(angle)), centreY + static_cast<s32>(85 * std::sin(angle)));
            mStar[i].SetRGB0(40, 40, 40);
            mStar[i].SetRGB1(static_cast<u8>(128 + 127 * std::cos(angle)), static_cast<u8>(128 + 127 * std::sin(angle)), 255);
        }

        SetBox(mBox, 240, 50, 140, 110);
        SetColour(mBox, 0, 0, 160);

        // Outline along the box's edge pixels: the box covers x 240..379, y 50..159
        SetLine(mOutline[0], 240, 50, 379, 50);
        SetLine(mOutline[1], 379, 50, 379, 159);
        SetLine(mOutline[2], 379, 159, 240, 159);
        SetLine(mOutline[3], 240, 159, 240, 50);
        for (Line_G2& line : mOutline)
        {
            SetColour(line, 255, 255, 255);
        }

        mZigZag.SetXY0(420, 60);
        mZigZag.SetXY1(470, 120);
        mZigZag.SetXY2(520, 60);
        mZigZag.SetXY3(570, 120);
        mZigZag.SetRGB0(255, 0, 0);
        mZigZag.SetRGB1(255, 255, 0);
        mZigZag.SetRGB2(0, 255, 0);
        mZigZag.SetRGB3(0, 255, 255);

        // Pairs of horizontal lines with gaps of 0, 1 and 2 pixels
        for (s32 i = 0; i < 3; i++)
        {
            const s32 y = 150 + i * 12;
            SetLine(mSpaced[i * 2], 420, y, 600, y);
            SetLine(mSpaced[i * 2 + 1], 420, y + i + 1, 600, y + i + 1);
            SetColour(mSpaced[i * 2], 255, 255, 255);
            SetColour(mSpaced[i * 2 + 1], 255, 255, 255);
        }
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawBackground(ot, 32, 32, 48);
        ot.Add(kLayerBack, &mBox);
        for (Line_G2& line : mStar)
        {
            ot.Add(kLayerMiddle, &line);
        }
        for (Line_G2& line : mOutline)
        {
            ot.Add(kLayerMiddle, &line);
        }
        ot.Add(kLayerMiddle, &mZigZag);
        for (Line_G2& line : mSpaced)
        {
            ot.Add(kLayerMiddle, &line);
        }
    }

private:
    static constexpr s32 kStarLines = 32;
    Line_G2 mStar[kStarLines];
    Poly_G4 mBox;
    Line_G2 mOutline[4];
    Line_G4 mZigZag;
    Line_G2 mSpaced[6];
};

// ----------------------------------------------------------------------------
// Blend modes
// ----------------------------------------------------------------------------

class BlendFlatScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Blend modes: polygons, triangles and lines";
    }

    const char* Expected() const override
    {
        return "Rows: opaque, B0 half and half, B1 add, B2 subtract, B3 add a quarter, over the colour bands\n"
               "Grey box, red triangles, green lines. B1 on white stays white, B2 on black stays black";
    }

    bool IsStatic() const override
    {
        return true;
    }

    void Enter(SceneContext& /*ctx*/) override
    {
        for (s32 row = 0; row < kRows; row++)
        {
            const BlendChoice& blend = kBlendChoices[row];
            const s32 top = kContentTop + 4 + row * 40;

            SetBox(mBoxes[row], 84, top, 552, 16);
            SetColour(mBoxes[row], 128, 128, 128);
            ApplyBlend(mBoxes[row], blend);

            for (s32 i = 0; i < kTrianglesPerRow; i++)
            {
                Poly_G3& tri = mTriangles[row][i];
                const s32 x = 84 + i * 46;
                tri.SetXY0(static_cast<s16>(x), static_cast<s16>(top + 30));
                tri.SetXY1(static_cast<s16>(x + 23), static_cast<s16>(top + 18));
                tri.SetXY2(static_cast<s16>(x + 46), static_cast<s16>(top + 30));
                SetColour(tri, 200, 40, 40);
                ApplyBlend(tri, blend);
            }

            for (s32 i = 0; i < kLinesPerRow; i++)
            {
                Line_G2& line = mLines[row][i];
                SetLine(line, 84, top + 32 + i * 2, 635, top + 32 + i * 2);
                SetColour(line, 40, 200, 40);
                ApplyBlend(line, blend);
            }
        }
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mBandsCam);
        for (s32 row = 0; row < kRows; row++)
        {
            ot.Add(kLayerMiddle, &mBoxes[row]);
            for (Poly_G3& tri : mTriangles[row])
            {
                ot.Add(kLayerMiddle, &tri);
            }
            for (Line_G2& line : mLines[row])
            {
                ot.Add(kLayerMiddle, &line);
            }
            ctx.mText.Draw(ot, 4, kContentTop + 8 + row * 40, kBlendChoices[row].mLabel);
        }
    }

private:
    static constexpr s32 kRows = 5;
    static constexpr s32 kTrianglesPerRow = 12;
    static constexpr s32 kLinesPerRow = 3;
    Poly_G4 mBoxes[kRows];
    Poly_G3 mTriangles[kRows][kTrianglesPerRow];
    Line_G2 mLines[kRows][kLinesPerRow];
};

class BlendSpritesScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Blend modes: sprites";
    }

    const char* Expected() const override
    {
        return "Rows: opaque, B0, B1, B2, B3. The top half of each card never blends, the bottom half\n"
               "blends like the polygon rows did. The band behind shows through the card's 1 pixel edge";
    }

    bool IsStatic() const override
    {
        return true;
    }

    void Enter(SceneContext& ctx) override
    {
        for (s32 row = 0; row < 5; row++)
        {
            for (s32 band = 0; band < 8; band++)
            {
                auto card = MakeCard(ctx.mRes.mTestCard, band == 0 ? 52 : 40 + band * 80, kContentTop + 20 + row * 41);
                ApplyBlend(card->mAnim, kBlendChoices[row]);
                mCards.push_back(std::move(card));
            }
        }
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mBandsCam);
        RenderAll(mCards, ot);
        for (s32 row = 0; row < 5; row++)
        {
            const char* label = row == 0 ? "Op" : kBlendChoices[row].mLabel;
            ctx.mText.Draw(ot, 2, kContentTop + 16 + row * 41, label);
        }
    }

private:
    std::vector<std::unique_ptr<SceneSprite>> mCards;
};

class BlendTextScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Blend modes: text";
    }

    const char* Expected() const override
    {
        return "Rows: opaque, B0, B1, B2, B3 text, in white over the colour bands. The debug font has no\n"
               "semi-transparent pixels, and only those blend: every row must look the same, solid white";
    }

    bool IsStatic() const override
    {
        return true;
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mBandsCam);
        for (s32 row = 0; row < 5; row++)
        {
            TextDrawer::Style style;
            style.r = 127;
            style.g = 127;
            style.b = 127;
            style.semiTrans = kBlendChoices[row].mSemiTrans;
            style.blendMode = kBlendChoices[row].mMode;
            style.scale = FP_FromInteger(2);
            style.layer = kLayerMiddle;

            const s32 y = kContentTop + 10 + row * 38;
            ctx.mText.Draw(ot, 4, y, kBlendChoices[row].mLabel);
            ctx.mText.Draw(ot, 90, y, "THE QUICK BROWN FOX JUMPS OVER 0123", style);
        }
    }
};

// ----------------------------------------------------------------------------
// Textures
// ----------------------------------------------------------------------------

class CameraScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Camera";
    }

    const char* Expected() const override
    {
        return "1 pixel white border on all four edges (H hides this text). Corners: red top left,\n"
               "green top right, blue bottom left, white bottom right. Sharp checkerboard on the left";
    }

    bool IsStatic() const override
    {
        return true;
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mGridCam);
    }
};

class SpriteShadingScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Sprite and text shading";
    }

    const char* Expected() const override
    {
        return "Left to right: normal, twice as bright, half as bright, red, green and blue tints, black\n"
               "The last has shading off (SetBlending(true)): normal colours, though its tint is black";
    }

    bool IsStatic() const override
    {
        return true;
    }

    void Enter(SceneContext& ctx) override
    {
        for (s32 i = 0; i < kTints; i++)
        {
            auto card = MakeCard(ctx.mRes.mTestCard, 60 + i * 72, 80);
            card->mAnim.SetRGB(kTintColours[i][0], kTintColours[i][1], kTintColours[i][2]);

            // Despite the name, SetBlending(false) is what makes the tint apply, as game objects do
            card->mAnim.SetBlending(i == kTints - 1);
            mCards.push_back(std::move(card));
        }
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mGridCam);
        RenderAll(mCards, ot);

        for (s32 i = 0; i < kTints; i++)
        {
            TextDrawer::Style style;
            style.r = static_cast<u8>(kTintColours[i][0]);
            style.g = static_cast<u8>(kTintColours[i][1]);
            style.b = static_cast<u8>(kTintColours[i][2]);
            style.layer = kLayerMiddle;
            ctx.mText.Draw(ot, 40 + i * 72, 130, "Tint", style);
        }
    }

private:
    static constexpr s32 kTints = 8;
    static constexpr s16 kTintColours[kTints][3] = {
        {127, 127, 127},
        {255, 255, 255},
        {64, 64, 64},
        {255, 64, 64},
        {64, 255, 64},
        {64, 64, 255},
        {0, 0, 0},
        {0, 0, 0},
    };
    std::vector<std::unique_ptr<SceneSprite>> mCards;
};

class SpriteTransformScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Sprites: flips, scale, animation, screen edges";
    }

    const char* Expected() const override
    {
        return "Top: normal F, mirrored, upside down, both, half size, half size mirrored\n"
               "The dark square goes round the middle cards: clockwise, fast, slow, anticlockwise\n"
               "Cards on the screen edges are cut off cleanly";
    }

    void Enter(SceneContext& ctx) override
    {
        const AnimResource& res = ctx.mRes.mTestCard;
        const s32 y = kContentTop + 30;

        mCards.push_back(MakeCard(res, 70, y));
        mCards.push_back(MakeCard(res, 150, y));
        mCards.back()->mAnim.SetFlipX(true);
        mCards.push_back(MakeCard(res, 230, y));
        mCards.back()->mAnim.SetFlipY(true);
        mCards.push_back(MakeCard(res, 310, y));
        mCards.back()->mAnim.SetFlipX(true);
        mCards.back()->mAnim.SetFlipY(true);
        mCards.push_back(MakeCard(res, 390, y));
        mCards.back()->mAnim.SetSpriteScale(FP_FromDouble(0.5));
        mCards.push_back(MakeCard(res, 470, y));
        mCards.back()->mAnim.SetSpriteScale(FP_FromDouble(0.5));
        mCards.back()->mAnim.SetFlipX(true);

        // Animated
        const s32 animY = kContentTop + 95;
        const u32 delays[4] = {4, 1, 12, 4};
        for (s32 i = 0; i < 4; i++)
        {
            auto card = MakeCard(res, 190 + i * 80, animY, true);
            card->mAnim.SetFrameDelay(delays[i]);
            card->mAnim.SetLoopBackwards(i == 3);
            mCards.push_back(std::move(card));
        }

        // Half off each edge
        mCards.push_back(MakeCard(res, 0, animY));
        mCards.push_back(MakeCard(res, 640, animY));
        mCards.push_back(MakeCard(res, 560, 0));
        mCards.push_back(MakeCard(res, 80, 240));
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mGridCam);
        RenderAll(mCards, ot);
    }

private:
    std::vector<std::unique_ptr<SceneSprite>> mCards;
};

class PalettesScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Palettes";
    }

    const char* Expected() const override
    {
        return "32 copies of the test card, each with its own palette: the colours shift smoothly\n"
               "along both rows. The card below changes palette every frame, cycling through all 64";
    }

    void Enter(SceneContext& ctx) override
    {
        for (s32 i = 0; i < 32; i++)
        {
            auto card = MakeCard(ctx.mRes.mTestCard, 20 + (i % 16) * 40, kContentTop + 30 + (i / 16) * 50);
            std::shared_ptr<AnimationPal> pal = ctx.mRes.mPalettes[i * 2];
            card->mAnim.LoadPal(pal);
            mCards.push_back(std::move(card));
        }

        mCycling = MakeCard(ctx.mRes.mTestCard, 320, kContentTop + 150);
    }

    void Update(SceneContext& ctx, u32 frame) override
    {
        std::shared_ptr<AnimationPal> pal = ctx.mRes.mPalettes[frame % ctx.mRes.mPalettes.size()];
        mCycling->mAnim.LoadPal(pal);
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mGridCam);
        RenderAll(mCards, ot);
        mCycling->Render(ot);
    }

private:
    std::vector<std::unique_ptr<SceneSprite>> mCards;
    std::unique_ptr<SceneSprite> mCycling;
};

class TextScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Text";
    }

    const char* Expected() const override
    {
        return "Every character of the debug font, then coloured text, then text at half, normal\n"
               "and double size. Letters are sharp, evenly spaced and don't overlap";
    }

    bool IsStatic() const override
    {
        return true;
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawBackground(ot, 32, 32, 48);

        char line[64] = {};
        for (s32 row = 0; row < 3; row++)
        {
            s32 len = 0;
            for (s32 c = 33 + row * 32; c < 33 + (row + 1) * 32 && c < 127; c++)
            {
                line[len++] = static_cast<char>(c);
            }
            line[len] = 0;
            ctx.mText.Draw(ot, 20, kContentTop + 10 + row * 12, line);
        }

        const u8 colours[6][3] = {{127, 0, 0}, {0, 127, 0}, {0, 0, 127}, {127, 127, 0}, {0, 127, 127}, {127, 0, 127}};
        const char* names[6] = {"RED", "GREEN", "BLUE", "YELLOW", "CYAN", "MAGENTA"};
        for (s32 i = 0; i < 6; i++)
        {
            TextDrawer::Style style;
            style.r = colours[i][0];
            style.g = colours[i][1];
            style.b = colours[i][2];
            ctx.mText.Draw(ot, 20 + i * 100, kContentTop + 60, names[i], style);
        }

        const f64 scales[3] = {0.5, 1.0, 2.0};
        s32 y = kContentTop + 85;
        for (f64 scale : scales)
        {
            TextDrawer::Style style;
            style.scale = FP_FromDouble(scale);
            ctx.mText.Draw(ot, 20, y, "Abe's Exoddus 1998 - Mudokons: 99", style);
            y += static_cast<s32>(12 * scale) + 6;
        }
    }
};

class Fg1Scene final : public Scene
{
public:
    const char* Title() const override
    {
        return "FG1 foreground layers";
    }

    const char* Expected() const override
    {
        return "Cards go behind the stone pillars, except the middle row which is in front of everything\n"
               "Top row: behind the green well, in front of the purple one\n"
               "Bottom: one card is cut off by the brick wall (only its top shows), the other is in front";
    }

    void Enter(SceneContext& ctx) override
    {
        Fg1Resource fg1 = ctx.mRes.mFg1;
        CamResource cam = ctx.mRes.mFg1Cam;
        relive_new FG1(fg1, cam, ctx.mResMan, ctx.mMap);

        // Row, layer and colour of each card
        AddCard(ctx, 70, Layer::eLayer_BeforeWell_22, 0, 0);
        AddCard(ctx, 125, Layer::eLayer_Above_FG1_39, 0, 200);
        AddCard(ctx, 196, Layer::eLayer_Foreground_Half_17, 40, 0);
        AddCard(ctx, 196, Layer::eLayer_Foreground_36, 0, 350);
    }

    void Update(SceneContext& /*ctx*/, u32 frame) override
    {
        for (Card& card : mCards)
        {
            card.mSprite->mX = static_cast<s32>((frame * 3 + card.mPhase) % 720) - 40;
        }
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mFg1Cam);
        for (Card& card : mCards)
        {
            card.mSprite->Render(ot);
        }
    }

private:
    void AddCard(SceneContext& ctx, s32 y, Layer layer, s32 paletteIdx, u32 phase)
    {
        Card card;
        card.mSprite = MakeCard(ctx.mRes.mTestCard, 0, y);
        card.mSprite->mAnim.SetRenderLayer(layer);
        std::shared_ptr<AnimationPal> pal = ctx.mRes.mPalettes[paletteIdx];
        card.mSprite->mAnim.LoadPal(pal);
        card.mPhase = phase;
        mCards.push_back(std::move(card));
    }

    struct Card final
    {
        std::unique_ptr<SceneSprite> mSprite;
        u32 mPhase = 0;
    };
    std::vector<Card> mCards;
};

// ----------------------------------------------------------------------------
// Clipping
// ----------------------------------------------------------------------------

class ScissorScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Clip rectangles (scissor)";
    }

    const char* Expected() const override
    {
        return "Left box: a shaded background and cards, cut off exactly at the white outline\n"
               "Right box: text, lines and cards cut off at its outline. Nothing outside either box";
    }

    bool IsStatic() const override
    {
        return true;
    }

    void Enter(SceneContext& ctx) override
    {
        // Real ScreenClippers: each one clips everything drawn after it in the frame. That's
        // the layers above it: within a layer the ordering table draws what was added last
        // first, and scenes add their primitives after the game objects.
        relive_new ScreenClipper({kLeft.x, kLeft.y}, {kLeft.w, kLeft.h}, Layer::eLayer_Foreground_Half_17, ctx.mResMan, ctx.mMap);
        relive_new ScreenClipper({0, 0}, {1, 1}, Layer::eLayer_27, ctx.mResMan, ctx.mMap);
        relive_new ScreenClipper({kRight.x, kRight.y}, {kRight.w, kRight.h}, Layer::eLayer_ZapLinesElumMuds_28, ctx.mResMan, ctx.mMap);
        relive_new ScreenClipper({0, 0}, {1, 1}, Layer::eLayer_FadeFlash_40, ctx.mResMan, ctx.mMap);

        // Left box content
        SetBox(mShaded, 0, 0, 640, 240);
        mShaded.SetRGB0(255, 0, 0);
        mShaded.SetRGB1(0, 255, 0);
        mShaded.SetRGB2(0, 0, 255);
        mShaded.SetRGB3(255, 255, 0);
        for (s32 i = 0; i < 4; i++)
        {
            mCards.push_back(MakeCard(ctx.mRes.mTestCard, 30 + i * 90, kLeft.y + (i % 2) * 90));
            mCards.back()->mAnim.SetRenderLayer(kLeftLayer);
        }

        // Right box content
        for (s32 i = 0; i < 12; i++)
        {
            SetLine(mLines[i], 300, kContentTop + i * 16, 640, kContentTop + 40 + i * 16);
            SetColour(mLines[i], 255, 255, 255);
        }
        for (s32 i = 0; i < 3; i++)
        {
            mCards.push_back(MakeCard(ctx.mRes.mTestCard, kRight.x + i * 110, kRight.h - 10));
            mCards.back()->mAnim.SetRenderLayer(kRightLayer);
        }

        SetOutline(mOutline, kLeft);
        SetOutline(mOutline + 4, kRight);
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mGridCam);
        ot.Add(kLeftBackLayer, &mShaded);
        RenderAll(mCards, ot);

        for (Line_G2& line : mLines)
        {
            ot.Add(kRightLayer, &line);
        }

        TextDrawer::Style text;
        text.layer = kRightLayer;
        text.scale = FP_FromInteger(2);
        for (s32 i = 0; i < 6; i++)
        {
            ctx.mText.Draw(ot, 300, kRight.y - 10 + i * 30, "CLIPPED TEXT CLIPPED TEXT", text);
        }

        // Drawn after the clip is reset
        for (Line_G2& line : mOutline)
        {
            ot.Add(Layer::eLayer_Menu_41, &line);
        }
    }

private:
    // As ScreenClipper takes them: top left and bottom right
    struct Corners final
    {
        s16 x, y, w, h;
    };
    static constexpr Corners kLeft = {20, 40, 300, 170};
    static constexpr Corners kRight = {360, 50, 620, 190};

    // Above each box's clipper
    static constexpr Layer kLeftBackLayer = Layer::eLayer_FG1_Half_18;
    static constexpr Layer kLeftLayer = Layer::eLayer_Above_FG1_Half_20;
    static constexpr Layer kRightLayer = Layer::eLayer_BirdPortal_29;

    // Just outside the clipped area, so the outline is next to the last pixels drawn
    static void SetOutline(Line_G2* lines, const Corners& c)
    {
        SetLine(lines[0], c.x - 1, c.y - 1, c.w, c.y - 1);
        SetLine(lines[1], c.w, c.y - 1, c.w, c.h);
        SetLine(lines[2], c.w, c.h, c.x - 1, c.h);
        SetLine(lines[3], c.x - 1, c.h, c.x - 1, c.y - 1);
        for (s32 i = 0; i < 4; i++)
        {
            SetColour(lines[i], 255, 255, 255);
        }
    }

    Poly_G4 mShaded;
    Line_G2 mLines[12];
    Line_G2 mOutline[8];
    std::vector<std::unique_ptr<SceneSprite>> mCards;
};

class ClipCarryOverScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Clip rectangle set at the end of a frame";
    }

    const char* Expected() const override
    {
        return "The whole grid camera, nothing cut off. A small clip rectangle is set after everything\n"
               "else is drawn, as a ScreenClipper can: it must not carry over into the next frame";
    }

    bool IsStatic() const override
    {
        return true;
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mGridCam);

        PSX_RECT rect = {};
        rect.x = 200;
        rect.y = 60;
        rect.w = 120;
        rect.h = 60;
        mClip.SetRect(rect);

        // After the HUD, which is on the last named layer
        ot.Add(static_cast<Layer>(static_cast<s16>(Layer::eLayer_Text_42) + 1), &mClip);
    }

private:
    Prim_ScissorRect mClip;
};

// ----------------------------------------------------------------------------
// Effects, all made by the real game objects
// ----------------------------------------------------------------------------

class ScreenShakeScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Screen shake";
    }

    const char* Expected() const override
    {
        return "The whole picture shakes in bursts, showing black at the edges it moves away from\n"
               "(Captures don't show it: the shake is applied when the frame is drawn to the window)";
    }

    void Update(SceneContext& ctx, u32 frame) override
    {
        if (frame % 60 == 0)
        {
            relive_new ScreenShake(false, false, ctx.mResMan, ctx.mMap);
        }
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mGridCam);
    }
};

class LaughingGasScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Laughing gas";
    }

    const char* Expected() const override
    {
        return "Swirling clouds of yellow gas over the whole camera, which still shows through them";
    }

    void Enter(SceneContext& ctx) override
    {
        constexpr s16 kSwitchId = 100;
        SwitchStates_Set(kSwitchId, 1);

        // Covering the camera
        relive::Path_LaughingGas tlv;
        tlv.mLaughingGasSwitchId = kSwitchId;
        tlv.mTopLeftX = static_cast<s16>(FP_GetExponent(WorldX(0)));
        tlv.mTopLeftY = static_cast<s16>(FP_GetExponent(WorldY(0)));
        tlv.mBottomRightX = static_cast<s16>(FP_GetExponent(WorldX(640)));
        tlv.mBottomRightY = static_cast<s16>(FP_GetExponent(WorldY(240)));
        relive_new LaughingGas(Layer::eLayer_Above_FG1_39, &tlv, Guid{}, ctx.mResMan, ctx.mMap);
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mGridCam);
    }
};

class ScreenWaveScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Screen wave (AO bell song)";
    }

    const char* Expected() const override
    {
        return "Rings ripple out from the middle, bending the grid lines as they pass.\n"
               "No black gaps or seams appear between the pieces of the ripple";
    }

    void Update(SceneContext& ctx, u32 frame) override
    {
        if (frame % 45 == 0)
        {
            relive_new AO::ScreenWave(WorldX(320), WorldY(120), Layer::eLayer_FG1_37, FP_FromInteger(18), FP_FromInteger(12), 0, ctx.mResMan, ctx.mMap);
        }
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mGridCam);
    }

    u32 CaptureFrame() const override
    {
        return 55;
    }
};

class FadeAndFlashScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Fades and flashes";
    }

    const char* Expected() const override
    {
        return "Repeats: fade in from black (B2 subtract), white strobe (B1 add),\n"
               "red strobe (B3 add a quarter), grey veil (B0 half and half)";
    }

    void Update(SceneContext& ctx, u32 frame) override
    {
        const u32 phase = (frame / kPhaseFrames) % 4;
        const u32 phaseFrame = frame % kPhaseFrames;

        if (phaseFrame == 0 && mFade)
        {
            mFade->SetDead(true);
            mFade = nullptr;
        }

        switch (phase)
        {
            case 0:
                if (phaseFrame == 0)
                {
                    mFade = relive_new Fade(Layer::eLayer_FadeFlash_40, FadeOptions::eFadeOut, false, 8, relive::TBlendModes::eBlend_2, ctx.mResMan, ctx.mMap);
                }
                break;

            case 1:
                if (phaseFrame % 6 == 0)
                {
                    relive_new Flash(Layer::eLayer_FadeFlash_40, 255, 255, 255, ctx.mResMan, ctx.mMap, relive::TBlendModes::eBlend_1, 2);
                }
                break;

            case 2:
                if (phaseFrame % 6 == 0)
                {
                    relive_new Flash(Layer::eLayer_FadeFlash_40, 255, 0, 0, ctx.mResMan, ctx.mMap, relive::TBlendModes::eBlend_3, 2);
                }
                break;

            case 3:
                relive_new Flash(Layer::eLayer_FadeFlash_40, 128, 128, 128, ctx.mResMan, ctx.mMap, relive::TBlendModes::eBlend_0, 0);
                break;
        }
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mGridCam);
    }

    u32 CaptureFrame() const override
    {
        // Part way through the fade
        return 12;
    }

private:
    static constexpr u32 kPhaseFrames = 36;
    // Only kept until the runner destroys it, see Update
    Fade* mFade = nullptr;
};

class MenuTransitionScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Main menu transition";
    }

    const char* Expected() const override
    {
        return "A spinning star of shaded triangles (added with B1) shrinks into the middle, repeatedly";
    }

    void Update(SceneContext& ctx, u32 frame) override
    {
        if (frame % 30 == 0)
        {
            // Fading out: the fade in plays a sound, and there's no sound here
            relive_new MainMenuTransition(Layer::eLayer_FadeFlash_40, 0, true, 16, relive::TBlendModes::eBlend_1, ctx.mResMan, ctx.mMap);
        }
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mGridCam);
    }

    u32 CaptureFrame() const override
    {
        return 35;
    }
};

class DeathGasScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Death gas";
    }

    const char* Expected() const override
    {
        return "Faint green gas slowly thickens over the camera, swirling";
    }

    void Enter(SceneContext& ctx) override
    {
        relive_new DeathGas(Layer::eLayer_Above_FG1_39, 2, ctx.mResMan, ctx.mMap);
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mGridCam);
    }

    u32 CaptureFrame() const override
    {
        // Full strength
        return 140;
    }
};

class ZapLinesScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Zap lines";
    }

    const char* Expected() const override
    {
        return "Top: thin flickering blue zaps, added faintly (B3)\n"
               "Bottom: thick red zaps, added fully (B1). Each is a few hundred small sprites";
    }

    void Enter(SceneContext& ctx) override
    {
        MakeZap(ctx, 60, 50, 580, 70, ZapLineType::eThin_1);
        MakeZap(ctx, 60, 90, 580, 60, ZapLineType::eThin_1);
        MakeZap(ctx, 60, 140, 580, 170, ZapLineType::eThick_0);
        MakeZap(ctx, 60, 190, 580, 150, ZapLineType::eThick_0);
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mGridCam);
    }

    static void MakeZap(SceneContext& ctx, s32 x0, s32 y0, s32 x1, s32 y1, ZapLineType type)
    {
        constexpr s32 kForever = 30000;
        relive_new ZapLine(WorldX(x0), WorldY(y0), WorldX(x1), WorldY(y1), kForever, type, Layer::eLayer_ZapLinesElumMuds_28, ctx.mResMan, ctx.mMap);
    }
};

// ----------------------------------------------------------------------------
// Stress tests: look at the frame times more than the picture
// ----------------------------------------------------------------------------

class StressSpritesScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Stress: 5000 sprites";
    }

    const char* Expected() const override
    {
        return "5000 glowing dots (added with B1) and test cards (B0) bouncing round the screen.\n"
               "Zap lines use hundreds of sprites; this is more than the game ever draws";
    }

    bool IsStress() const override
    {
        return true;
    }

    void Enter(SceneContext& ctx) override
    {
        SceneRandom random(1234);
        for (s32 i = 0; i < kCount; i++)
        {
            const bool isCard = (i % 20) == 0;
            Mover mover;
            mover.mSprite = MakeCard(isCard ? ctx.mRes.mTestCard : ctx.mRes.mParticle, random.Next(640), kContentTop + random.Next(200), isCard);
            ApplyBlend(mover.mSprite->mAnim, isCard ? kBlendChoices[1] : kBlendChoices[2]);
            mover.mDx = random.Next(7) - 3;
            mover.mDy = random.Next(5) - 2;
            mMovers.push_back(std::move(mover));
        }
    }

    void Update(SceneContext& /*ctx*/, u32 /*frame*/) override
    {
        for (Mover& mover : mMovers)
        {
            SceneSprite& sprite = *mover.mSprite;
            sprite.mX += mover.mDx;
            sprite.mY += mover.mDy;
            if (sprite.mX < 0 || sprite.mX > 640)
            {
                mover.mDx = -mover.mDx;
            }
            if (sprite.mY < kContentTop || sprite.mY > 214)
            {
                mover.mDy = -mover.mDy;
            }
        }
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mGridCam);
        for (Mover& mover : mMovers)
        {
            mover.mSprite->Render(ot);
        }
    }

private:
    static constexpr s32 kCount = 5000;
    struct Mover final
    {
        std::unique_ptr<SceneSprite> mSprite;
        s32 mDx = 0;
        s32 mDy = 0;
    };
    std::vector<Mover> mMovers;
};

class StressZapLinesScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Stress: 30 zap lines";
    }

    const char* Expected() const override
    {
        return "30 real zap lines at once, about 7800 sprites, criss crossing the screen";
    }

    bool IsStress() const override
    {
        return true;
    }

    void Enter(SceneContext& ctx) override
    {
        SceneRandom random(99);
        for (s32 i = 0; i < 30; i++)
        {
            ZapLinesScene::MakeZap(ctx, random.Next(640), kContentTop + random.Next(200), random.Next(640), kContentTop + random.Next(200), i % 2 ? ZapLineType::eThin_1 : ZapLineType::eThick_0);
        }
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mGridCam);
    }
};

class StressStateChangesScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Stress: state changes";
    }

    const char* Expected() const override
    {
        return "Boxes, sprites and lines in a grid, alternating blend modes, 24 different textures\n"
               "and clip rectangles: the worst case for batching. Every cell is drawn, none are missing";
    }

    bool IsStress() const override
    {
        return true;
    }

    bool IsStatic() const override
    {
        return true;
    }

    void Enter(SceneContext& ctx) override
    {
        // More textures than the OpenGL renderer can bind for one batch
        for (s32 i = 0; i < kTextures; i++)
        {
            mTextures.push_back(ctx.mRes.MakeUniqueTestCard());
        }

        for (s32 i = 0; i < kCells; i++)
        {
            const s32 x = (i % kColumns) * 16;
            const s32 y = kContentTop + (i / kColumns) * 16;
            const BlendChoice& blend = kBlendChoices[(i / 4) % 5];

            switch (i % 4)
            {
                case 0:
                {
                    Poly_G4& box = mBoxes[i / 4];
                    SetBox(box, x + 2, y + 2, 12, 12);
                    SetColour(box, 200, 120, 60);
                    ApplyBlend(box, kBlendChoices[3 + (i / 4) % 2]);
                    break;
                }

                case 1:
                case 3:
                {
                    auto sprite = MakeCard(mTextures[(i / 2) % kTextures], x + 8, y + 8);
                    sprite->mAnim.SetSpriteScale(FP_FromDouble(0.5));
                    ApplyBlend(sprite->mAnim, blend);
                    mSprites.push_back(std::move(sprite));
                    break;
                }

                case 2:
                {
                    Line_G2& line = mLines[i / 4];
                    SetLine(line, x + 2, y + 2, x + 14, y + 14);
                    SetColour(line, 255, 255, 255);
                    ApplyBlend(line, blend);
                    break;
                }
            }
        }

        // A clip rectangle every few rows, each one the whole screen
        for (s32 i = 0; i < kClips; i++)
        {
            PSX_RECT rect = {0, 0, 640, 240};
            mClips[i].SetRect(rect);
        }
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mBandsCam);

        // All on one layer, interleaved, so nothing can be batched by type
        s32 spriteIdx = 0;
        for (s32 i = 0; i < kCells; i++)
        {
            switch (i % 4)
            {
                case 0:
                    ot.Add(kLayerMiddle, &mBoxes[i / 4]);
                    break;
                case 1:
                case 3:
                    mSprites[spriteIdx++]->Render(ot);
                    break;
                case 2:
                    ot.Add(kLayerMiddle, &mLines[i / 4]);
                    break;
            }

            if (i % (kCells / kClips) == 0)
            {
                ot.Add(kLayerMiddle, &mClips[i / (kCells / kClips)]);
            }
        }
    }

private:
    static constexpr s32 kColumns = 40;
    static constexpr s32 kCells = kColumns * 12;
    static constexpr s32 kTextures = 24;
    static constexpr s32 kClips = 8;
    std::vector<AnimResource> mTextures;
    Poly_G4 mBoxes[kCells / 4];
    Line_G2 mLines[kCells / 4];
    Prim_ScissorRect mClips[kClips];
    std::vector<std::unique_ptr<SceneSprite>> mSprites;
};

class StressPolygonsScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Stress: 20000 polygons";
    }

    const char* Expected() const override
    {
        return "A smooth colour gradient made of 20000 tiny boxes, over 65536 vertices in one frame.\n"
               "No gaps, stray triangles or missing rows";
    }

    bool IsStress() const override
    {
        return true;
    }

    bool IsStatic() const override
    {
        return true;
    }

    void Enter(SceneContext& /*ctx*/) override
    {
        mBoxes.resize(kColumns * kRows);
        for (s32 y = 0; y < kRows; y++)
        {
            for (s32 x = 0; x < kColumns; x++)
            {
                Poly_G4& box = mBoxes[y * kColumns + x];
                SetBox(box, 20 + x * 3, kContentTop + y * 2, 3, 2);
                SetColour(box, static_cast<u8>(255 * x / kColumns), static_cast<u8>(255 * y / kRows), 160);
            }
        }
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawBackground(ot, 0, 0, 0);
        for (Poly_G4& box : mBoxes)
        {
            ot.Add(kLayerMiddle, &box);
        }
    }

private:
    static constexpr s32 kColumns = 200;
    static constexpr s32 kRows = 100;
    std::vector<Poly_G4> mBoxes;
};

class TextureChurnScene final : public Scene
{
public:
    const char* Title() const override
    {
        return "Stress: texture churn";
    }

    const char* Expected() const override
    {
        return "Test cards popping up all over. Each has a texture of its own: the renderer uploads 8\n"
               "new ones every frame, and must free them again (checked after the last scene)";
    }

    bool IsStress() const override
    {
        return true;
    }

    void Update(SceneContext& ctx, u32 frame) override
    {
        SceneRandom random(frame);
        for (s32 i = 0; i < kPerFrame; i++)
        {
            mCards.push_back(MakeCard(ctx.mRes.MakeUniqueTestCard(), random.Next(640), kContentTop + random.Next(200)));
        }

        while (mCards.size() > kKept)
        {
            mCards.erase(mCards.begin());
        }
    }

    void Render(SceneContext& ctx, OrderingTable& ot) override
    {
        ctx.DrawCamera(ot, ctx.mRes.mGridCam);
        RenderAll(mCards, ot);
    }

private:
    static constexpr s32 kPerFrame = 8;
    static constexpr std::size_t kKept = 64;
    std::vector<std::unique_ptr<SceneSprite>> mCards;
};

// ----------------------------------------------------------------------------

template <typename T>
static std::unique_ptr<Scene> MakeScene()
{
    return std::make_unique<T>();
}

std::vector<SceneFactory> AllScenes()
{
    return {
        &MakeScene<ReferenceScene>,
        &MakeScene<PolygonsScene>,
        &MakeScene<LinesScene>,
        &MakeScene<BlendFlatScene>,
        &MakeScene<BlendSpritesScene>,
        &MakeScene<BlendTextScene>,
        &MakeScene<CameraScene>,
        &MakeScene<SpriteShadingScene>,
        &MakeScene<SpriteTransformScene>,
        &MakeScene<PalettesScene>,
        &MakeScene<TextScene>,
        &MakeScene<Fg1Scene>,
        &MakeScene<ScissorScene>,
        &MakeScene<ClipCarryOverScene>,
        &MakeScene<ScreenShakeScene>,
        &MakeScene<LaughingGasScene>,
        &MakeScene<ScreenWaveScene>,
        &MakeScene<FadeAndFlashScene>,
        &MakeScene<MenuTransitionScene>,
        &MakeScene<DeathGasScene>,
        &MakeScene<ZapLinesScene>,
        &MakeScene<StressSpritesScene>,
        &MakeScene<StressZapLinesScene>,
        &MakeScene<StressStateChangesScene>,
        &MakeScene<StressPolygonsScene>,
        &MakeScene<TextureChurnScene>,
    };
}

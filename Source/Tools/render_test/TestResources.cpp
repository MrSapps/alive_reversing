#include "stdafx.h"
#include "TestResources.hpp"
#include "../../relive_lib/SpriteMask.hpp"
#include "../../relive_lib/data_conversion/PaletteFit.hpp"
#include "../../relive_lib/data_conversion/rgb_conversion.hpp"
#include <array>
#include "../../relive_lib/AnimResources.hpp"
#include <algorithm>
#include <cmath>

static constexpr s32 kCamWidth = 640;
static constexpr s32 kCamHeight = 240;

struct RenderTestRgb final
{
    u8 r = 0;
    u8 g = 0;
    u8 b = 0;
};

// Hue in degrees, saturation and value 0 to 1
static RenderTestRgb HsvToRgb(f32 hue, f32 saturation, f32 value)
{
    hue = std::fmod(hue, 360.0f);
    if (hue < 0.0f)
    {
        hue += 360.0f;
    }

    const f32 chroma = value * saturation;
    const f32 x = chroma * (1.0f - std::fabs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));
    const f32 m = value - chroma;

    f32 r = 0.0f;
    f32 g = 0.0f;
    f32 b = 0.0f;
    switch (static_cast<s32>(hue / 60.0f))
    {
        case 0: r = chroma; g = x; break;
        case 1: r = x; g = chroma; break;
        case 2: g = chroma; b = x; break;
        case 3: g = x; b = chroma; break;
        case 4: r = x; b = chroma; break;
        default: r = chroma; b = x; break;
    }

    return {
        static_cast<u8>((r + m) * 255.0f),
        static_cast<u8>((g + m) * 255.0f),
        static_cast<u8>((b + m) * 255.0f)};
}

// An RGBA image drawn in code, for camera and FG1 images
class RenderTestImage final
{
public:
    RenderTestImage(s32 width, s32 height)
        : mWidth(width)
        , mHeight(height)
        , mPixels(std::make_shared<std::vector<u8>>(width * height * 4, static_cast<u8>(0)))
    {
    }

    void Set(s32 x, s32 y, RenderTestRgb c)
    {
        if (x < 0 || y < 0 || x >= mWidth || y >= mHeight)
        {
            return;
        }

        u8* pPixel = &(*mPixels)[(y * mWidth + x) * 4];
        pPixel[0] = c.r;
        pPixel[1] = c.g;
        pPixel[2] = c.b;
        pPixel[3] = 255;
    }

    void FillRect(s32 x, s32 y, s32 w, s32 h, RenderTestRgb c)
    {
        for (s32 yy = y; yy < y + h; yy++)
        {
            for (s32 xx = x; xx < x + w; xx++)
            {
                Set(xx, yy, c);
            }
        }
    }

    void FillCircle(s32 centreX, s32 centreY, s32 radius, RenderTestRgb c)
    {
        for (s32 y = -radius; y <= radius; y++)
        {
            for (s32 x = -radius; x <= radius; x++)
            {
                if (x * x + y * y <= radius * radius)
                {
                    Set(centreX + x, centreY + y, c);
                }
            }
        }
    }

    RgbaData ToRgbaData() const
    {
        RgbaData data;
        data.mWidth = mWidth;
        data.mHeight = mHeight;
        data.mPixels = mPixels;
        return data;
    }

private:
    s32 mWidth = 0;
    s32 mHeight = 0;
    std::shared_ptr<std::vector<u8>> mPixels;
};

// The colours of a sheet's palette indices
using TestPalette = std::array<RGBA32, 256>;

// A sprite sheet of palette indices drawn in code, with its frames side by side
class RenderTestSheet final
{
public:
    RenderTestSheet(u32 frameWidth, u32 frameHeight, u32 frameCount)
        : mFrameWidth(frameWidth)
        , mFrameHeight(frameHeight)
        , mFrameCount(frameCount)
        , mPixels(frameWidth * frameCount * frameHeight, TestResources::eTransparent)
    {
    }

    void Set(u32 frame, s32 x, s32 y, u8 index)
    {
        if (x < 0 || y < 0 || x >= static_cast<s32>(mFrameWidth) || y >= static_cast<s32>(mFrameHeight))
        {
            return;
        }
        mPixels[y * (mFrameWidth * mFrameCount) + frame * mFrameWidth + x] = index;
    }

    void FillRect(u32 frame, s32 x, s32 y, s32 w, s32 h, u8 index)
    {
        for (s32 yy = y; yy < y + h; yy++)
        {
            for (s32 xx = x; xx < x + w; xx++)
            {
                Set(frame, xx, yy, index);
            }
        }
    }

    // A dot fading out from the centre through the glow indices
    void FillGlow(u32 frame, f32 radius)
    {
        const f32 centreX = (mFrameWidth - 1) / 2.0f;
        const f32 centreY = (mFrameHeight - 1) / 2.0f;
        for (s32 y = 0; y < static_cast<s32>(mFrameHeight); y++)
        {
            for (s32 x = 0; x < static_cast<s32>(mFrameWidth); x++)
            {
                const f32 distance = std::hypot(x - centreX, y - centreY);
                if (distance < radius)
                {
                    const f32 brightness = 1.0f - (distance / radius);
                    const s32 step = static_cast<s32>(brightness * (TestResources::eGlowLast - TestResources::eGlowFirst));
                    Set(frame, x, y, static_cast<u8>(TestResources::eGlowFirst + step));
                }
            }
        }
    }

    // Each frame is anchored at its centre, so flipping doesn't move it
    AnimResource Build(AnimId id, const TestPalette& pal, u32 frameDelay, bool loop) const
    {
        auto png = std::make_shared<PngData>();
        // True colour, like the sheets from the data conversion: an entry without the
        // semi-transparency bit is solid (alpha 255), one with it blends (RGBConversion::kBlendAlpha)
        png->mRgba.resize(mPixels.size() * 4);
        for (std::size_t i = 0; i < mPixels.size(); i++)
        {
            const RGBA32& entry = pal[mPixels[i]];
            const bool transparent = entry.r == 0 && entry.g == 0 && entry.b == 0 && entry.a == 0;
            const RGBA32 pixel = transparent ? RGBA32{0, 0, 0, 0} : RGBA32{entry.r, entry.g, entry.b, entry.a != 255 ? u8(255) : RGBConversion::kBlendAlpha};
            memcpy(&png->mRgba[i * 4], &pixel, 4);
        }
        png->mWidth = mFrameWidth * mFrameCount;
        png->mHeight = mFrameHeight;

        auto frames = std::make_shared<AnimationAttributesAndFrames>();
        frames->mAttributes.mFrameRate = frameDelay;
        frames->mAttributes.mLoop = loop;
        frames->mAttributes.mLoopStartFrame = 0;
        frames->mAttributes.mMaxWidth = mFrameWidth;
        frames->mAttributes.mMaxHeight = mFrameHeight;

        for (u32 i = 0; i < mFrameCount; i++)
        {
            PerFrameInfo frame;
            frame.mXOffset = -static_cast<s32>(mFrameWidth / 2);
            frame.mYOffset = -static_cast<s32>(mFrameHeight / 2);
            frame.mWidth = mFrameWidth;
            frame.mHeight = mFrameHeight;
            frame.mSpriteWidth = mFrameWidth;
            frame.mSpriteHeight = mFrameHeight;
            frame.mSpriteSheetX = i * mFrameWidth;
            frame.mSpriteSheetY = 0;
            frames->mFrames.push_back(frame);
        }

        SpriteFx::ComputeLuma(png->mRgba, frames->mAttributes.mLuma);
        frames->mAttributes.mHasLuma = true;
        return AnimResource(id, frames, png);
    }

private:
    u32 mFrameWidth = 0;
    u32 mFrameHeight = 0;
    u32 mFrameCount = 0;
    std::vector<u8> mPixels;
};

RGBA32 TestResources::Opaque(u8 r, u8 g, u8 b)
{
    // All zeros is the transparent colour, so black is nudged off it
    if (r == 0 && g == 0 && b == 0)
    {
        r = g = b = 8;
    }
    // The alpha channel holds the PSX semi-transparency bit
    return {r, g, b, 0};
}

RGBA32 TestResources::SemiTrans(u8 r, u8 g, u8 b)
{
    return {r, g, b, 255};
}

static TestPalette MakeTestCardPalette(f32 hueShift)
{
    TestPalette pal = {};
    pal[TestResources::eTransparent] = {0, 0, 0, 0};
    pal[TestResources::eWhite] = TestResources::Opaque(255, 255, 255);
    pal[TestResources::eNearBlack] = TestResources::Opaque(0, 0, 0);
    pal[TestResources::eMarker] = TestResources::Opaque(64, 64, 64);

    const RenderTestRgb red = HsvToRgb(0.0f + hueShift, 1.0f, 0.85f);
    const RenderTestRgb green = HsvToRgb(120.0f + hueShift, 1.0f, 0.85f);
    const RenderTestRgb blue = HsvToRgb(240.0f + hueShift, 1.0f, 0.85f);
    pal[TestResources::eOpaqueRed] = TestResources::Opaque(red.r, red.g, red.b);
    pal[TestResources::eOpaqueGreen] = TestResources::Opaque(green.r, green.g, green.b);
    pal[TestResources::eOpaqueBlue] = TestResources::Opaque(blue.r, blue.g, blue.b);
    pal[TestResources::eSemiRed] = TestResources::SemiTrans(red.r, red.g, red.b);
    pal[TestResources::eSemiGreen] = TestResources::SemiTrans(green.r, green.g, green.b);
    pal[TestResources::eSemiBlue] = TestResources::SemiTrans(blue.r, blue.g, blue.b);

    for (s32 i = TestResources::eGlowFirst; i <= TestResources::eGlowLast; i++)
    {
        const u8 v = static_cast<u8>(255 * (i - TestResources::eGlowFirst + 1) / (TestResources::eGlowLast - TestResources::eGlowFirst + 1));
        pal[i] = TestResources::SemiTrans(v, v, v);
    }
    return pal;
}

// A glow palette of one colour, for particles
static TestPalette MakeGlowPalette(u8 r, u8 g, u8 b)
{
    TestPalette pal = {};
    for (s32 i = TestResources::eGlowFirst; i <= TestResources::eGlowLast; i++)
    {
        const s32 step = i - TestResources::eGlowFirst + 1;
        const s32 steps = TestResources::eGlowLast - TestResources::eGlowFirst + 1;
        pal[i] = TestResources::SemiTrans(
            static_cast<u8>(r * step / steps),
            static_cast<u8>(g * step / steps),
            static_cast<u8>(b * step / steps));
    }
    return pal;
}

// A 5x7 "F": asymmetric both ways, so any flip shows
static const char* kGlyphF[7] = {
    "#####",
    "#....",
    "#....",
    "####.",
    "#....",
    "#....",
    "#....",
};

static RenderTestSheet MakeTestCardSheet()
{
    constexpr s32 kSize = 40;
    constexpr u32 kFrames = 8;
    RenderTestSheet sheet(kSize, kSize, kFrames);

    // Where the marker is in each frame: round the border, clockwise from the top left
    const s32 markerSteps[kFrames][2] = {{1, 1}, {18, 1}, {35, 1}, {35, 18}, {35, 35}, {18, 35}, {1, 35}, {1, 18}};

    for (u32 frame = 0; frame < kFrames; frame++)
    {
        // 1 pixel transparent margin, then a 2 pixel white border
        sheet.FillRect(frame, 1, 1, kSize - 2, kSize - 2, TestResources::eWhite);

        // Left: white F on black
        sheet.FillRect(frame, 3, 3, 18, kSize - 6, TestResources::eNearBlack);
        for (s32 y = 0; y < 7; y++)
        {
            for (s32 x = 0; x < 5; x++)
            {
                if (kGlyphF[y][x] == '#')
                {
                    sheet.FillRect(frame, 5 + x * 3, 9 + y * 3, 3, 3, TestResources::eWhite);
                }
            }
        }

        // Right: red, green and blue bars, opaque on top, semi-transparent below
        const u8 opaque[3] = {TestResources::eOpaqueRed, TestResources::eOpaqueGreen, TestResources::eOpaqueBlue};
        const u8 semi[3] = {TestResources::eSemiRed, TestResources::eSemiGreen, TestResources::eSemiBlue};
        for (s32 bar = 0; bar < 3; bar++)
        {
            const s32 x = 21 + bar * 5;
            const s32 w = bar == 2 ? 6 : 5;
            sheet.FillRect(frame, x, 3, w, 17, opaque[bar]);
            sheet.FillRect(frame, x, 20, w, 17, semi[bar]);
        }

        sheet.FillRect(frame, markerSteps[frame][0], markerSteps[frame][1], 4, 4, TestResources::eMarker);
    }
    return sheet;
}

static CamResource MakeGridCam()
{
    RenderTestImage image(kCamWidth, kCamHeight);

    // Hue across, brightness down
    for (s32 y = 0; y < kCamHeight; y++)
    {
        for (s32 x = 0; x < kCamWidth; x++)
        {
            const f32 value = 0.35f + 0.5f * (1.0f - static_cast<f32>(y) / kCamHeight);
            image.Set(x, y, HsvToRgb(360.0f * x / kCamWidth, 0.6f, value));
        }
    }

    // Grid every 40 pixels
    for (s32 x = 0; x < kCamWidth; x += 40)
    {
        image.FillRect(x, 0, 1, kCamHeight, {40, 40, 40});
    }
    for (s32 y = 0; y < kCamHeight; y += 40)
    {
        image.FillRect(0, y, kCamWidth, 1, {40, 40, 40});
    }

    // Crosshair in the centre
    image.FillRect(kCamWidth / 2 - 20, kCamHeight / 2, 41, 1, {255, 255, 255});
    image.FillRect(kCamWidth / 2, kCamHeight / 2 - 20, 1, 41, {255, 255, 255});

    // 1 pixel checkerboard: turns into a grey smear if the image is scaled or filtered
    for (s32 y = 0; y < 32; y++)
    {
        for (s32 x = 0; x < 64; x++)
        {
            const u8 v = ((x + y) & 1) ? 255 : 0;
            image.Set(64 + x, 104 + y, {v, v, v});
        }
    }

    // Corner squares, inside the border
    image.FillRect(1, 1, 12, 12, {255, 0, 0});
    image.FillRect(kCamWidth - 13, 1, 12, 12, {0, 255, 0});
    image.FillRect(1, kCamHeight - 13, 12, 12, {0, 0, 255});
    image.FillRect(kCamWidth - 13, kCamHeight - 13, 12, 12, {255, 255, 255});

    // 1 pixel white border round the very edge
    image.FillRect(0, 0, kCamWidth, 1, {255, 255, 255});
    image.FillRect(0, kCamHeight - 1, kCamWidth, 1, {255, 255, 255});
    image.FillRect(0, 0, 1, kCamHeight, {255, 255, 255});
    image.FillRect(kCamWidth - 1, 0, 1, kCamHeight, {255, 255, 255});

    CamResource cam;
    cam.mData = image.ToRgbaData();
    return cam;
}

static CamResource MakeBandsCam()
{
    RenderTestImage image(kCamWidth, kCamHeight);
    const RenderTestRgb bands[7] = {{0, 0, 0}, {64, 64, 64}, {128, 128, 128}, {255, 255, 255}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
    for (s32 i = 0; i < 7; i++)
    {
        image.FillRect(i * 80, 0, 80, kCamHeight, bands[i]);
    }

    // Last band: black to white, top to bottom
    for (s32 y = 0; y < kCamHeight; y++)
    {
        const u8 v = static_cast<u8>(255 * y / (kCamHeight - 1));
        image.FillRect(560, y, 80, 1, {v, v, v});
    }

    CamResource cam;
    cam.mData = image.ToRgbaData();
    return cam;
}

// The FG1 scene's layout
static constexpr s32 kPillarXs[3] = {140, 300, 460};
static constexpr s32 kPillarWidth = 40;
static constexpr s32 kWallTop = 196;
static constexpr s32 kWellX = 560;
static constexpr s32 kWellY = 70;
static constexpr s32 kWellRadius = 44;
static constexpr s32 kBackWellX = 80;

static CamResource MakeFg1Cam()
{
    RenderTestImage image(kCamWidth, kCamHeight);

    // Sky
    for (s32 y = 0; y < kCamHeight; y++)
    {
        const u8 v = static_cast<u8>(60 + 120 * y / kCamHeight);
        image.FillRect(0, y, kCamWidth, 1, {static_cast<u8>(v / 3), static_cast<u8>(v / 2), v});
    }

    // Striped stone pillars
    for (s32 pillarX : kPillarXs)
    {
        for (s32 y = 0; y < kWallTop; y++)
        {
            const u8 v = (y / 8) & 1 ? 150 : 120;
            image.FillRect(pillarX, y, kPillarWidth, 1, {v, v, static_cast<u8>(v - 20)});
        }
        image.FillRect(pillarX, 0, 2, kWallTop, {220, 220, 200});
    }

    // Brick wall along the bottom
    for (s32 y = kWallTop; y < kCamHeight; y++)
    {
        for (s32 x = 0; x < kCamWidth; x++)
        {
            const bool mortar = (y % 11) == 0 || ((x + ((y / 11) & 1) * 16) % 32) == 0;
            image.Set(x, y, mortar ? RenderTestRgb{90, 80, 70} : RenderTestRgb{150, 70, 40});
        }
    }

    // A well in front, and one further back
    image.FillCircle(kWellX, kWellY, kWellRadius, {40, 120, 60});
    image.FillCircle(kWellX, kWellY, kWellRadius - 6, {20, 60, 30});
    image.FillCircle(kBackWellX, kWellY, kWellRadius, {120, 40, 120});
    image.FillCircle(kBackWellX, kWellY, kWellRadius - 6, {60, 20, 60});

    CamResource cam;
    cam.mData = image.ToRgbaData();
    return cam;
}

static Fg1Resource MakeFg1()
{
    // Any non-black pixel in an FG1 image masks the camera image back over what's been drawn
    RenderTestImage pillars(kCamWidth, kCamHeight);
    for (s32 pillarX : kPillarXs)
    {
        pillars.FillRect(pillarX, 0, kPillarWidth, kWallTop, {255, 255, 255});
    }

    RenderTestImage wall(kCamWidth, kCamHeight);
    wall.FillRect(0, kWallTop, kCamWidth, kCamHeight - kWallTop, {255, 255, 255});

    RenderTestImage well(kCamWidth, kCamHeight);
    well.FillCircle(kWellX, kWellY, kWellRadius, {255, 255, 255});

    RenderTestImage backWell(kCamWidth, kCamHeight);
    backWell.FillCircle(kBackWellX, kWellY, kWellRadius, {255, 255, 255});

    Fg1Resource fg1;
    fg1.mFg.mImage = pillars.ToRgbaData();
    fg1.mBg.mImage = wall.ToRgbaData();
    fg1.mFgWell.mImage = well.ToRgbaData();
    fg1.mBgWell.mImage = backWell.ToRgbaData();
    fg1.mFg1ResBlockCount = 4;
    return fg1;
}

static AnimResource MakeAlphaCard()
{
    constexpr u32 kSize = 48;
    auto png = std::make_shared<PngData>();
    png->mWidth = kSize;
    png->mHeight = kSize;
    png->mRgba.assign(kSize * kSize * 4, 0);
    png->mMask.assign(kSize * kSize * 4, 0);
    for (std::size_t i = 3; i < png->mMask.size(); i += 4)
    {
        png->mMask[i] = 255;
    }

    auto set = [&](u32 x, u32 y, u8 r, u8 g, u8 b, u8 a, u8 maskFlags = 0)
    {
        u8* pPixel = &png->mRgba[(y * kSize + x) * 4];
        pPixel[0] = r;
        pPixel[1] = g;
        pPixel[2] = b;
        pPixel[3] = a;
        SpriteMask::Set(&png->mMask[(y * kSize + x) * 4], maskFlags);
    };

    // The bar: alpha 0 to 255 across 44 pixels
    for (u32 y = 2; y < 12; y++)
    {
        for (u32 x = 2; x < 46; x++)
        {
            set(x, y, 255, 160, 40, static_cast<u8>(255 * (x - 2) / 43));
        }
    }

    // The glow
    for (u32 y = 14; y < 34; y++)
    {
        for (u32 x = 14; x < 34; x++)
        {
            const f32 distance = std::hypot(x - 23.5f, y - 23.5f);
            if (distance < 10.0f)
            {
                set(x, y, 255, 255, 255, static_cast<u8>(255 * (1.0f - distance / 10.0f)));
            }
        }
    }

    // The swatches
    const u8 swatchAlphas[3] = {RGBConversion::kBlendAlpha, 255, RGBConversion::kBlendAlpha};
    const u8 swatchMasks[3] = {0, 0, SpriteMask::kGlow};
    for (u32 swatch = 0; swatch < 3; swatch++)
    {
        for (u32 y = 36; y < 46; y++)
        {
            for (u32 x = 2 + swatch * 15; x < 2 + swatch * 15 + 13; x++)
            {
                set(x, y, 190, 190, 190, swatchAlphas[swatch], swatchMasks[swatch]);
            }
        }
    }

    auto frames = std::make_shared<AnimationAttributesAndFrames>();
    frames->mAttributes.mFrameRate = 1;
    frames->mAttributes.mLoop = true;
    frames->mAttributes.mLoopStartFrame = 0;
    frames->mAttributes.mMaxWidth = kSize;
    frames->mAttributes.mMaxHeight = kSize;

    PerFrameInfo frame;
    frame.mXOffset = -static_cast<s32>(kSize / 2);
    frame.mYOffset = -static_cast<s32>(kSize / 2);
    frame.mWidth = kSize;
    frame.mHeight = kSize;
    frame.mSpriteWidth = kSize;
    frame.mSpriteHeight = kSize;
    frames->mFrames.push_back(frame);

    return AnimResource(AnimId::None, frames, png);
}

SpriteFx TestResources::MakeHueShift(f32 degrees)
{
    // The way the data conversion makes a palette swap: the change of colours that takes the test
    // card's palette to the same palette with its hues rotated
    const TestPalette from = MakeTestCardPalette(0.0f);
    const TestPalette to = MakeTestCardPalette(degrees);

    SpriteFx fx;
    PaletteFit::Fit(from.data(), to.data(), 32, fx.mMatrix);
    return fx;
}

TestResources::TestResources(ResourceManagerWrapper& resMan)
{
    mGridCam = MakeGridCam();
    mBandsCam = MakeBandsCam();
    mFg1Cam = MakeFg1Cam();
    mFg1 = MakeFg1();

    mTestCard = MakeTestCardSheet().Build(AnimId::None, MakeTestCardPalette(0.0f), 4, true);

    constexpr s32 kPaletteCount = 64;
    for (s32 i = 0; i < kPaletteCount; i++)
    {
        mHueShifts.push_back(MakeHueShift(360.0f * i / kPaletteCount));
    }

    mAlphaCard = MakeAlphaCard();

    RenderTestSheet particle(16, 16, 1);
    particle.FillGlow(0, 8.0f);
    mParticle = particle.Build(AnimId::None, MakeGlowPalette(255, 200, 120), 1, true);

    // What the zap lines and sparks load
    RenderTestSheet zap(8, 8, 1);
    zap.FillGlow(0, 4.5f);
    const AnimResource blueZap = zap.Build(AnimId::Zap_Line_Blue, MakeGlowPalette(120, 160, 255), 1, true);
    const AnimResource redZap = zap.Build(AnimId::Zap_Line_Red, MakeGlowPalette(255, 90, 60), 1, true);
    resMan.AddAnimation(AnimId::Zap_Line_Blue, blueZap.mJsonPtr, blueZap.mPngPtr);
    resMan.AddAnimation(AnimId::Zap_Line_Red, redZap.mJsonPtr, redZap.mPngPtr);

    constexpr u32 kSparkFrames = 6;
    RenderTestSheet spark(12, 12, kSparkFrames);
    for (u32 i = 0; i < kSparkFrames; i++)
    {
        spark.FillGlow(i, 6.0f - i);
    }
    const AnimResource sparkRes = spark.Build(AnimId::ChantOrb_Particle_Small, MakeGlowPalette(255, 255, 160), 2, false);
    resMan.AddAnimation(AnimId::ChantOrb_Particle_Small, sparkRes.mJsonPtr, sparkRes.mPngPtr);
}

AnimResource TestResources::MakeUniqueTestCard() const
{
    // A copy of the test card's data gets a texture of its own: the renderers cache
    // textures by the resource's unique id
    AnimResource res = mTestCard;
    res.mUniqueId = UniqueResId();
    res.mPngPtr = std::make_shared<PngData>(*mTestCard.mPngPtr);
    return res;
}

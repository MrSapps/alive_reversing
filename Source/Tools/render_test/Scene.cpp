#include "stdafx.h"
#include "Scene.hpp"
#include "../../relive_lib/GameObjects/ScreenManager.hpp"
#include "../../relive_lib/PsxDisplay.hpp"
#include "../../AliveLibAE/PsxRender.hpp"
#include <cctype>

TextDrawer::TextDrawer(ResourceManagerWrapper& resMan)
{
    mFontContext.LoadFontType(FontType::Debug, resMan);

    PalResource pal;
    pal.mPal = mFontContext.mFntResource.mCurPal;
    mFont.Load(4096, pal, &mFontContext);
}

void TextDrawer::Draw(OrderingTable& ot, s32 x, s32 y, const char* text, const Style& style)
{
    gFontDrawScreenSpace = true;
    mPolyOffset = mFont.DrawString(
        ot,
        text,
        x,
        static_cast<s16>(y),
        style.blendMode,
        style.semiTrans,
        0,
        style.layer,
        style.r,
        style.g,
        style.b,
        mPolyOffset,
        style.scale,
        640,
        style.colourRandomRange);
    gFontDrawScreenSpace = false;
}

s32 TextDrawer::Width(const char* text)
{
    // In screen pixels, as Draw takes
    gFontDrawScreenSpace = true;
    const s32 width = mFont.MeasureTextWidth(text);
    gFontDrawScreenSpace = false;
    return width;
}

void SceneContext::DrawCamera(OrderingTable& ot, const CamResource& cam)
{
    CamResource camRes = cam;
    gScreenManager->DecompressCameraToVRam(camRes);
    gScreenManager->VRender(ot);
}

void SceneContext::DrawBackground(OrderingTable& ot, u8 r, u8 g, u8 b)
{
    mBackground.SetXYWH(0, 0, 640, 240);
    mBackground.SetRGB0(r, g, b);
    mBackground.SetRGB1(r, g, b);
    mBackground.SetRGB2(r, g, b);
    mBackground.SetRGB3(r, g, b);
    ot.Add(Layer::eLayer_1, &mBackground);
}

SceneSprite::SceneSprite(const AnimResource& res, s32 x, s32 y)
    : mX(x)
    , mY(y)
{
    mAnim.Init(res, nullptr);
    mAnim.SetRenderLayer(Layer::eLayer_Foreground_36);
}

SceneSprite::~SceneSprite()
{
    mAnim.VCleanUp();
}

void SceneSprite::Render(OrderingTable& ot)
{
    // Animation::VRender scales x from the PSX's 368 pixel wide screen
    mAnim.VRender(PCToPsxX(mX), mY, ot, 0, 0);
}

std::string SceneSlug(const Scene& scene)
{
    std::string slug;
    for (const char* p = scene.Title(); *p; p++)
    {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (std::isalnum(c))
        {
            slug += static_cast<char>(std::tolower(c));
        }
        else if (!slug.empty() && slug.back() != '_')
        {
            slug += '_';
        }
    }

    while (!slug.empty() && slug.back() == '_')
    {
        slug.pop_back();
    }
    return slug;
}

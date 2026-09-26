#include "stdafx.h"
#include "../../relive_lib/Renderer/FrameStatsOverlay.hpp"
#include "RenderTest.hpp"
#include "TestResources.hpp"
#include "../../relive_lib/Window.hpp"
#include "../../relive_lib/Events.hpp"
#include "../../relive_lib/Psx.hpp"
#include "../../relive_lib/PsxDisplay.hpp"
#include "../../relive_lib/ResourceManagerWrapper.hpp"
#include "../../relive_lib/data_conversion/file_system.hpp"
#include "../../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../../relive_lib/GameObjects/ScreenManager.hpp"
#include "../../AliveLibAE/Map.hpp"
#include "../../AliveLibAO/Map.hpp"
#include "../../AliveLibAE/LaughingGas.hpp"
#include <algorithm>
#include <cstdio>
#include <numeric>

extern u32 sGnFrame;
extern u8 sRandomSeed;

// Longer than any renderer keeps an unused texture for (OpenGL: 300 frames)
static constexpr u32 kTextureExpiryFrames = 330;

// Frames run before timing starts, so textures are uploaded and caches are warm
static constexpr u32 kWarmUpFrames = 10;

// Enough for a screen shake that starts on a scene's first frame to show
static constexpr u32 kScreenShakeCheckFrames = 30;

// Pixel differences between renderers smaller than this are rounding, not bugs
static constexpr u8 kRendererDiffTolerance = 8;

// The renderers draw the same apart from which pixel wins where a triangle edge or a texel
// boundary falls exactly on a pixel centre. More pixels than this differing is a bug.
static constexpr f64 kMaxRendererDiffPercent = 0.1;

static f64 NowMs()
{
    return static_cast<f64>(SDL_GetPerformanceCounter()) * 1000.0 / static_cast<f64>(SDL_GetPerformanceFrequency());
}

static f64 Percentile(std::vector<f64> values, f64 fraction)
{
    if (values.empty())
    {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const std::size_t idx = std::min(values.size() - 1, static_cast<std::size_t>(fraction * (values.size() - 1) + 0.5));
    return values[idx];
}

static f64 Average(const std::vector<f64>& values)
{
    return values.empty() ? 0.0 : std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

static std::string Lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    return s;
}

static std::string Format(const char* fmt, ...)
{
    char buffer[1024] = {};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    return buffer;
}

RenderTest::RenderTest(GameType gameType, FileSystem& fs, const RenderTestOptions& options)
    : mGameType(gameType)
    , mFs(fs)
    , mOptions(options)
{
    // The same set up as Engine::Init and Engine::Game_Run, less everything that needs game
    // data, sound or input
    mResMan = std::make_unique<ResourceManagerWrapper>(mFs, "");
    if (mGameType == GameType::eAe)
    {
        mMap = std::make_unique<Map>(*mResMan, mFactory);
    }
    else
    {
        mMap = std::make_unique<AO::Map>(*mResMan, mFactory);
    }

    gPsxDisplay.mWidth = 640;
    gPsxDisplay.mHeight = 240;

    gBaseGameObjects = relive_new DynamicArrayT<BaseGameObject>(90);
    BaseAnimatedWithPhysicsGameObject::MakeArray();
    AnimationBase::CreateAnimationArray();

    mRes = std::make_unique<TestResources>(*mResMan);
    mText = std::make_unique<TextDrawer>(*mResMan);
    mContext.reset(new SceneContext{*mResMan, *mMap, *mRes, *mText, {}});

    CamResource noCam;
    gScreenManager = relive_new ScreenManager(noCam, &mMap->mCameraOffset, *mResMan, *mMap);

    mSceneFactories = AllScenes();
    for (u32 i = 0; i < mSceneFactories.size(); i++)
    {
        const std::string slug = SceneSlug(*mSceneFactories[i]());
        if (mOptions.mSceneFilter.empty() || slug.find(Lower(mOptions.mSceneFilter)) != std::string::npos || i == 0)
        {
            mSceneOrder.push_back(i);
        }
    }
}

RenderTest::~RenderTest()
{
    mScene.reset();
    DestroySceneObjects();

    gBaseGameObjects->Remove_Item(gScreenManager);
    relive_delete gScreenManager;
    gScreenManager = nullptr;

    DestroyWindowAndRenderer();

    mMap.reset();
    AnimationBase::FreeAnimationArray();
    BaseAnimatedWithPhysicsGameObject::FreeArray();
    relive_delete gBaseGameObjects;
    gBaseGameObjects = nullptr;
}

void RenderTest::ListScenes()
{
    const std::vector<SceneFactory> factories = AllScenes();
    for (u32 i = 0; i < factories.size(); i++)
    {
        const std::unique_ptr<Scene> scene = factories[i]();
        printf("%2u  %-45s %s\n", i + 1, SceneSlug(*scene).c_str(), scene->IsStress() ? "(stress)" : "");
    }
}

s32 RenderTest::Run()
{
    return mOptions.mAuto ? RunAuto() : RunInteractive();
}

bool RenderTest::CreateWindowAndRenderer(IRenderer::Renderers type)
{
    const std::string title = std::string("R.E.L.I.V.E. render test (") + (mGameType == GameType::eAe ? "AE" : "AO") + ")";
    mWindow = std::make_unique<Window>();
    if (!mWindow->CreateWithRenderer(type, title, mOptions.mRendererChecks))
    {
        mWindow.reset();
        return false;
    }

    IRenderer& renderer = *IRenderer::GetRenderer();
    if (renderer.GetType() != type)
    {
        // CreateWithRenderer fell back to another one
        DestroyWindowAndRenderer();
        return false;
    }

    // The renderer, frame rate, frame time, draw calls and cached textures, in the top right. Left
    // out of --auto's captures, as they change every frame.
    if (!mOptions.mAuto)
    {
        renderer.ShowFrameStats(std::make_unique<FrameStatsOverlay>(*mResMan));
    }

    // The PSX framebuffer at 640x240, shown without filtering so single pixels can be seen
    renderer.SetUseOriginalResolution(true);
    renderer.SetKeepAspectRatio(true);
    renderer.SetFilterScreen(mFilter);
    return true;
}

void RenderTest::DestroyWindowAndRenderer()
{
    if (IRenderer::GetRenderer())
    {
        IRenderer::FreeRenderer();
    }
    mWindow.reset();
}

void RenderTest::StartScene(u32 sceneIdx)
{
    EndScene();

    // Every run of a scene starts from the same state, so its frames come out the same
    sRandomSeed = 0;
    LaughingGas::ResetRandomSeed();
    sGnFrame = 0;
    mSceneFrame = 0;

    mScene = mSceneFactories[sceneIdx]();
    mScene->Enter(*mContext);
}

void RenderTest::EndScene()
{
    mScene.reset();
    DestroySceneObjects();
}

void RenderTest::RunFrame(bool advance)
{
    if (advance)
    {
        EventsResetActive();
        UpdateObjects();
        mScene->Update(*mContext, mSceneFrame);
        AnimationBase::AnimateAll(AnimationBase::gAnimations);
    }

    mText->StartFrame();
    DrawObjects(mOt);
    mScene->Render(*mContext, mOt);
    if (mShowHud)
    {
        DrawHud(mOt);
    }

    PSX_DrawOTag(mOt);
    PSX_PutDispEnv_4F5890();

    DestroyDeadObjects();

    if (advance)
    {
        sGnFrame++;
        mSceneFrame++;
    }
}

void RenderTest::UpdateObjects()
{
    for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
    {
        BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
        if (!pObj)
        {
            break;
        }

        if (pObj->GetUpdatable() && !pObj->GetDead())
        {
            const s32 updateDelay = pObj->UpdateDelay();
            if (updateDelay <= 0)
            {
                pObj->VUpdate();
            }
            else
            {
                pObj->SetUpdateDelay(updateDelay - 1);
            }
        }
    }
}

void RenderTest::DrawObjects(OrderingTable& ot)
{
    for (s32 i = 0; i < gObjListDrawables->Size(); i++)
    {
        BaseGameObject* pDrawable = gObjListDrawables->ItemAt(i);
        if (!pDrawable)
        {
            break;
        }

        if (pDrawable->GetDead())
        {
            pDrawable->SetCantKill(false);
        }
        else if (pDrawable->GetDrawable())
        {
            pDrawable->SetCantKill(true);
            pDrawable->VRender(ot);
        }
    }
}

void RenderTest::DestroyDeadObjects()
{
    for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
    {
        BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
        if (!pObj)
        {
            break;
        }

        if (pObj->GetDead() && !pObj->GetCantKill() && pObj->mChaseCounter == 0)
        {
            i = gBaseGameObjects->RemoveAt(i);
            relive_delete pObj;
        }
    }
}

void RenderTest::DestroySceneObjects()
{
    // Everything but the screen manager. Taken out first, as deleting an object can make or
    // kill others.
    for (;;)
    {
        BaseGameObject* pVictim = nullptr;
        for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
        {
            BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
            if (pObj && pObj != gScreenManager)
            {
                pVictim = pObj;
                gBaseGameObjects->RemoveAt(i);
                break;
            }
        }

        if (!pVictim)
        {
            break;
        }
        relive_delete pVictim;
    }
}

void RenderTest::DrawHud(OrderingTable& ot)
{
    TextDrawer::Style shadow;
    shadow.r = 0;
    shadow.g = 0;
    shadow.b = 0;
    shadow.layer = Layer::eLayer_Menu_41;

    TextDrawer::Style text;
    text.layer = Layer::eLayer_Text_42;

    auto drawShadowed = [&](s32 x, s32 y, const char* str, u8 r, u8 g, u8 b)
    {
        mText->Draw(ot, x + 1, y + 1, str, shadow);
        text.r = r;
        text.g = g;
        text.b = b;
        mText->Draw(ot, x, y, str, text);
    };

    const u32 sceneNumber = mSceneOrder[mSceneOrderIdx] + 1;
    const std::string title = Format("[%u/%u] %s%s", sceneNumber, static_cast<u32>(mSceneFactories.size()), mScene->Title(), mPaused ? "  (paused)" : "");
    drawShadowed(4, 2, title.c_str(), 127, 127, 0);

    std::vector<std::string> lines;
    std::string expected = mScene->Expected();
    std::size_t start = 0;
    while (start <= expected.size())
    {
        const std::size_t end = expected.find('\n', start);
        lines.push_back(expected.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos)
        {
            break;
        }
        start = end + 1;
    }

    s32 y = 240 - 2 - static_cast<s32>(lines.size()) * TextDrawer::kLineHeight;
    for (const std::string& line : lines)
    {
        drawShadowed(4, y, line.c_str(), 127, 127, 127);
        y += TextDrawer::kLineHeight;
    }
}

Capture RenderTest::RunFrameAndCapture(bool advance)
{
    IRenderer& renderer = *IRenderer::GetRenderer();
    renderer.RequestCapture();
    RunFrame(advance);

    Capture capture;
    renderer.TakeCapture(capture.mPixels, capture.mWidth, capture.mHeight);
    return capture;
}

static bool SameScreenPosition(const SDL_Rect& a, const SDL_Rect& b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

bool RenderTest::RunFramesAndCheckScreenMoved(u32 frames)
{
    const IRenderer& renderer = *IRenderer::GetRenderer();
    SDL_Rect first = {};
    bool moved = false;
    for (u32 frame = 0; frame < frames; frame++)
    {
        RunFrame(true);
        const SDL_Rect& screenRect = renderer.GetLastFrameStats().mScreenRect;
        if (frame == 0)
        {
            first = screenRect;
        }
        else if (!SameScreenPosition(first, screenRect))
        {
            moved = true;
        }
    }
    return moved;
}

void RenderTest::Fail(const std::string& message)
{
    LOG_ERROR("FAIL: %s", message.c_str());
    mChecks.push_back({true, message});
}

void RenderTest::Warn(const std::string& message)
{
    LOG_WARNING("WARN: %s", message.c_str());
    mChecks.push_back({false, message});
}

std::string RenderTest::RendererDir(IRenderer::Renderers type) const
{
    return mOptions.mOutDir + "/" + Lower(IRenderer::TypeToString(type));
}

// ----------------------------------------------------------------------------
// Screen wave
// ----------------------------------------------------------------------------

// Frames drawn bigger than the screen, sampled down to its size
static Capture ToScreenSize(const Capture& capture)
{
    if (capture.mWidth == IRenderer::kPsxFramebufferWidth && capture.mHeight == IRenderer::kPsxFramebufferHeight)
    {
        return capture;
    }

    Capture out;
    out.mWidth = IRenderer::kPsxFramebufferWidth;
    out.mHeight = IRenderer::kPsxFramebufferHeight;
    out.mPixels.resize(static_cast<std::size_t>(out.mWidth) * out.mHeight * 4);
    for (s32 y = 0; y < out.mHeight; y++)
    {
        const s32 srcY = (2 * y + 1) * capture.mHeight / (2 * out.mHeight);
        for (s32 x = 0; x < out.mWidth; x++)
        {
            const s32 srcX = (2 * x + 1) * capture.mWidth / (2 * out.mWidth);
            memcpy(&out.mPixels[(y * out.mWidth + x) * 4], &capture.mPixels[(srcY * capture.mWidth + srcX) * 4], 4);
        }
    }
    return out;
}

// Draws the frame bigger than the screen, as the game does unless it's set to the original
// resolution
void RenderTest::SetScaledFramebuffer(bool scaled)
{
    IRenderer& renderer = *IRenderer::GetRenderer();
    renderer.SetUseOriginalResolution(!scaled);
    SDL_SetWindowSize(mWindow->Get(), scaled ? 1280 : 640, scaled ? 960 : 480);
}

// Screen wave pieces that move part of the frame without bending it, so that every renderer draws
// them exactly: each pixel must be where it's expected. Black, and anything from outside the
// screen, stays where it is.
void RenderTest::CheckScreenWave(bool scaled)
{
    IRenderer& renderer = *IRenderer::GetRenderer();
    const char* rendererName = IRenderer::TypeToString(renderer.GetType());
    const char* sizeName = scaled ? "scaled" : "original size";

    struct Move final
    {
        const char* mName;
        s32 mDestX, mDestY, mWidth, mHeight;
        s32 mSourceX, mSourceY;
    };
    static constexpr Move kMoves[] = {
        // The camera's 1 pixel black and white checkerboard: the black stays
        {"black", 400, 150, 64, 32, 64, 104},
        // With the green corner square
        {"right of the screen", 200, 40, 64, 32, 608, 0},
        // With the blue corner square
        {"below the screen", 300, 60, 32, 32, 0, 220},
        // With the red corner square
        {"left of the screen", 480, 40, 64, 32, -24, 0},
        {"above the screen", 560, 100, 32, 32, 100, -20},
    };

    std::vector<Prim_ScreenWave> pieces(ALIVE_COUNTOF(kMoves));
    for (std::size_t i = 0; i < pieces.size(); i++)
    {
        const Move& m = kMoves[i];
        Prim_ScreenWave& piece = pieces[i];
        const s16 x[4] = {0, static_cast<s16>(m.mWidth), 0, static_cast<s16>(m.mWidth)};
        const s16 y[4] = {0, 0, static_cast<s16>(m.mHeight), static_cast<s16>(m.mHeight)};
        piece.SetXY0(static_cast<s16>(m.mDestX + x[0]), static_cast<s16>(m.mDestY + y[0]));
        piece.SetXY1(static_cast<s16>(m.mDestX + x[1]), static_cast<s16>(m.mDestY + y[1]));
        piece.SetXY2(static_cast<s16>(m.mDestX + x[2]), static_cast<s16>(m.mDestY + y[2]));
        piece.SetXY3(static_cast<s16>(m.mDestX + x[3]), static_cast<s16>(m.mDestY + y[3]));
        for (u32 corner = 0; corner < 4; corner++)
        {
            piece.SetSource(corner, static_cast<s16>(m.mSourceX + x[corner]), static_cast<s16>(m.mSourceY + y[corner]));
        }
    }

    // The frame without them, then with them
    SetScaledFramebuffer(scaled);
    mContext->DrawCamera(mOt, mRes->mGridCam);
    const Capture before = ToScreenSize(RunFrameAndCaptureOt());
    mContext->DrawCamera(mOt, mRes->mGridCam);
    for (Prim_ScreenWave& piece : pieces)
    {
        mOt.Add(Layer::eLayer_FG1_37, &piece);
    }
    const Capture after = ToScreenSize(RunFrameAndCaptureOt());
    SetScaledFramebuffer(false);

    if (before.IsEmpty() || after.IsEmpty())
    {
        Fail(Format("%s: couldn't capture the screen wave check (%s)", rendererName, sizeName));
        return;
    }

    auto pixel = [](const Capture& c, s32 x, s32 y)
    {
        return &c.mPixels[(y * c.mWidth + x) * 4];
    };

    Capture expected = before;
    for (const Move& m : kMoves)
    {
        for (s32 y = 0; y < m.mHeight; y++)
        {
            for (s32 x = 0; x < m.mWidth; x++)
            {
                const s32 sourceX = m.mSourceX + x;
                const s32 sourceY = m.mSourceY + y;
                if (sourceX < 0 || sourceY < 0 || sourceX >= before.mWidth || sourceY >= before.mHeight)
                {
                    continue;
                }

                // What would be black in 16 bit colour
                const u8* pSource = pixel(before, sourceX, sourceY);
                if (pSource[0] < 8 && pSource[1] < 4 && pSource[2] < 8)
                {
                    continue;
                }
                memcpy(&expected.mPixels[((m.mDestY + y) * expected.mWidth + m.mDestX + x) * 4], pSource, 4);
            }
        }
    }

    const std::string diffFile = Format("screenwave_%s.png", scaled ? "scaled" : "original");
    for (const Move& m : kMoves)
    {
        u32 wrong = 0;
        for (s32 y = m.mDestY; y < m.mDestY + m.mHeight; y++)
        {
            for (s32 x = m.mDestX; x < m.mDestX + m.mWidth; x++)
            {
                wrong += memcmp(pixel(after, x, y), pixel(expected, x, y), 3) == 0 ? 0 : 1;
            }
        }
        if (wrong)
        {
            Fail(Format("%s: the screen wave draws wrongly (%s): %s (%u pixels, see %s)", rendererName, sizeName, m.mName, wrong, diffFile.c_str()));
        }
    }

    if (!CaptureDiff::Compare(after, expected).Identical())
    {
        CaptureDiff::MakeImage(after, expected).SavePng(mFs, RendererDir(renderer.GetType()) + "/" + diffFile);
    }
}

// Draws what's in the OT, and returns what the renderer drew
Capture RenderTest::RunFrameAndCaptureOt()
{
    IRenderer& renderer = *IRenderer::GetRenderer();
    renderer.RequestCapture();
    PSX_DrawOTag(mOt);
    PSX_PutDispEnv_4F5890();

    Capture capture;
    renderer.TakeCapture(capture.mPixels, capture.mWidth, capture.mHeight);
    return capture;
}

// ----------------------------------------------------------------------------
// Interactive
// ----------------------------------------------------------------------------

s32 RenderTest::RunInteractive()
{
    mRendererIdx = 0;
    if (!CreateWindowAndRenderer(mOptions.mRenderers[mRendererIdx]))
    {
        LOG_ERROR("Couldn't create the %s renderer", IRenderer::TypeToString(mOptions.mRenderers[mRendererIdx]));
        return 1;
    }

    // Start on the first scene the filter picked, rather than the reference frame
    mSceneOrderIdx = mSceneOrder.size() > 1 && !mOptions.mSceneFilter.empty() ? 1 : 0;
    StartScene(mSceneOrder[mSceneOrderIdx]);

    printf("Keys: left/right change scene, H hide text, P pause, F filtering, U uncapped fps,\n"
           "      R next renderer, C save a capture, Escape quit\n");

    constexpr u64 kFrameNs = 1000000000ull / 30;
    u64 nextFrameNs = SDL_GetTicksNS();
    bool quit = false;
    while (!quit)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                quit = true;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
            {
                HandleKey(event.key.key, quit);
            }
        }

        if (quit)
        {
            break;
        }

        RunFrame(!mPaused);

        if (!mUncapped)
        {
            // 30 fps, like the game
            nextFrameNs += kFrameNs;
            const u64 now = SDL_GetTicksNS();
            if (nextFrameNs > now)
            {
                SDL_DelayPrecise(nextFrameNs - now);
                IRenderer::GetRenderer()->AddIdleTime(SDL_GetTicksNS() - now);
            }
            else
            {
                nextFrameNs = now;
            }
        }
    }

    EndScene();
    DestroyWindowAndRenderer();
    return 0;
}

void RenderTest::HandleKey(SDL_Keycode key, bool& quit)
{
    const u32 sceneCount = static_cast<u32>(mSceneOrder.size());
    switch (key)
    {
        case SDLK_ESCAPE:
            quit = true;
            break;

        case SDLK_RIGHT:
        case SDLK_PAGEDOWN:
        case SDLK_SPACE:
            mSceneOrderIdx = (mSceneOrderIdx + 1) % sceneCount;
            StartScene(mSceneOrder[mSceneOrderIdx]);
            break;

        case SDLK_LEFT:
        case SDLK_PAGEUP:
            mSceneOrderIdx = (mSceneOrderIdx + sceneCount - 1) % sceneCount;
            StartScene(mSceneOrder[mSceneOrderIdx]);
            break;

        case SDLK_H:
            mShowHud = !mShowHud;
            break;

        case SDLK_P:
            mPaused = !mPaused;
            break;

        case SDLK_U:
            mUncapped = !mUncapped;
            break;

        case SDLK_F:
            mFilter = !mFilter;
            IRenderer::GetRenderer()->SetFilterScreen(mFilter);
            break;

        case SDLK_R:
        {
            // The scene carries on: it doesn't depend on the renderer
            DestroyWindowAndRenderer();
            for (u32 i = 1; i <= mOptions.mRenderers.size(); i++)
            {
                const u32 idx = (mRendererIdx + i) % mOptions.mRenderers.size();
                if (CreateWindowAndRenderer(mOptions.mRenderers[idx]))
                {
                    mRendererIdx = idx;
                    break;
                }
            }

            if (!IRenderer::GetRenderer())
            {
                LOG_ERROR("No renderer could be created");
                quit = true;
            }
            break;
        }

        case SDLK_C:
        {
            const Capture capture = RunFrameAndCapture(false);
            const IRenderer::Renderers type = IRenderer::GetRenderer()->GetType();
            mFs.CreateDirectory(FileSystem::Path(mOptions.mOutDir));
            mFs.CreateDirectory(FileSystem::Path(RendererDir(type)));
            const std::string path = RendererDir(type) + "/" + SceneSlug(*mScene) + ".png";
            if (capture.SavePng(mFs, path))
            {
                LOG_INFO("Saved %s", path.c_str());
            }
            break;
        }

        default:
            break;
    }
}

// ----------------------------------------------------------------------------
// Automatic
// ----------------------------------------------------------------------------

s32 RenderTest::RunAuto()
{
    mFs.CreateDirectory(FileSystem::Path(mOptions.mOutDir));

    std::vector<RendererResult> results;
    for (IRenderer::Renderers type : mOptions.mRenderers)
    {
        RendererResult result;
        result.mType = type;
        RunAutoOnRenderer(type, result);
        if (result.mRan && !mOptions.mBaselineDir.empty())
        {
            CompareWithBaseline(result);
        }
        results.push_back(std::move(result));
    }

    CompareRenderers(results);
    WriteReport(results);

    const bool anyFailed = std::any_of(mChecks.begin(), mChecks.end(), [](const Check& check) { return check.mFailure; });
    return anyFailed ? 1 : 0;
}

void RenderTest::RunAutoOnRenderer(IRenderer::Renderers type, RendererResult& result)
{
    const char* rendererName = IRenderer::TypeToString(type);
    if (!CreateWindowAndRenderer(type))
    {
        Warn(Format("%s: couldn't be created, skipped", rendererName));
        return;
    }

    LOG_INFO("Render test: running %u scenes on %s", static_cast<u32>(mSceneOrder.size()), rendererName);
    result.mRan = true;
    mFs.CreateDirectory(FileSystem::Path(RendererDir(type)));

    // The reference frame, which every scene has to leave untouched. Drawn twice, as the
    // first frame after creating a renderer uploads everything.
    mSceneOrderIdx = 0;
    StartScene(0);
    RunFrame(false);
    const Capture reference = RunFrameAndCapture(false);
    const u32 baseCachedTextures = IRenderer::GetRenderer()->GetLastFrameStats().mCachedTextures;
    EndScene();

    if (reference.IsEmpty())
    {
        Fail(Format("%s: couldn't read back the frame", rendererName));
    }

    // Anything a scene leaves behind shows up as a difference from these
    const s32 baseObjects = gBaseGameObjects->Size();
    const s32 baseDrawables = gObjListDrawables->Size();
    const s32 baseAnimations = AnimationBase::gAnimations->Size();

    for (mSceneOrderIdx = 0; mSceneOrderIdx < mSceneOrder.size(); mSceneOrderIdx++)
    {
        const u32 sceneIdx = mSceneOrder[mSceneOrderIdx];
        RunAutoScene(sceneIdx, reference, result);

        const std::string& title = result.mScenes.back().mTitle;
        if (gBaseGameObjects->Size() != baseObjects || gObjListDrawables->Size() != baseDrawables || AnimationBase::gAnimations->Size() != baseAnimations)
        {
            Fail(Format("%s: \"%s\" left objects behind (objects %d -> %d, drawables %d -> %d, animations %d -> %d)",
                        rendererName, title.c_str(),
                        baseObjects, gBaseGameObjects->Size(),
                        baseDrawables, gObjListDrawables->Size(),
                        baseAnimations, AnimationBase::gAnimations->Size()));
        }
    }

    // What the screen wave draws, exactly
    mSceneOrderIdx = 0;
    StartScene(0);
    CheckScreenWave(false);
    CheckScreenWave(true);
    EndScene();

    // Textures from the scenes must all expire once nothing draws them
    mSceneOrderIdx = 0;
    StartScene(0);
    for (u32 i = 0; i < kTextureExpiryFrames; i++)
    {
        RunFrame(true);
    }
    const u32 cachedTextures = IRenderer::GetRenderer()->GetLastFrameStats().mCachedTextures;
    if (cachedTextures > baseCachedTextures)
    {
        Fail(Format("%s: %u textures still cached %u frames after the scenes ended (%u when it started)",
                    rendererName, cachedTextures, kTextureExpiryFrames, baseCachedTextures));
    }

    // A frame with nothing in it: the renderers don't clear the screen between frames,
    // which the game doesn't need as it always draws a full screen camera
    EndScene();
    IRenderer::GetRenderer()->RequestCapture();
    PSX_DrawOTag(mOt);
    PSX_PutDispEnv_4F5890();
    Capture empty;
    IRenderer::GetRenderer()->TakeCapture(empty.mPixels, empty.mWidth, empty.mHeight);
    if (!empty.IsEmpty() && !empty.IsAllBlack())
    {
        Warn(Format("%s: a frame that draws nothing still shows the last frame (the framebuffer isn't cleared)", rendererName));
    }

    DestroyWindowAndRenderer();
}

void RenderTest::RunAutoScene(u32 sceneIdx, const Capture& reference, RendererResult& result)
{
    IRenderer& renderer = *IRenderer::GetRenderer();
    const char* rendererName = IRenderer::TypeToString(renderer.GetType());

    StartScene(sceneIdx);

    SceneResult sceneResult;
    sceneResult.mTitle = mScene->Title();
    sceneResult.mFileName = Format("%02u_%s.png", sceneIdx + 1, SceneSlug(*mScene).c_str());
    LOG_INFO("Render test: %s: %s", rendererName, sceneResult.mTitle.c_str());

    const u32 captureFrame = mScene->CaptureFrame();
    const u32 timedFrames = mOptions.mTimedFrames * (mScene->IsStress() ? 4 : 1);
    const u32 totalFrames = std::max(captureFrame + 2, kWarmUpFrames + timedFrames);

    const bool shakesScreen = mScene->ShakesScreen();
    SDL_Rect firstScreenRect = {};
    bool screenMoved = false;

    Capture nextFrame;
    for (u32 frame = 0; frame < totalFrames; frame++)
    {
        const bool capture = frame == captureFrame || (frame == captureFrame + 1 && mScene->IsStatic());
        if (capture)
        {
            renderer.RequestCapture();
        }

        const f64 start = NowMs();
        RunFrame(true);
        const f64 elapsed = NowMs() - start;

        const IRenderer::FrameStats& stats = renderer.GetLastFrameStats();
        if (frame >= kWarmUpFrames)
        {
            sceneResult.mFrameMs.push_back(elapsed);
            sceneResult.mDrawCalls += stats.mDrawCalls;
            sceneResult.mTextureUploads += stats.mTextureUploads;
        }
        sceneResult.mPeakCachedTextures = std::max(sceneResult.mPeakCachedTextures, stats.mCachedTextures);

        if (frame == 0)
        {
            firstScreenRect = stats.mScreenRect;
        }
        else if (!SameScreenPosition(firstScreenRect, stats.mScreenRect))
        {
            screenMoved = true;
        }

        if (capture)
        {
            Capture& target = frame == captureFrame ? sceneResult.mCapture : nextFrame;
            renderer.TakeCapture(target.mPixels, target.mWidth, target.mHeight);
        }
    }
    EndScene();

    sceneResult.mCapture.SavePng(mFs, RendererDir(renderer.GetType()) + "/" + sceneResult.mFileName);

    if (sceneResult.mCapture.IsEmpty())
    {
        Fail(Format("%s: \"%s\" couldn't be captured", rendererName, sceneResult.mTitle.c_str()));
    }

    // Nothing in a static scene moves, so its frames must be identical
    if (!nextFrame.IsEmpty())
    {
        const CaptureDiff diff = CaptureDiff::Compare(sceneResult.mCapture, nextFrame);
        if (!diff.Identical())
        {
            Fail(Format("%s: \"%s\" doesn't draw the same twice in a row (%u pixels differ)", rendererName, sceneResult.mTitle.c_str(), diff.mDifferingPixels));
            CaptureDiff::MakeImage(sceneResult.mCapture, nextFrame).SavePng(mFs, RendererDir(renderer.GetType()) + "/unstable_" + sceneResult.mFileName);
        }
    }

    // Captures are taken before the frame is drawn to the window, so they can't show screen shake.
    // Where the renderer drew the frame in the window is checked instead.
    if (shakesScreen && !screenMoved)
    {
        Fail(Format("%s: \"%s\" didn't shake the screen", rendererName, sceneResult.mTitle.c_str()));
    }
    else if (!shakesScreen && screenMoved)
    {
        Fail(Format("%s: \"%s\" moved the frame in the window, but has no screen shake", rendererName, sceneResult.mTitle.c_str()));
    }

    if (shakesScreen)
    {
        // Again with the frame stats drawn over the frame, as the game and the interactive
        // test can show them. They're left out of the run above because they'd be in its captures.
        StartScene(sceneIdx);
        renderer.ShowFrameStats(std::make_unique<FrameStatsOverlay>(*mResMan));
        const bool movedWithStats = RunFramesAndCheckScreenMoved(kScreenShakeCheckFrames);
        renderer.ShowFrameStats(nullptr);
        EndScene();

        if (!movedWithStats)
        {
            Fail(Format("%s: \"%s\" didn't shake the screen with the frame stats shown", rendererName, sceneResult.mTitle.c_str()));
        }
    }

    // Draw the reference frame again: any state the scene left in the renderer (blending,
    // clipping, which framebuffer is current...) shows up as a difference
    if (sceneIdx != 0)
    {
        mSceneOrderIdx = 0;
        StartScene(0);
        const Capture after = RunFrameAndCapture(false);
        EndScene();

        const CaptureDiff diff = CaptureDiff::Compare(reference, after);
        if (!diff.Identical())
        {
            const std::string leakFile = "leak_after_" + sceneResult.mFileName;
            Fail(Format("%s: \"%s\" changed how the next frame is drawn (%u pixels of the reference frame differ, see %s)",
                        rendererName, sceneResult.mTitle.c_str(), diff.mDifferingPixels, leakFile.c_str()));
            CaptureDiff::MakeImage(after, reference).SavePng(mFs, RendererDir(renderer.GetType()) + "/" + leakFile);
        }

        // Back to the scene for the HUD's numbering
        mSceneOrderIdx = static_cast<u32>(std::find(mSceneOrder.begin(), mSceneOrder.end(), sceneIdx) - mSceneOrder.begin());
    }

    result.mScenes.push_back(std::move(sceneResult));
}

void RenderTest::CompareWithBaseline(const RendererResult& result)
{
    const char* rendererName = IRenderer::TypeToString(result.mType);
    const std::string baselineDir = mOptions.mBaselineDir + "/" + Lower(rendererName);
    for (const SceneResult& scene : result.mScenes)
    {
        const Capture baseline = Capture::LoadPng(mFs, baselineDir + "/" + scene.mFileName);
        if (baseline.IsEmpty())
        {
            Warn(Format("%s: \"%s\" has no baseline in %s", rendererName, scene.mTitle.c_str(), baselineDir.c_str()));
            continue;
        }

        const CaptureDiff diff = CaptureDiff::Compare(scene.mCapture, baseline);
        if (!diff.Identical())
        {
            const std::string diffFile = "baseline_diff_" + scene.mFileName;
            Fail(Format("%s: \"%s\" differs from the baseline (%.2f%% of pixels, see %s)", rendererName, scene.mTitle.c_str(), diff.DifferingPercent(), diffFile.c_str()));
            CaptureDiff::MakeImage(scene.mCapture, baseline).SavePng(mFs, RendererDir(result.mType) + "/" + diffFile);
        }
    }
}

void RenderTest::CompareRenderers(const std::vector<RendererResult>& results)
{
    for (std::size_t a = 0; a < results.size(); a++)
    {
        for (std::size_t b = a + 1; b < results.size(); b++)
        {
            if (!results[a].mRan || !results[b].mRan)
            {
                continue;
            }

            const std::string dir = mOptions.mOutDir + "/diff_" + Lower(IRenderer::TypeToString(results[a].mType)) + "_" + Lower(IRenderer::TypeToString(results[b].mType));
            mFs.CreateDirectory(FileSystem::Path(dir));
            for (std::size_t i = 0; i < results[a].mScenes.size() && i < results[b].mScenes.size(); i++)
            {
                const SceneResult& sceneA = results[a].mScenes[i];
                const SceneResult& sceneB = results[b].mScenes[i];
                const CaptureDiff diff = CaptureDiff::Compare(sceneA.mCapture, sceneB.mCapture, kRendererDiffTolerance);
                if (!diff.Identical())
                {
                    CaptureDiff::MakeImage(sceneA.mCapture, sceneB.mCapture, kRendererDiffTolerance).SavePng(mFs, dir + "/" + sceneA.mFileName);
                }

                if (diff.DifferingPercent() > kMaxRendererDiffPercent)
                {
                    Fail(Format("\"%s\" is drawn differently by %s and %s (%.2f%% of pixels, see %s)",
                                sceneA.mTitle.c_str(), IRenderer::TypeToString(results[a].mType), IRenderer::TypeToString(results[b].mType),
                                diff.DifferingPercent(), (dir.substr(mOptions.mOutDir.size() + 1) + "/" + sceneA.mFileName).c_str()));
                }
            }
        }
    }
}

void RenderTest::WriteReport(const std::vector<RendererResult>& results)
{
    std::string report;
    report += Format("R.E.L.I.V.E. render test: %s, %u scenes, %u timed frames each (stress scenes x4)\n\n",
                     mGameType == GameType::eAe ? "AE" : "AO", static_cast<u32>(mSceneOrder.size()), mOptions.mTimedFrames);

    report += "Frame times in ms: CPU time for a whole frame, including building it, the renderer's\n"
              "work and presenting it. calls = draw calls per frame, up = textures uploaded per frame.\n\n";

    std::string header = Format("%-44s", "Scene");
    for (const RendererResult& result : results)
    {
        if (result.mRan)
        {
            header += Format(" | %-8s %6s %6s %6s %5s %5s", IRenderer::TypeToString(result.mType), "avg", "p95", "max", "calls", "up");
        }
    }
    report += header + "\n" + std::string(header.size(), '-') + "\n";

    std::string csv = "renderer,scene,frames,avg_ms,p50_ms,p95_ms,max_ms,draw_calls_per_frame,uploads_per_frame,peak_cached_textures\n";

    const std::size_t sceneCount = mSceneOrder.size();
    for (std::size_t i = 0; i < sceneCount; i++)
    {
        std::string line;
        for (const RendererResult& result : results)
        {
            if (!result.mRan || i >= result.mScenes.size())
            {
                continue;
            }

            const SceneResult& scene = result.mScenes[i];
            if (line.empty())
            {
                line = Format("%-44.44s", scene.mTitle.c_str());
            }

            const f64 frames = std::max<f64>(1.0, static_cast<f64>(scene.mFrameMs.size()));
            line += Format(" | %-8s %6.2f %6.2f %6.2f %5.0f %5.1f", "",
                           Average(scene.mFrameMs), Percentile(scene.mFrameMs, 0.95), Percentile(scene.mFrameMs, 1.0),
                           scene.mDrawCalls / frames, scene.mTextureUploads / frames);

            csv += Format("%s,\"%s\",%u,%.3f,%.3f,%.3f,%.3f,%.1f,%.2f,%u\n",
                          IRenderer::TypeToString(result.mType), scene.mTitle.c_str(), static_cast<u32>(scene.mFrameMs.size()),
                          Average(scene.mFrameMs), Percentile(scene.mFrameMs, 0.5), Percentile(scene.mFrameMs, 0.95), Percentile(scene.mFrameMs, 1.0),
                          scene.mDrawCalls / frames, scene.mTextureUploads / frames, scene.mPeakCachedTextures);
        }
        if (!line.empty())
        {
            report += line + "\n";
        }
    }

    // How different the renderers' pictures are
    for (std::size_t a = 0; a < results.size(); a++)
    {
        for (std::size_t b = a + 1; b < results.size(); b++)
        {
            if (!results[a].mRan || !results[b].mRan)
            {
                continue;
            }

            report += Format("\nPixels that differ between %s and %s by more than %u (images in diff_%s_%s):\n",
                             IRenderer::TypeToString(results[a].mType), IRenderer::TypeToString(results[b].mType), kRendererDiffTolerance,
                             Lower(IRenderer::TypeToString(results[a].mType)).c_str(), Lower(IRenderer::TypeToString(results[b].mType)).c_str());
            for (std::size_t i = 0; i < results[a].mScenes.size() && i < results[b].mScenes.size(); i++)
            {
                const CaptureDiff diff = CaptureDiff::Compare(results[a].mScenes[i].mCapture, results[b].mScenes[i].mCapture, kRendererDiffTolerance);
                report += Format("  %-44.44s %6.2f%%  (largest difference %u)\n", results[a].mScenes[i].mTitle.c_str(), diff.DifferingPercent(), diff.mMaxDifference);
            }
        }
    }

    u32 failures = 0;
    u32 warnings = 0;
    report += "\nChecks:\n";
    for (const Check& check : mChecks)
    {
        report += Format("  %s %s\n", check.mFailure ? "FAIL" : "WARN", check.mMessage.c_str());
        (check.mFailure ? failures : warnings)++;
    }
    if (mChecks.empty())
    {
        report += "  all passed\n";
    }
    report += Format("\n%u failed, %u warnings. Captures are in %s\n", failures, warnings, mOptions.mOutDir.c_str());

    printf("\n%s", report.c_str());
    mFs.Save((mOptions.mOutDir + "/report.txt").c_str(), std::vector<u8>(report.begin(), report.end()));
    mFs.Save((mOptions.mOutDir + "/perf.csv").c_str(), std::vector<u8>(csv.begin(), csv.end()));
}

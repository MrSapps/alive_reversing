#pragma once

#include "../../relive_lib/Renderer/IRenderer.hpp"
#include "../../relive_lib/GameType.hpp"
#include "../../relive_lib/Factory.hpp"
#include "../../AliveLibAE/PsxRender.hpp"
#include "Capture.hpp"
#include "Scene.hpp"
#include <memory>
#include <string>
#include <vector>

class FileSystem;
class Window;
class BaseMap;
class ResourceManagerWrapper;
class TestResources;

struct RenderTestOptions final
{
    // --auto runs each one in turn. Interactively, R switches between them.
    std::vector<IRenderer::Renderers> mRenderers;

    // Run every scene unattended: check for leaks, time them, save captures, write a report
    // and exit
    bool mAuto = false;

    // Frames timed per scene in --auto. Stress scenes are timed for 4 times as long.
    u32 mTimedFrames = 120;

    // Where --auto writes its report and captures, and interactive C saves a capture
    std::string mOutDir = "render_test_out";

    // A previous --auto run's output directory to compare the captures against
    std::string mBaselineDir;

    // Only run scenes whose name contains this (interactively: start on the first one)
    std::string mSceneFilter;
};

// Runs the render test scenes with the real renderers, the real ordering table and as
// much of the real game code as works without game data. See README.md next to this.
class RenderTest final
{
public:
    RenderTest(GameType gameType, FileSystem& fs, const RenderTestOptions& options);
    ~RenderTest();

    // Returns the process exit code: non zero if --auto found a failure
    s32 Run();

    // Prints each scene's number and name
    static void ListScenes();

private:
    struct Check final
    {
        bool mFailure = false;
        std::string mMessage;
    };

    struct SceneResult final
    {
        std::string mTitle;
        std::string mFileName;
        std::vector<f64> mFrameMs;
        u64 mDrawCalls = 0;
        u64 mTextureUploads = 0;
        u32 mPeakCachedTextures = 0;
        Capture mCapture;
    };

    struct RendererResult final
    {
        IRenderer::Renderers mType = IRenderer::Renderers::OpenGL;
        bool mRan = false;
        std::vector<SceneResult> mScenes;
    };

    bool CreateWindowAndRenderer(IRenderer::Renderers type);
    void DestroyWindowAndRenderer();

    void StartScene(u32 sceneIdx);
    void EndScene();

    // One frame of the game loop, done the way Engine::RunFrame does it. When advance is
    // false (paused) nothing moves, it's only drawn again.
    void RunFrame(bool advance);
    void UpdateObjects();
    void DrawObjects(OrderingTable& ot);
    void DestroyDeadObjects();
    void DestroySceneObjects();
    void DrawHud(OrderingTable& ot);

    // Runs a frame and returns what the renderer drew
    Capture RunFrameAndCapture(bool advance);

    s32 RunInteractive();
    void HandleKey(SDL_Keycode key, bool& quit);

    void SetScaledFramebuffer(bool scaled);
    void CheckScreenWave(bool scaled);
    Capture RunFrameAndCaptureOt();

    s32 RunAuto();
    void RunAutoOnRenderer(IRenderer::Renderers type, RendererResult& result);
    void RunAutoScene(u32 sceneIdx, const Capture& reference, RendererResult& result);
    void CompareRenderers(const std::vector<RendererResult>& results);
    void CompareWithBaseline(const RendererResult& result);
    void WriteReport(const std::vector<RendererResult>& results);

    void Fail(const std::string& message);
    void Warn(const std::string& message);
    std::string RendererDir(IRenderer::Renderers type) const;

    GameType mGameType;
    FileSystem& mFs;
    RenderTestOptions mOptions;

    std::unique_ptr<ResourceManagerWrapper> mResMan;
    relive::Factory mFactory;
    std::unique_ptr<BaseMap> mMap;
    std::unique_ptr<TestResources> mRes;
    std::unique_ptr<TextDrawer> mText;
    std::unique_ptr<SceneContext> mContext;
    std::unique_ptr<Window> mWindow;

    std::vector<SceneFactory> mSceneFactories;
    std::vector<u32> mSceneOrder;
    u32 mSceneOrderIdx = 0;
    std::unique_ptr<Scene> mScene;
    u32 mSceneFrame = 0;

    OrderingTable mOt;

    // Interactive settings
    u32 mRendererIdx = 0;
    bool mShowHud = true;
    bool mPaused = false;
    bool mUncapped = false;
    bool mFilter = false;
    std::vector<f64> mRecentFrameMs;

    std::vector<Check> mChecks;
};

#pragma once

#include <SDL3/SDL.h>
#include <memory>
#include <vector>

struct BasePrimitive;
struct Prim_ScissorRect;
struct Prim_ScreenOffset;
struct Prim_GasEffect;
struct Line_G2;
struct Line_G4;
struct Poly_G3;
struct Poly_FT4;
struct Poly_G4;
class Window;
class FrameStatsOverlay;

namespace relive {
enum class TBlendModes : u32;
}

class RendererException final : public std::exception
{
public:
    explicit RendererException(char const* const message) noexcept
        : mMessage(message)
    {

    }

    [[nodiscard]] virtual const char* what() const noexcept override
    {
         return mMessage;
    }

private:
    const char* mMessage = nullptr;
};

class IRenderer
{
public: // TODO: Make protected later
    enum class PsxDrawMode : u32
    {
        Flat = 0,
        DefaultFT4 = 1,
        Camera = 2,
        FG1 = 3,
        Gas = 4,
        ScreenWave = 5
    };

    struct PsxVertexData final
    {
        f32 x, y;
        f32 r, g, b;
        f32 u, v;
        PsxDrawMode drawMode;
        u32 isSemiTrans;
        u32 isShaded;
        relive::TBlendModes blendMode;
        u32 paletteIndex;
        u32 textureUnitIndex;
    };

    // Original game resolution - 640x240
    static constexpr s32 kPsxFramebufferHeight = 240;
    static constexpr s32 kPsxFramebufferWidth = 640;

    struct Point2D final
    {
        f32 x;
        f32 y;

        Point2D(f32 x0, f32 y0)
            : x(x0)
            , y(y0)
        { }
    };

    struct Quad2D final
    {
        Point2D verts[4];
    };

    // A 1 pixel wide quad along the line. verts[0] and [1] are at p1, [2] and [3] at p2.
    static Quad2D LineToQuad(const Point2D& p1, const Point2D& p2);

    // A textured quad's texture coordinates in texels: (u0, v0) at its first corner, (u1, v1)
    // at its last
    struct QuadUVs final
    {
        f32 u0, v0, u1, v1;
    };

    // The area of its sprite sheet an animated poly's frame is in, flipped as the poly says
    static QuadUVs GetAnimUVs(const Poly_FT4& poly);

    // A font poly's glyph
    static QuadUVs GetFontUVs(const Poly_FT4& poly);

    // Prim_ScissorRect's (0, 0, 1, 1) means no clipping
    static bool IsScissorDisabled(const Prim_ScissorRect& clipper);

public:
    enum class Renderers
    {
        Sdl3,
        OpenGL
    };

    static IRenderer* GetRenderer();
    // Creates a renderer of type for window, see Window::CreateWithRenderer. checks turns on
    // the renderer's slow error checking (-renderer_checks).
    static bool CreateRenderer(Renderers type, Window& window, bool checks);
    static void FreeRenderer();

public:
    explicit IRenderer(Window& window);
    virtual ~IRenderer();

    // Draws this over every frame from now on: the frame rate, frame time and what the renderer did
    void ShowFrameStats(std::unique_ptr<FrameStatsOverlay> frameStats);

    // Time spent waiting for the next frame's turn, which the frame time leaves out
    void AddIdleTime(u64 ns);

    // Which renderer this is. Window::CreateWithRenderer falls back to another one if the
    // requested one can't be created.
    virtual Renderers GetType() const = 0;
    static const char* TypeToString(Renderers type);

    // What the renderer did in the last frame
    struct FrameStats final
    {
        // Batches (OpenGL) or geometry submissions (SDL3)
        u32 mDrawCalls = 0;
        // Textures created or re-uploaded
        u32 mTextureUploads = 0;
        // Textures the renderer is keeping alive in its cache
        u32 mCachedTextures = 0;
    };

    const FrameStats& GetLastFrameStats() const
    {
        return mLastFrameStats;
    }

    // Asks for a copy of the next frame's 640x240 PSX framebuffer, as it is before it's scaled
    // to the window (so screen shake isn't in it). TakeCapture hands it over once that frame
    // has ended: RGBA32 rows, top row first.
    void RequestCapture()
    {
        mCaptureRequested = true;
    }
    bool TakeCapture(std::vector<u8>& rgbaPixels, s32& width, s32& height);

    void SetFilterScreen(bool filterScreen)
    {
        mFramebufferFilter = filterScreen;
    }

    void SetKeepAspectRatio(bool keepAspectRatio)
    {
        mKeepAspectRatio = keepAspectRatio;
    }

    void SetUseOriginalResolution(bool useOriginalResolution)
    {
        mUseOriginalResolution = useOriginalResolution;
    }

    virtual void Clear(u8 r, u8 g, u8 b) = 0;

    // Derived objects should always call this
    virtual void StartFrame();

    virtual void EndFrame() = 0;

    virtual void SetClip(const Prim_ScissorRect& clipper) = 0;
    void SetScreenOffset(const Prim_ScreenOffset& offset);

    virtual void Draw(const Prim_GasEffect& gasEffect) = 0;

    // AO: Spark/SnoozeParticle, AE: Spark/SnoozeParticle + ThrowableTotal
    virtual void Draw(const Line_G2& line) = 0;

    // SnoozeParticle
    virtual void Draw(const Line_G4& line) = 0;

    // MainMenuTransistion
    virtual void Draw(const Poly_G3& poly) = 0;

    // FG1, Animation, Font, Water
    virtual void Draw(const Poly_FT4& poly) = 0;

    // The screen wave (AO's bell song): moves pieces of the frame drawn so far. The pieces in a run
    // of these all draw from the frame as it was before the first of them. Black, and anything
    // from outside the screen, stays where it is: it isn't drawn over what's already there.
    virtual void Draw(const Prim_ScreenWave& wave) = 0;

    // Fleech (tounge), DeathGas, ColourfulMeter
    virtual void Draw(const Poly_G4& poly) = 0;

    // Recommendations for reserving memory to fit 'peak' amounts of quads
    // during batching:
    //   - For regular Poly_FT4s, the peak tends to be about 300 when the game
    //     is rendering a Spline (chant orb zap made out of ~260 individual
    //     sprites)
    //   - For the screen wave there are 128 quads
    //
    static constexpr s32 kReserveFT4QuadCount = 300;
    static constexpr s32 kReserveScreenWaveQuadCount = 128;

protected:




    // Original game target resolution - 640x480
    static constexpr s32 kTargetFramebufferHeight = 480;
    static constexpr s32 kTargetFramebufferWidth = 640;

protected:
    SDL_Rect GetFramebufferRect();
    SDL_Rect GetTargetDrawRect();

    // Copies the PSX framebuffer out as RGBA32 rows, top row first
    virtual void ReadPsxFramebuffer(std::vector<u8>& rgbaPixels, s32& width, s32& height) = 0;

    // Derived objects call this in EndFrame once everything has been drawn to the PSX framebuffer
    void CaptureIfRequested();

    // Derived objects call this at the start of EndFrame, before drawing what's been queued
    void DrawFrameStats();

    FrameStats mLastFrameStats;

protected:
    bool mIsFirstStartFrame = true;

    Window& mWindow;

    s32 mOffsetX = 0;
    s32 mOffsetY = 0;

    bool mFramebufferFilter = true;
    bool mKeepAspectRatio = true;
    bool mUseOriginalResolution = true;

private:
    std::unique_ptr<FrameStatsOverlay> mFrameStats;

    bool mCaptureRequested = false;
    bool mCaptureReady = false;
    std::vector<u8> mCapturePixels;
    s32 mCaptureWidth = 0;
    s32 mCaptureHeight = 0;
};

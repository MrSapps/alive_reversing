#pragma once

#include "Sdl3Context.hpp"
#include "Sdl3Texture.hpp"
#include "../IRenderer.hpp"
#include "../TextureCache.hpp"

class Sdl3Renderer final : public IRenderer
{
public:
    explicit Sdl3Renderer(Window& window);
    ~Sdl3Renderer() override;

    void Clear(u8 r, u8 g, u8 b) override;
    void Draw(const Prim_GasEffect& gasEffect) override;
    void Draw(const Line_G2& line) override;
    void Draw(const Line_G4& line) override;
    void Draw(const Poly_G3& poly) override;
    void Draw(const Poly_FT4& poly) override;
    void Draw(const Poly_G4& poly) override;
    void EndFrame() override;
    void SetClip(const Prim_ScissorRect& clipper) override;
    void StartFrame() override;

    Renderers GetType() const override
    {
        return Renderers::Sdl3;
    }

protected:
    void ReadPsxFramebuffer(std::vector<u8>& rgbaPixels, s32& width, s32& height) override;

private:
    void DrawLines(const IRenderer::Point2D points[], const SDL_FColor colours[], s32 numPoints, relive::TBlendModes blendMode);
    void DrawVertices(SDL_Vertex vertices[], s32 numVertices, const s32 indices[], s32 numIndices, SDL_Texture* texture, bool isSemiTrans, relive::TBlendModes blendMode);
    Sdl3Texture& GetActiveFbTexture();
    std::shared_ptr<Sdl3Texture> PrepareTextureFromPoly(const Poly_FT4& poly);
    SDL_FPoint PointToViewport(const SDL_FPoint& point);
    void ScaleVertices(SDL_Vertex vertices[], s32 numVertices);
    void SwitchActiveFbTexture();
    void ApplyClip();

private:
    Sdl3Context mContext;

    u8 mActiveFbTexture = 0;
    // Set while drawing a run of framebuffer effect quads (screen wave): they draw from a copy
    // of the frame made when the run started
    bool mFramebufferSnapshotValid = false;
    bool mClipEnabled = false;
    SDL_Rect mClipRect = {};
    Sdl3Texture mPsxFbTexture[2];

    // Laughing gas: the low resolution gas image, a screen sized render target it's stretched
    // into, and the checkerboard it's blended in with (see Draw(const Prim_GasEffect&))
    Sdl3Texture mGasTexture;
    Sdl3Texture mGasTarget;
    SdlTexturePtr mGasMask;

    // The camera the FG1 textures were masked from, and the last one drawn
    u32 mFg1CamId = 0;
    u32 mLastTouchedCamId = 0;

    TextureCache<std::shared_ptr<Sdl3Texture>> mTextureCache;
};

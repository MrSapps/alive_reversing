#pragma once

#include <vector>
#include "Sdl2Context.hpp"
#include "Sdl2Texture.hpp"
#include "../IRenderer.hpp"
#include "../TextureCache.hpp"

class Sdl2Renderer final : public IRenderer
{
public:
    explicit Sdl2Renderer(TWindowHandleType window);
    ~Sdl2Renderer() override;

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

private:
    void DrawLines(const IRenderer::Point2D points[], s32 numPoints, RGBA32 color, relive::TBlendModes blendMode);
    void DrawVertices(SDL_Vertex vertices[], s32 numVertices, const s32 indices[], s32 numIndices, SDL_Texture* texture, bool isSemiTrans, relive::TBlendModes blendMode);
    Sdl2Texture& GetActiveFbTexture();
    std::shared_ptr<Sdl2Texture> PrepareTextureFromPoly(const Poly_FT4& poly);
    SDL_FPoint PointToViewport(const SDL_FPoint& point);
    void ScaleVertices(SDL_Vertex vertices[], s32 numVertices);

private:
    Sdl2Context mContext;

    u8 mActiveFbTexture = 0;
    Sdl2Texture mPsxFbTexture[2];

    Sdl2Texture mGasTexture;
    TextureCache<std::shared_ptr<Sdl2Texture>> mTextureCache;
};

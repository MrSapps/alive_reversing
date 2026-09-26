#include "Batcher.hpp"
#include "OpenGLRenderer.hpp"
#include "../../Primitives.hpp"
#include "../../FG1.hpp"
#include "../../Animation.hpp"
#include "../../../relive_lib/Font.hpp"
#include <cmath>
#include <cstring>

template <typename TextureType, typename RenderBatchType, std::size_t kTextureBatchSize>
void Batcher<TextureType, RenderBatchType, kTextureBatchSize>::PushVertexData(IRenderer::PsxVertexData* pVertData, s32 count, std::shared_ptr<TextureType>& texture, u32 textureResId)
{
    const relive::TBlendModes blendMode = pVertData[0].blendMode;

    // Check if we need to invalidate the existing batched data
    //
    // We need to invalidate the batch when:
    //     - The blend mode switches to/from subtractive blending
    //     - The current batch draws from the framebuffer
    if (
        mConstructingBatch.mSourceIsFramebuffer ||
        (
            mConstructingBatch.mBlendMode != blendMode &&
            mConstructingBatch.mBlendMode != kBatchValueUnset && (mConstructingBatch.mBlendMode == relive::TBlendModes::eBlend_2 || blendMode == relive::TBlendModes::eBlend_2)
        )
    )
    {
        NewBatch();
    }

    if (texture)
    {
        mConstructingBatch.AddTexture(textureResId, mBatchTextures, texture);
    }

    mConstructingBatch.mBlendMode = blendMode;

    // Locate and update texture unit IDs for the buffer data
    const u32 textureIdx = mConstructingBatch.TextureIdxForId(textureResId);
    for (int i = 0; i < count; i++)
    {
        // TODO: Do we even need this now ? (its looked up again in GL atm)
        pVertData[i].textureUnitIndex = textureIdx;
    }

    InsertVertexData(pVertData, count);

    // DEBUGGING: If batching is disabled we invalidate immediately
    bool bNewBatch = !mBatchingEnabled;
    if (!bNewBatch)
    {
        // Note: We could batch flat polys into the textured batch
        // but this will break in DX9 as the shader can only render 1
        // draw type.
        // TODO: Probably there should be a batcher config option to say if we
        // should split batches on changing draw types.
        bNewBatch = mConstructingBatch.mTexturesInBatch >= kTextureBatchSize - 1;
    }

    if (bNewBatch)
    {
        // TODO: With a batch limit of 1 and > 1 batches using the same texture this will still create
        // 2 batches which is wrong, probably need to also not break when we used 1 texture
        // and rendering flat prims
        NewBatch();
    }
}

template <typename TextureType, typename RenderBatchType, std::size_t kTextureBatchSize>
void Batcher<TextureType, RenderBatchType, kTextureBatchSize>::PushFramebufferVertexData(const IRenderer::PsxVertexData* pVertData, s32 count)
{
    // We should invalidate here if the current batch is not drawing using
    // the framebuffer as the source texture
    if (!mConstructingBatch.mSourceIsFramebuffer)
    {
        NewBatch();

        mConstructingBatch.mSourceIsFramebuffer = true;

        // Add entire contents of the screen itself first
        IRenderer::PsxVertexData verts[4] = {
            {0.0f, 0.0f, 127.0f, 127.0f, 127.0f, 0.0f, IRenderer::kPsxFramebufferHeight, IRenderer::PsxDrawMode::DefaultFT4, 0, 0, relive::TBlendModes::eBlend_0, 0, 0},
            {0.0f, IRenderer::kPsxFramebufferHeight, 127.0f, 127.0f, 127.0f, 0.0f, 0.0f, IRenderer::PsxDrawMode::DefaultFT4, 0, 0, relive::TBlendModes::eBlend_0, 0, 0},
            {IRenderer::kPsxFramebufferWidth, 0.0f, 127.0f, 127.0f, 127.0f, IRenderer::kPsxFramebufferWidth, IRenderer::kPsxFramebufferHeight, IRenderer::PsxDrawMode::DefaultFT4, 0, 0, relive::TBlendModes::eBlend_0, 0, 0},
            {IRenderer::kPsxFramebufferWidth, IRenderer::kPsxFramebufferHeight, 127.0f, 127.0f, 127.0f, IRenderer::kPsxFramebufferWidth, 0.0f, IRenderer::PsxDrawMode::DefaultFT4, 0, 0, relive::TBlendModes::eBlend_0, 0, 0}};

        InsertVertexData(verts, ALIVE_COUNTOF(verts));
    }

    InsertVertexData(pVertData, count);

    // DEBUGGING: If batching is disabled we invalidate immediately
    if (!mBatchingEnabled)
    {
        NewBatch();
    }
}

template <typename T>
static void AppendTo(std::vector<T>& to, const T* pFrom, s32 count)
{
    const std::size_t oldSize = to.size();
    to.resize(oldSize + count);
    memcpy(to.data() + oldSize, pFrom, sizeof(T) * count);
}

template <typename TextureType, typename RenderBatchType, std::size_t kTextureBatchSize>
void Batcher<TextureType, RenderBatchType, kTextureBatchSize>::InsertVertexData(const IRenderer::PsxVertexData* pVertData, s32 count)
{
    // Push indicies for this data. Copied in a whole primitive at a time, as this runs for every
    // primitive and std::vector's element by element and range inserts are slow in Debug builds.
    const u32 nextIndex = mIndexBufferIndex;
    const s32 numTriangles = count - 2;

    if (numTriangles == 1)
    {
        const u32 indices[] = {nextIndex, nextIndex + 1, nextIndex + 2};
        AppendTo(mIndices, indices, 3);

        mIndexBufferIndex += 3;
    }
    else if (numTriangles == 2)
    {
        // Split along 1-2 like the PSX does, which matters for gouraud shading
        const u32 indices[] = {
            nextIndex, nextIndex + 1, nextIndex + 2,
            nextIndex + 1, nextIndex + 2, nextIndex + 3};
        AppendTo(mIndices, indices, 6);

        mIndexBufferIndex += 4;
    }

    AppendTo(mVertices, pVertData, count);

    mConstructingBatch.mNumTrisToDraw += numTriangles;

    mBatchInProgress = true;
}

template <typename TextureType, typename RenderBatchType, std::size_t kTextureBatchSize>
void Batcher<TextureType, RenderBatchType, kTextureBatchSize>::PushLines(const IRenderer::PsxVertexData* vertices, s32 count)
{
    const s32 numLines = count - 1;

    for (s32 i = 0; i < numLines; i++)
    {
        const IRenderer::PsxVertexData& vertA = vertices[i];
        const IRenderer::PsxVertexData& vertB = vertices[i + 1];

        const IRenderer::Quad2D quad = IRenderer::LineToQuad(IRenderer::Point2D(vertA.x, vertA.y), IRenderer::Point2D(vertB.x, vertB.y));

        IRenderer::PsxVertexData triangleVerts[4] = {
            {quad.verts[0].x, quad.verts[0].y, vertA.r, vertA.g, vertA.b, 0.0f, 0.0f, vertA.drawMode, vertA.isSemiTrans, vertA.isShaded, vertA.blendMode, 0, 0},
            {quad.verts[1].x, quad.verts[1].y, vertA.r, vertA.g, vertA.b, 0.0f, 0.0f, vertA.drawMode, vertA.isSemiTrans, vertA.isShaded, vertA.blendMode, 0, 0},
            {quad.verts[2].x, quad.verts[2].y, vertB.r, vertB.g, vertB.b, 0.0f, 0.0f, vertB.drawMode, vertB.isSemiTrans, vertB.isShaded, vertB.blendMode, 0, 0},
            {quad.verts[3].x, quad.verts[3].y, vertB.r, vertB.g, vertB.b, 0.0f, 0.0f, vertB.drawMode, vertB.isSemiTrans, vertB.isShaded, vertB.blendMode, 0, 0}};

        std::shared_ptr<TextureType> nullTex;
        PushVertexData(triangleVerts, ALIVE_COUNTOF(triangleVerts), nullTex, 0);
    }
}

template <typename TextureType, typename RenderBatchType, std::size_t kTextureBatchSize>
void Batcher<TextureType, RenderBatchType, kTextureBatchSize>::NewBatch()
{
    const SDL_Rect oldScissor = mConstructingBatch.mScissor;
    // An empty batch would only be an empty draw call. It has no textures, as those are only
    // added along with vertices.
    if (mConstructingBatch.mNumTrisToDraw > 0)
    {
        mBatches.emplace_back(mConstructingBatch);
    }
    mConstructingBatch = {};
    mConstructingBatch.mScissor = oldScissor;
    mBatchInProgress = false;
}

template <typename TextureType, typename RenderBatchType, std::size_t kTextureBatchSize>
void Batcher<TextureType, RenderBatchType, kTextureBatchSize>::SetScissor(const SDL_Rect& scissor)
{
    NewBatch();
    mConstructingBatch.mScissor = scissor;
}

template <typename TextureType, typename RenderBatchType, std::size_t kTextureBatchSize>
void Batcher<TextureType, RenderBatchType, kTextureBatchSize>::PushGas(const Prim_GasEffect& gasEffect)
{
    if (gasEffect.pGasPixels == nullptr)
    {
        return;
    }

    const f32 x = static_cast<f32>(gasEffect.x);
    const f32 y = static_cast<f32>(gasEffect.y);
    const f32 w = static_cast<f32>(gasEffect.w);
    const f32 h = static_cast<f32>(gasEffect.h);

    const f32 r = 127;
    const f32 g = 127;
    const f32 b = 127;

    const f32 gasWidth = std::floor(static_cast<f32>(gasEffect.w - gasEffect.x) / 4);
    const f32 gasHeight = std::floor(static_cast<f32>(gasEffect.h - gasEffect.y) / 2);

    const bool isSemiTrans = true;
    const bool isShaded = true;

    IRenderer::PsxVertexData verts[4] = {
        {x, y, r, g, b, 0.0f, 0.0f, IRenderer::PsxDrawMode::Gas, isSemiTrans, isShaded, relive::TBlendModes::eBlend_0, 0, 0},
        {w, y, r, g, b, gasWidth, 0.0f, IRenderer::PsxDrawMode::Gas, isSemiTrans, isShaded, relive::TBlendModes::eBlend_0, 0, 0},
        {x, h, r, g, b, 0.0f, gasHeight, IRenderer::PsxDrawMode::Gas, isSemiTrans, isShaded, relive::TBlendModes::eBlend_0, 0, 0},
        {w, h, r, g, b, gasWidth, gasHeight, IRenderer::PsxDrawMode::Gas, isSemiTrans, isShaded, relive::TBlendModes::eBlend_0, 0, 0}};

    std::shared_ptr<TextureType> nullTex;
    PushVertexData(verts, ALIVE_COUNTOF(verts), nullTex, 0);
}

// An untextured, gouraud shaded primitive's first N corners. Filled in one go as it's done for
// every flat primitive, and a call per corner is slow in Debug builds.
template <s32 N>
static void FlatVertices(const BasePrimitive& prim, relive::TBlendModes blendMode, IRenderer::PsxVertexData (&verts)[N])
{
    for (s32 i = 0; i < N; i++)
    {
        const Vert& vert = prim.mVerts[i];
        const Prim_RGB& rgb = prim.mRgbs[i];
        verts[i] = {static_cast<f32>(vert.x), static_cast<f32>(vert.y), static_cast<f32>(rgb.r), static_cast<f32>(rgb.g), static_cast<f32>(rgb.b), 0.0f, 0.0f, IRenderer::PsxDrawMode::Flat, prim.mSemiTransparent, true, blendMode, 0, 0};
    }
}

template <typename TextureType, typename RenderBatchType, std::size_t kTextureBatchSize>
void Batcher<TextureType, RenderBatchType, kTextureBatchSize>::PushPolyG4(const Poly_G4& prim, relive::TBlendModes blendMode)
{
    IRenderer::PsxVertexData verts[4];
    FlatVertices(prim, blendMode, verts);

    std::shared_ptr<TextureType> nullTex;
    PushVertexData(verts, ALIVE_COUNTOF(verts), nullTex, 0);
}

template <typename TextureType, typename RenderBatchType, std::size_t kTextureBatchSize>
void Batcher<TextureType, RenderBatchType, kTextureBatchSize>::PushPolyG3(const Poly_G3& prim, relive::TBlendModes blendMode)
{
    IRenderer::PsxVertexData verts[3];
    FlatVertices(prim, blendMode, verts);

    std::shared_ptr<TextureType> nullTex;
    PushVertexData(verts, ALIVE_COUNTOF(verts), nullTex, 0);
}

template <typename TextureType, typename RenderBatchType, std::size_t kTextureBatchSize>
void Batcher<TextureType, RenderBatchType, kTextureBatchSize>::PushLine(const Line_G2& prim, relive::TBlendModes blendMode)
{
    IRenderer::PsxVertexData verts[2];
    FlatVertices(prim, blendMode, verts);

    PushLines(verts, ALIVE_COUNTOF(verts));
}

template <typename TextureType, typename RenderBatchType, std::size_t kTextureBatchSize>
void Batcher<TextureType, RenderBatchType, kTextureBatchSize>::PushLine(const Line_G4& prim, relive::TBlendModes blendMode)
{
    IRenderer::PsxVertexData verts[4];
    FlatVertices(prim, blendMode, verts);

    PushLines(verts, ALIVE_COUNTOF(verts));
}

template <typename TextureType, typename RenderBatchType, std::size_t kTextureBatchSize>
void Batcher<TextureType, RenderBatchType, kTextureBatchSize>::PushSprite(const Poly_FT4& poly, const IRenderer::QuadUVs& uvs, u32 palIndex, std::shared_ptr<TextureType>& texture, u32 textureResId)
{
    const f32 r = static_cast<f32>(poly.R0());
    const f32 g = static_cast<f32>(poly.G0());
    const f32 b = static_cast<f32>(poly.B0());

    const u32 isShaded = poly.mIsShaded;
    const u32 isSemiTrans = poly.mSemiTransparent;
    const relive::TBlendModes blendMode = poly.mBlendMode;

    IRenderer::PsxVertexData verts[4] = {
        {static_cast<f32>(poly.X0()), static_cast<f32>(poly.Y0()), r, g, b, uvs.u0, uvs.v0, IRenderer::PsxDrawMode::DefaultFT4, isSemiTrans, isShaded, blendMode, palIndex, 0},
        {static_cast<f32>(poly.X1()), static_cast<f32>(poly.Y1()), r, g, b, uvs.u1, uvs.v0, IRenderer::PsxDrawMode::DefaultFT4, isSemiTrans, isShaded, blendMode, palIndex, 0},
        {static_cast<f32>(poly.X2()), static_cast<f32>(poly.Y2()), r, g, b, uvs.u0, uvs.v1, IRenderer::PsxDrawMode::DefaultFT4, isSemiTrans, isShaded, blendMode, palIndex, 0},
        {static_cast<f32>(poly.X3()), static_cast<f32>(poly.Y3()), r, g, b, uvs.u1, uvs.v1, IRenderer::PsxDrawMode::DefaultFT4, isSemiTrans, isShaded, blendMode, palIndex, 0}};

    PushVertexData(verts, ALIVE_COUNTOF(verts), texture, textureResId);
}

template <typename TextureType, typename RenderBatchType, std::size_t kTextureBatchSize>
void Batcher<TextureType, RenderBatchType, kTextureBatchSize>::PushFont(const Poly_FT4& poly, u32 palIndex, std::shared_ptr<TextureType>& texture)
{
    PushSprite(poly, IRenderer::GetFontUVs(poly), palIndex, texture, poly.mFont->mFntResource.mUniqueId.Id());
}

template <typename TextureType, typename RenderBatchType, std::size_t kTextureBatchSize>
void Batcher<TextureType, RenderBatchType, kTextureBatchSize>::PushAnim(const Poly_FT4& poly, u32 palIndex, std::shared_ptr<TextureType>& texture)
{
    PushSprite(poly, IRenderer::GetAnimUVs(poly), palIndex, texture, poly.mAnim->mAnimRes.mUniqueId.Id());
}

template <typename TextureType, typename RenderBatchType, std::size_t kTextureBatchSize>
void Batcher<TextureType, RenderBatchType, kTextureBatchSize>::PushScreenImage(const Poly_FT4& poly, IRenderer::PsxDrawMode drawMode, std::shared_ptr<TextureType>& texture, u32 textureResId)
{
    const f32 r = static_cast<f32>(poly.R0());
    const f32 g = static_cast<f32>(poly.G0());
    const f32 b = static_cast<f32>(poly.B0());

    const u32 isShaded = poly.mIsShaded;
    const u32 isSemiTrans = poly.mSemiTransparent;
    const relive::TBlendModes blendMode = poly.mBlendMode;

    constexpr f32 w = IRenderer::kPsxFramebufferWidth;
    constexpr f32 h = IRenderer::kPsxFramebufferHeight;
    IRenderer::PsxVertexData verts[4] = {
        {static_cast<f32>(poly.X0()), static_cast<f32>(poly.Y0()), r, g, b, 0.0f, 0.0f, drawMode, isSemiTrans, isShaded, blendMode, 0, 0},
        {static_cast<f32>(poly.X1()), static_cast<f32>(poly.Y1()), r, g, b, w, 0.0f, drawMode, isSemiTrans, isShaded, blendMode, 0, 0},
        {static_cast<f32>(poly.X2()), static_cast<f32>(poly.Y2()), r, g, b, 0.0f, h, drawMode, isSemiTrans, isShaded, blendMode, 0, 0},
        {static_cast<f32>(poly.X3()), static_cast<f32>(poly.Y3()), r, g, b, w, h, drawMode, isSemiTrans, isShaded, blendMode, 0, 0}};

    PushVertexData(verts, ALIVE_COUNTOF(verts), texture, textureResId);
}

template <typename TextureType, typename RenderBatchType, std::size_t kTextureBatchSize>
void Batcher<TextureType, RenderBatchType, kTextureBatchSize>::PushFG1(const Poly_FT4& poly, std::shared_ptr<TextureType>& texture)
{
    PushScreenImage(poly, IRenderer::PsxDrawMode::FG1, texture, poly.mFg1->mUniqueId.Id());
}

template <typename TextureType, typename RenderBatchType, std::size_t kTextureBatchSize>
void Batcher<TextureType, RenderBatchType, kTextureBatchSize>::PushCAM(const Poly_FT4& poly, std::shared_ptr<TextureType>& texture)
{
    PushScreenImage(poly, IRenderer::PsxDrawMode::Camera, texture, texture ? poly.mCam->mUniqueId.Id() : 0);

    if (texture)
    {
        mCamTexture = texture;
    }
}

template class Batcher<GLTexture2D, OpenGLRenderer::BatchData, 12>;


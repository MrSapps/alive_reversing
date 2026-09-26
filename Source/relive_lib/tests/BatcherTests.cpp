// Unit tests for the OpenGL renderer's Batcher: which primitives are drawn together in one draw
// call, and the indices it makes for them. None of this needs a GL context.

#include "Renderer/OpenGL3/OpenGLRenderer.hpp"
#include <gtest/gtest.h>

using GLBatcher = OpenGLRenderer::GLBatcher;

static IRenderer::PsxVertexData Vertex(relive::TBlendModes blendMode = relive::TBlendModes::eBlend_0)
{
    return {0.0f, 0.0f, 127.0f, 127.0f, 127.0f, 0.0f, 0.0f, IRenderer::PsxDrawMode::Flat, 0, 0, blendMode, 0, 0};
}

// A quad's worth of vertices
static void PushQuad(GLBatcher& batcher, relive::TBlendModes blendMode = relive::TBlendModes::eBlend_0)
{
    IRenderer::PsxVertexData verts[4] = {Vertex(blendMode), Vertex(blendMode), Vertex(blendMode), Vertex(blendMode)};
    std::shared_ptr<GLTexture2D> noTexture;
    batcher.PushVertexData(verts, 4, noTexture, 0);
}

// The batcher only stores and hands back the texture pointers, so they don't have to be real
// textures (which need a GL context). This one points at dummy without owning it.
static std::shared_ptr<GLTexture2D> FakeTexture(u8& dummy)
{
    return std::shared_ptr<GLTexture2D>(std::shared_ptr<void>(), reinterpret_cast<GLTexture2D*>(&dummy));
}

static void PushTexturedQuad(GLBatcher& batcher, std::shared_ptr<GLTexture2D> texture, u32 textureId)
{
    IRenderer::PsxVertexData verts[4] = {Vertex(), Vertex(), Vertex(), Vertex()};
    batcher.PushVertexData(verts, 4, texture, textureId);
}

static GLBatcher StartedBatcher()
{
    GLBatcher batcher;
    batcher.StartFrame();
    return batcher;
}

TEST(Batcher, QuadsAreSplitAlongTheirMiddleDiagonal)
{
    GLBatcher batcher = StartedBatcher();
    PushQuad(batcher);
    PushQuad(batcher);
    batcher.EndFrame();

    // Like the PSX: 0 1 2 and 1 2 3, which matters for gouraud shading
    const std::vector<u32> expected = {0, 1, 2, 1, 2, 3, 4, 5, 6, 5, 6, 7};
    EXPECT_EQ(batcher.mIndices, expected);
    EXPECT_EQ(batcher.mVertices.size(), 8u);
}

TEST(Batcher, TrianglesAreOneTriangle)
{
    GLBatcher batcher = StartedBatcher();
    IRenderer::PsxVertexData verts[3] = {Vertex(), Vertex(), Vertex()};
    std::shared_ptr<GLTexture2D> noTexture;
    batcher.PushVertexData(verts, 3, noTexture, 0);
    PushQuad(batcher);
    batcher.EndFrame();

    const std::vector<u32> expected = {0, 1, 2, 3, 4, 5, 4, 5, 6};
    EXPECT_EQ(batcher.mIndices, expected);
    ASSERT_EQ(batcher.mBatches.size(), 1u);
    EXPECT_EQ(batcher.mBatches[0].mNumTrisToDraw, 3u);
}

TEST(Batcher, UntexturedPrimitivesShareOneBatch)
{
    GLBatcher batcher = StartedBatcher();
    for (s32 i = 0; i < 100; i++)
    {
        PushQuad(batcher);
    }
    batcher.EndFrame();

    ASSERT_EQ(batcher.mBatches.size(), 1u);
    EXPECT_EQ(batcher.mBatches[0].mNumTrisToDraw, 200u);
}

TEST(Batcher, OnlySubtractiveBlendingNeedsItsOwnBatch)
{
    // The other blend modes all use the same GL blend function
    GLBatcher batcher = StartedBatcher();
    PushQuad(batcher, relive::TBlendModes::eBlend_0);
    PushQuad(batcher, relive::TBlendModes::eBlend_1);
    PushQuad(batcher, relive::TBlendModes::eBlend_3);
    PushQuad(batcher, relive::TBlendModes::eBlend_2);
    PushQuad(batcher, relive::TBlendModes::eBlend_2);
    PushQuad(batcher, relive::TBlendModes::eBlend_0);
    batcher.EndFrame();

    ASSERT_EQ(batcher.mBatches.size(), 3u);
    EXPECT_EQ(batcher.mBatches[0].mNumTrisToDraw, 6u);
    EXPECT_EQ(batcher.mBatches[1].mNumTrisToDraw, 4u);
    EXPECT_EQ(batcher.mBatches[1].mBlendMode, relive::TBlendModes::eBlend_2);
    EXPECT_EQ(batcher.mBatches[2].mNumTrisToDraw, 2u);
}

TEST(Batcher, ABatchHasOneTextureUnitLessThanItCouldUse)
{
    u8 dummies[20] = {};
    GLBatcher batcher = StartedBatcher();

    // Drawing with a texture again doesn't use up another unit
    for (u32 i = 0; i < 10; i++)
    {
        PushTexturedQuad(batcher, FakeTexture(dummies[i]), i + 1);
        PushTexturedQuad(batcher, FakeTexture(dummies[i]), i + 1);
    }
    EXPECT_EQ(batcher.mBatches.size(), 0u);

    // The 11th texture fills the batch
    PushTexturedQuad(batcher, FakeTexture(dummies[10]), 11);
    ASSERT_EQ(batcher.mBatches.size(), 1u);
    EXPECT_EQ(batcher.mBatches[0].mTexturesInBatch, 11u);

    PushTexturedQuad(batcher, FakeTexture(dummies[11]), 12);
    batcher.EndFrame();

    ASSERT_EQ(batcher.mBatches.size(), 2u);
    EXPECT_EQ(batcher.mBatches[1].mTexturesInBatch, 1u);

    // Each batch's textures follow on from the last's, in the order they were first drawn
    ASSERT_EQ(batcher.mBatchTextures.size(), 12u);
    for (u32 i = 0; i < 12; i++)
    {
        EXPECT_EQ(batcher.mBatchTextures[i].get(), reinterpret_cast<GLTexture2D*>(&dummies[i]));
    }
}

TEST(Batcher, VerticesSayWhichOfTheBatchsTexturesTheyUse)
{
    u8 dummies[2] = {};
    GLBatcher batcher = StartedBatcher();
    PushTexturedQuad(batcher, FakeTexture(dummies[0]), 100);
    PushTexturedQuad(batcher, FakeTexture(dummies[1]), 200);
    PushTexturedQuad(batcher, FakeTexture(dummies[0]), 100);
    batcher.EndFrame();

    ASSERT_EQ(batcher.mVertices.size(), 12u);
    EXPECT_EQ(batcher.mVertices[0].textureUnitIndex, 0u);
    EXPECT_EQ(batcher.mVertices[4].textureUnitIndex, 1u);
    EXPECT_EQ(batcher.mVertices[8].textureUnitIndex, 0u);
}

TEST(Batcher, FramebufferEffectsGetTheirOwnBatch)
{
    GLBatcher batcher = StartedBatcher();
    PushQuad(batcher);

    // A run of screen wave pieces all go in one batch, which starts with a copy of the whole frame
    IRenderer::PsxVertexData wave[4] = {Vertex(), Vertex(), Vertex(), Vertex()};
    batcher.PushFramebufferVertexData(wave, 4);
    batcher.PushFramebufferVertexData(wave, 4);

    PushQuad(batcher);
    batcher.EndFrame();

    ASSERT_EQ(batcher.mBatches.size(), 3u);
    EXPECT_FALSE(batcher.mBatches[0].mSourceIsFramebuffer);
    EXPECT_TRUE(batcher.mBatches[1].mSourceIsFramebuffer);
    EXPECT_EQ(batcher.mBatches[1].mNumTrisToDraw, 6u);
    EXPECT_FALSE(batcher.mBatches[2].mSourceIsFramebuffer);

    const IRenderer::PsxVertexData& wholeFrame = batcher.mVertices[4];
    EXPECT_EQ(wholeFrame.x, 0.0f);
    EXPECT_EQ(wholeFrame.v, static_cast<f32>(IRenderer::kPsxFramebufferHeight));
    EXPECT_EQ(batcher.mVertices[7].x, static_cast<f32>(IRenderer::kPsxFramebufferWidth));
}

TEST(Batcher, AClipRectangleStartsABatchAndCarriesOn)
{
    GLBatcher batcher = StartedBatcher();
    PushQuad(batcher);

    const SDL_Rect clip = {10, 20, 30, 40};
    batcher.SetScissor(clip);
    PushQuad(batcher, relive::TBlendModes::eBlend_0);
    PushQuad(batcher, relive::TBlendModes::eBlend_2);

    batcher.SetScissor({});
    PushQuad(batcher);
    batcher.EndFrame();

    ASSERT_EQ(batcher.mBatches.size(), 4u);
    EXPECT_EQ(batcher.mBatches[0].mScissor.w, 0);
    EXPECT_EQ(batcher.mBatches[1].mScissor.x, 10);
    EXPECT_EQ(batcher.mBatches[1].mScissor.h, 40);
    EXPECT_EQ(batcher.mBatches[2].mScissor.x, 10);
    EXPECT_EQ(batcher.mBatches[2].mScissor.h, 40);
    EXPECT_EQ(batcher.mBatches[3].mScissor.w, 0);
}

TEST(Batcher, NothingDrawnMeansNoBatch)
{
    GLBatcher batcher = StartedBatcher();
    batcher.SetScissor({1, 2, 3, 4});
    batcher.SetScissor({5, 6, 7, 8});
    PushQuad(batcher);
    batcher.SetScissor({});
    batcher.SetScissor({});
    batcher.EndFrame();

    ASSERT_EQ(batcher.mBatches.size(), 1u);
    EXPECT_EQ(batcher.mBatches[0].mScissor.x, 5);
}

TEST(Batcher, StartFrameForgetsTheLastFrame)
{
    u8 dummy = 0;
    GLBatcher batcher = StartedBatcher();
    PushTexturedQuad(batcher, FakeTexture(dummy), 1);
    batcher.EndFrame();

    batcher.StartFrame();
    EXPECT_TRUE(batcher.mBatches.empty());
    EXPECT_TRUE(batcher.mBatchTextures.empty());
    EXPECT_TRUE(batcher.mVertices.empty());
    EXPECT_TRUE(batcher.mIndices.empty());

    // Indices start from 0 again
    PushQuad(batcher);
    EXPECT_EQ(batcher.mIndices[0], 0u);
}

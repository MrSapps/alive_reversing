// Unit tests for TextureCache: how long the renderers keep a texture after it was last drawn

#include "Renderer/TextureCache.hpp"
#include <gtest/gtest.h>
#include <memory>

using TestCache = TextureCache<std::shared_ptr<int>>;

// A texture with a lifetime of n is still there after n frames and gone after n + 1
static void EndFrames(TestCache& cache, s32 count)
{
    for (s32 i = 0; i < count; i++)
    {
        cache.DecreaseResourceLifetimes();
    }
}

TEST(TextureCache, MissingTexturesAreNull)
{
    TestCache cache;
    EXPECT_EQ(cache.GetCachedTexture(1, 5), nullptr);
    EXPECT_EQ(cache.Size(), 0u);
}

TEST(TextureCache, AddedTexturesAreFound)
{
    TestCache cache;
    auto texture = std::make_shared<int>(42);

    EXPECT_EQ(cache.Add(7, 5, texture), texture);
    EXPECT_EQ(cache.GetCachedTexture(7, 0), texture);
    EXPECT_EQ(cache.GetCachedTexture(8, 0), nullptr);
    EXPECT_EQ(cache.Size(), 1u);
}

TEST(TextureCache, UnusedTexturesLastTheirLifetime)
{
    TestCache cache;
    cache.Add(1, 3, std::make_shared<int>(1));

    EndFrames(cache, 3);
    EXPECT_NE(cache.GetCachedTexture(1, 0), nullptr);

    EndFrames(cache, 1);
    EXPECT_EQ(cache.GetCachedTexture(1, 0), nullptr);
    EXPECT_EQ(cache.Size(), 0u);
}

TEST(TextureCache, UsingATextureResetsItsLifetime)
{
    TestCache cache;
    cache.Add(1, 3, std::make_shared<int>(1));

    EndFrames(cache, 3);
    EXPECT_NE(cache.GetCachedTexture(1, 3), nullptr);

    EndFrames(cache, 3);
    EXPECT_NE(cache.GetCachedTexture(1, 0), nullptr);

    EndFrames(cache, 1);
    EXPECT_EQ(cache.GetCachedTexture(1, 0), nullptr);
}

TEST(TextureCache, ABumpOfZeroLeavesTheLifetimeAlone)
{
    TestCache cache;
    cache.Add(1, 2, std::make_shared<int>(1));

    EndFrames(cache, 2);
    EXPECT_NE(cache.GetCachedTexture(1, 0), nullptr);

    EndFrames(cache, 1);
    EXPECT_EQ(cache.GetCachedTexture(1, 0), nullptr);
}

TEST(TextureCache, GetOrAddOnlyCreatesOnce)
{
    TestCache cache;
    s32 created = 0;
    auto create = [&]()
    {
        created++;
        return std::make_shared<int>(created);
    };

    auto first = cache.GetOrAdd(1, 2, create);
    auto second = cache.GetOrAdd(1, 2, create);
    EXPECT_EQ(created, 1);
    EXPECT_EQ(first, second);

    cache.GetOrAdd(2, 2, create);
    EXPECT_EQ(created, 2);
    EXPECT_EQ(cache.Size(), 2u);
}

TEST(TextureCache, GetOrAddResetsTheLifetime)
{
    TestCache cache;
    auto create = []()
    {
        return std::make_shared<int>(1);
    };

    auto texture = cache.GetOrAdd(1, 2, create);
    EndFrames(cache, 2);
    EXPECT_EQ(cache.GetOrAdd(1, 2, create), texture);

    EndFrames(cache, 2);
    EXPECT_EQ(cache.GetCachedTexture(1, 0), texture);

    // Recreated once it has expired
    EndFrames(cache, 1);
    EXPECT_NE(cache.GetOrAdd(1, 2, create), texture);
}

TEST(TextureCache, ClearRemovesEverything)
{
    TestCache cache;
    cache.Add(1, 5, std::make_shared<int>(1));
    cache.Add(2, 5, std::make_shared<int>(2));

    cache.Clear();
    EXPECT_EQ(cache.Size(), 0u);
    EXPECT_EQ(cache.GetCachedTexture(1, 0), nullptr);
}

// Unit tests for PaletteCache: which slot of the OpenGL renderer's palette texture each palette
// is drawn with, and when a palette has to be uploaded again

#include "Renderer/PaletteCache.hpp"
#include <gtest/gtest.h>

static AnimationPal MakePalette(u8 seed)
{
    AnimationPal pal;
    for (u32 i = 0; i < 256; i++)
    {
        pal.mPal[i] = {static_cast<u8>(seed + i), static_cast<u8>(seed * 3 + i), static_cast<u8>(seed * 7), 255};
    }
    return pal;
}

TEST(PaletteCache, APaletteIsOnlyAllocatedOnce)
{
    PaletteCache cache(4);
    AnimationPal pal = MakePalette(1);

    const PaletteCache::AddResult first = cache.Add(pal);
    EXPECT_TRUE(first.mAllocated);

    const PaletteCache::AddResult again = cache.Add(pal);
    EXPECT_FALSE(again.mAllocated);
    EXPECT_EQ(again.mIndex, first.mIndex);

    // Still there next frame
    cache.ResetUseFlags();
    const PaletteCache::AddResult nextFrame = cache.Add(pal);
    EXPECT_FALSE(nextFrame.mAllocated);
    EXPECT_EQ(nextFrame.mIndex, first.mIndex);
}

TEST(PaletteCache, DifferentColoursGetDifferentSlots)
{
    PaletteCache cache(4);
    AnimationPal a = MakePalette(1);
    AnimationPal b = MakePalette(2);

    const PaletteCache::AddResult resultA = cache.Add(a);
    const PaletteCache::AddResult resultB = cache.Add(b);
    EXPECT_TRUE(resultA.mAllocated);
    EXPECT_TRUE(resultB.mAllocated);
    EXPECT_NE(resultA.mIndex, resultB.mIndex);
}

TEST(PaletteCache, TheSameColoursShareASlot)
{
    PaletteCache cache(4);
    AnimationPal a = MakePalette(1);
    AnimationPal copyOfA = a;

    const PaletteCache::AddResult resultA = cache.Add(a);
    const PaletteCache::AddResult resultCopy = cache.Add(copyOfA);
    EXPECT_FALSE(resultCopy.mAllocated);
    EXPECT_EQ(resultCopy.mIndex, resultA.mIndex);
}

TEST(PaletteCache, ChangedColoursAreUploadedNextFrame)
{
    PaletteCache cache(4);
    AnimationPal pal = MakePalette(1);
    const PaletteCache::AddResult before = cache.Add(pal);

    // Palettes are only changed between frames, by the game objects' updates
    cache.ResetUseFlags();
    pal.mPal[10] = {1, 2, 3, 255};

    const PaletteCache::AddResult after = cache.Add(pal);
    EXPECT_TRUE(after.mAllocated);
    EXPECT_NE(after.mIndex, before.mIndex);
}

TEST(PaletteCache, AChangeWithinAFrameIsNotSeen)
{
    // Each palette is only looked up once a frame, as hashing it is most of the cost of a sprite
    PaletteCache cache(4);
    AnimationPal pal = MakePalette(1);
    const PaletteCache::AddResult before = cache.Add(pal);

    pal.mPal[10] = {1, 2, 3, 255};

    const PaletteCache::AddResult after = cache.Add(pal);
    EXPECT_FALSE(after.mAllocated);
    EXPECT_EQ(after.mIndex, before.mIndex);
}

TEST(PaletteCache, WhenFullAPaletteNotUsedThisFrameLosesItsSlot)
{
    PaletteCache cache(2);
    AnimationPal a = MakePalette(1);
    AnimationPal b = MakePalette(2);
    AnimationPal c = MakePalette(3);

    const u32 indexA = cache.Add(a).mIndex;
    const u32 indexB = cache.Add(b).mIndex;

    // Next frame only b is drawn, so c takes a's slot
    cache.ResetUseFlags();
    EXPECT_EQ(cache.Add(b).mIndex, indexB);
    const PaletteCache::AddResult resultC = cache.Add(c);
    EXPECT_TRUE(resultC.mAllocated);
    EXPECT_EQ(resultC.mIndex, indexA);

    // b kept its slot, and a has to be uploaded again
    EXPECT_EQ(cache.Add(b).mIndex, indexB);
    cache.ResetUseFlags();
    EXPECT_EQ(cache.Add(c).mIndex, indexA);
    const PaletteCache::AddResult resultA = cache.Add(a);
    EXPECT_TRUE(resultA.mAllocated);
    EXPECT_EQ(resultA.mIndex, indexB);
}

TEST(PaletteCache, ClearForgetsEverything)
{
    PaletteCache cache(4);
    AnimationPal pal = MakePalette(1);
    cache.Add(pal);

    cache.Clear();
    EXPECT_TRUE(cache.Add(pal).mAllocated);
}

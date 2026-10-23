// Unit tests for SpriteFxCache: which slot of the OpenGL renderer's table of sprite effects each
// effect is drawn with, and when an effect has to be uploaded again

#include "Renderer/SpriteFxCache.hpp"
#include <gtest/gtest.h>

static SpriteFx MakeFx(f32 seed)
{
    SpriteFx fx;
    fx.mSubtract = seed;
    return fx;
}

TEST(SpriteFxCache, AnEffectIsOnlyAllocatedOnce)
{
    SpriteFxCache cache(4);
    const SpriteFx fx = MakeFx(0.1f);

    const SpriteFxCache::AddResult first = cache.Add(fx);
    EXPECT_TRUE(first.mAllocated);

    const SpriteFxCache::AddResult again = cache.Add(fx);
    EXPECT_FALSE(again.mAllocated);
    EXPECT_EQ(again.mIndex, first.mIndex);

    // Still there next frame
    cache.ResetUseFlags();
    const SpriteFxCache::AddResult nextFrame = cache.Add(fx);
    EXPECT_FALSE(nextFrame.mAllocated);
    EXPECT_EQ(nextFrame.mIndex, first.mIndex);
}

TEST(SpriteFxCache, DifferentEffectsGetDifferentSlots)
{
    SpriteFxCache cache(4);

    const SpriteFxCache::AddResult resultA = cache.Add(MakeFx(0.1f));
    const SpriteFxCache::AddResult resultB = cache.Add(MakeFx(0.2f));
    EXPECT_TRUE(resultA.mAllocated);
    EXPECT_TRUE(resultB.mAllocated);
    EXPECT_NE(resultA.mIndex, resultB.mIndex);
}

TEST(SpriteFxCache, TheSameEffectShareASlot)
{
    SpriteFxCache cache(4);
    SpriteFx a = MakeFx(0.1f);
    a.mSweeps[1].mProgress = 0.5f;
    SpriteFx copyOfA = a;

    const SpriteFxCache::AddResult resultA = cache.Add(a);
    const SpriteFxCache::AddResult resultCopy = cache.Add(copyOfA);
    EXPECT_FALSE(resultCopy.mAllocated);
    EXPECT_EQ(resultCopy.mIndex, resultA.mIndex);
}

TEST(SpriteFxCache, AChangedEffectIsUploaded)
{
    SpriteFxCache cache(4);
    SpriteFx fx = MakeFx(0.1f);
    const SpriteFxCache::AddResult before = cache.Add(fx);

    fx.mSweeps[0].mProgress = 0.25f;

    const SpriteFxCache::AddResult after = cache.Add(fx);
    EXPECT_TRUE(after.mAllocated);
    EXPECT_NE(after.mIndex, before.mIndex);
}

TEST(SpriteFxCache, WhenFullAnEffectNotUsedThisFrameLosesItsSlot)
{
    SpriteFxCache cache(2);
    const SpriteFx a = MakeFx(0.1f);
    const SpriteFx b = MakeFx(0.2f);
    const SpriteFx c = MakeFx(0.3f);

    const u32 indexA = cache.Add(a).mIndex;
    const u32 indexB = cache.Add(b).mIndex;

    // Next frame only b is drawn, so c takes a's slot
    cache.ResetUseFlags();
    EXPECT_EQ(cache.Add(b).mIndex, indexB);
    const SpriteFxCache::AddResult resultC = cache.Add(c);
    EXPECT_TRUE(resultC.mAllocated);
    EXPECT_EQ(resultC.mIndex, indexA);

    // b kept its slot, and a has to be uploaded again
    EXPECT_EQ(cache.Add(b).mIndex, indexB);
    cache.ResetUseFlags();
    EXPECT_EQ(cache.Add(c).mIndex, indexA);
    const SpriteFxCache::AddResult resultA = cache.Add(a);
    EXPECT_TRUE(resultA.mAllocated);
    EXPECT_EQ(resultA.mIndex, indexB);
}

TEST(SpriteFxCache, ClearForgetsEverything)
{
    SpriteFxCache cache(4);
    const SpriteFx fx = MakeFx(0.1f);
    cache.Add(fx);

    cache.Clear();
    EXPECT_TRUE(cache.Add(fx).mAllocated);
}

TEST(SpriteFx, TheDefaultIsNoChange)
{
    SpriteFx fx;
    EXPECT_TRUE(fx.IsIdentity());

    fx.mSubtract = 0.1f;
    EXPECT_FALSE(fx.IsIdentity());

    fx = SpriteFx{};
    fx.mMatrix[0][0] = 0.5f;
    EXPECT_FALSE(fx.IsIdentity());
    fx.ResetMatrix();
    EXPECT_TRUE(fx.IsIdentity());
}

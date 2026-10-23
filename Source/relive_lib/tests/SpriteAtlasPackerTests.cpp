#include <gtest/gtest.h>
#include "../SpriteAtlasPacker.hpp"

using Item = SpriteAtlasPacker::Item;
using FrameSize = SpriteAtlasPacker::FrameSize;

static Item MakeItem(u32 group, std::initializer_list<FrameSize> frames)
{
    Item item;
    item.mFrames = frames;
    item.mGroup = group;
    return item;
}

struct PlacedFrame final
{
    u32 mPage;
    u32 mX;
    u32 mY;
    u32 mW;
    u32 mH;
};

static bool Overlaps(const PlacedFrame& a, const PlacedFrame& b)
{
    return a.mPage == b.mPage && a.mX < b.mX + b.mW && b.mX < a.mX + a.mW && a.mY < b.mY + b.mH && b.mY < a.mY + a.mH;
}

static void ExpectValid(const std::vector<Item>& items, const SpriteAtlasPacker::Result& result, u32 pageSize)
{
    ASSERT_EQ(result.mPlacements.size(), items.size());
    std::vector<PlacedFrame> placed;
    for (std::size_t i = 0; i < items.size(); i++)
    {
        const auto& p = result.mPlacements[i];
        if (p.mPage == SpriteAtlasPacker::kNoPage)
        {
            continue;
        }
        EXPECT_LT(p.mPage, result.mPageCount);
        ASSERT_EQ(p.mFrames.size(), items[i].mFrames.size());
        for (std::size_t f = 0; f < p.mFrames.size(); f++)
        {
            const FrameSize& size = items[i].mFrames[f];
            if (size.mWidth == 0 || size.mHeight == 0)
            {
                continue;
            }
            EXPECT_LE(p.mFrames[f].mX + size.mWidth, pageSize);
            EXPECT_LE(p.mFrames[f].mY + size.mHeight, pageSize);
            // The gap is kept, so the rectangles are checked with it
            placed.push_back({p.mPage, p.mFrames[f].mX, p.mFrames[f].mY, size.mWidth + SpriteAtlasPacker::kGap, size.mHeight + SpriteAtlasPacker::kGap});
        }
    }

    for (std::size_t a = 0; a < placed.size(); a++)
    {
        for (std::size_t b = a + 1; b < placed.size(); b++)
        {
            EXPECT_FALSE(Overlaps(placed[a], placed[b])) << a << " overlaps " << b;
        }
    }
}

TEST(SpriteAtlasPacker, EmptyHasNoPages)
{
    const auto result = SpriteAtlasPacker::Pack({}, 256);
    EXPECT_EQ(result.mPageCount, 0u);
    EXPECT_TRUE(result.mPlacements.empty());
}

TEST(SpriteAtlasPacker, FramesDoNotOverlapAndStayOnThePage)
{
    std::vector<Item> items;
    for (u32 i = 0; i < 200; i++)
    {
        Item item;
        item.mGroup = i % 5;
        for (u32 f = 0; f < 1 + i % 4; f++)
        {
            item.mFrames.push_back({8 + (i * 7 + f * 3) % 60, 8 + (i * 13 + f * 5) % 50});
        }
        items.push_back(item);
    }
    const auto result = SpriteAtlasPacker::Pack(items, 256);
    ExpectValid(items, result, 256);
    for (const auto& p : result.mPlacements)
    {
        EXPECT_NE(p.mPage, SpriteAtlasPacker::kNoPage);
    }
}

TEST(SpriteAtlasPacker, FramesOfAnItemAreOnOnePage)
{
    // Many items, so some pages fill up and the items after that have to go elsewhere whole
    std::vector<Item> items(40, MakeItem(1, {{100, 90}, {60, 70}, {30, 120}}));
    const auto result = SpriteAtlasPacker::Pack(items, 256);
    EXPECT_GT(result.mPageCount, 1u);
    ExpectValid(items, result, 256);
}

TEST(SpriteAtlasPacker, ItemWithAFrameBiggerThanAPageHasNoPage)
{
    const auto result = SpriteAtlasPacker::Pack({MakeItem(0, {{300, 10}}), MakeItem(0, {{10, 10}})}, 256);
    EXPECT_EQ(result.mPlacements[0].mPage, SpriteAtlasPacker::kNoPage);
    EXPECT_NE(result.mPlacements[1].mPage, SpriteAtlasPacker::kNoPage);
}

TEST(SpriteAtlasPacker, ItemTooBigForAPageHasNoPage)
{
    // Every frame fits, all of them together don't
    const auto result = SpriteAtlasPacker::Pack({MakeItem(0, {{200, 200}, {200, 200}})}, 256);
    EXPECT_EQ(result.mPlacements[0].mPage, SpriteAtlasPacker::kNoPage);
}

TEST(SpriteAtlasPacker, ItemWithoutPixelsHasNoPage)
{
    const auto result = SpriteAtlasPacker::Pack({MakeItem(0, {{0, 10}}), MakeItem(0, {})}, 256);
    EXPECT_EQ(result.mPlacements[0].mPage, SpriteAtlasPacker::kNoPage);
    EXPECT_EQ(result.mPlacements[1].mPage, SpriteAtlasPacker::kNoPage);
    EXPECT_EQ(result.mPageCount, 0u);
}

TEST(SpriteAtlasPacker, SmallGroupsShareAPage)
{
    std::vector<Item> items = {MakeItem(1, {{64, 64}}), MakeItem(2, {{64, 64}}), MakeItem(3, {{64, 64}})};
    const auto result = SpriteAtlasPacker::Pack(items, 256);
    EXPECT_EQ(result.mPageCount, 1u);
    ExpectValid(items, result, 256);
}

TEST(SpriteAtlasPacker, AGroupIsNotSplitIfItFitsOnAPage)
{
    // Group 1 fills 3/4 of the first page, group 2 doesn't fit in the rest: it goes together on
    // a page of its own
    std::vector<Item> items = {MakeItem(1, {{255, 191}}), MakeItem(2, {{127, 127}}), MakeItem(2, {{127, 127}})};
    const auto result = SpriteAtlasPacker::Pack(items, 256);
    EXPECT_EQ(result.mPageCount, 2u);
    EXPECT_EQ(result.mPlacements[1].mPage, result.mPlacements[2].mPage);
    EXPECT_NE(result.mPlacements[0].mPage, result.mPlacements[1].mPage);
    ExpectValid(items, result, 256);
}

TEST(SpriteAtlasPacker, AGroupBiggerThanAPageSpillsOver)
{
    std::vector<Item> items(6, MakeItem(7, {{127, 127}}));
    const auto result = SpriteAtlasPacker::Pack(items, 256);
    EXPECT_EQ(result.mPageCount, 2u);
    ExpectValid(items, result, 256);
}

TEST(SpriteAtlasPacker, SmallFramesFillTheHolesOfBigOnes)
{
    // One tall frame and many small ones: shelves would give the small ones their own rows
    // beside it, free rectangles pack them all into the page
    Item item;
    item.mFrames.push_back({60, 250});
    for (u32 i = 0; i < 30; i++)
    {
        item.mFrames.push_back({30, 30});
    }
    const std::vector<Item> items = {item};
    const auto result = SpriteAtlasPacker::Pack(items, 256);
    EXPECT_EQ(result.mPageCount, 1u);
    ExpectValid(items, result, 256);
}

TEST(SpriteAtlasPacker, FillsAPageWithFramesOfOneSize)
{
    // 8 x 8 frames of 31 pixels and a gap each fill the 256 pixel page exactly
    Item item;
    item.mFrames.assign(64, {31, 31});
    const std::vector<Item> items = {item};
    const auto result = SpriteAtlasPacker::Pack(items, 256);
    EXPECT_EQ(result.mPageCount, 1u);
    EXPECT_NE(result.mPlacements[0].mPage, SpriteAtlasPacker::kNoPage);
    ExpectValid(items, result, 256);
}

TEST(SpriteAtlasPacker, DoesNotDependOnTheOrderOfTheItems)
{
    std::vector<Item> items = {MakeItem(2, {{32, 40}}), MakeItem(1, {{64, 16}}), MakeItem(2, {{20, 20}}), MakeItem(1, {{50, 60}}), MakeItem(3, {{10, 10}})};
    const auto a = SpriteAtlasPacker::Pack(items, 128);

    std::vector<Item> reversed(items.rbegin(), items.rend());
    const auto b = SpriteAtlasPacker::Pack(reversed, 128);

    ASSERT_EQ(a.mPageCount, b.mPageCount);
    for (std::size_t i = 0; i < items.size(); i++)
    {
        const auto& pa = a.mPlacements[i];
        const auto& pb = b.mPlacements[items.size() - 1 - i];
        EXPECT_EQ(pa.mPage, pb.mPage);
        ASSERT_EQ(pa.mFrames.size(), pb.mFrames.size());
        for (std::size_t f = 0; f < pa.mFrames.size(); f++)
        {
            EXPECT_EQ(pa.mFrames[f].mX, pb.mFrames[f].mX);
            EXPECT_EQ(pa.mFrames[f].mY, pb.mFrames[f].mY);
        }
    }
}

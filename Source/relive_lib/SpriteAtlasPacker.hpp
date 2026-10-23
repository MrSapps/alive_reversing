#pragma once

#include "Types.hpp"
#include <vector>

// Works out where the frames of sprite sheets go in the pages of a sprite atlas. A page is a
// square of pageSize pixels, filled with free rectangles (MaxRects), so frames of every size sit
// next to each other and the page has few holes. All the frames of an item (an animation) go on
// one page, because the animation uses one texture. Items of one group (the animations of a BAN
// file, which a level loads together) share pages with each other and are kept together on as few
// pages as they fit on, so a page is only needed on the GPU while its group is in use. Small
// groups share a page with each other. This is all just arithmetic, the pixels aren't touched:
// see SpriteAtlas.hpp for the atlas itself.
class SpriteAtlasPacker final
{
public:
    static constexpr u32 kNoPage = 0xFFFFFFFFu;

    // The empty pixels kept to the right of and below every frame, so a sampler never reads a
    // neighbour's pixels
    static constexpr u32 kGap = 1;

    struct FrameSize final
    {
        u32 mWidth = 0;
        u32 mHeight = 0;
    };

    struct Item final
    {
        // The frames to place. A frame without pixels is left out.
        std::vector<FrameSize> mFrames;
        // Items with the same group are packed together
        u32 mGroup = 0;
    };

    struct Position final
    {
        u32 mX = 0;
        u32 mY = 0;
    };

    struct Placement final
    {
        // kNoPage if a frame of the item is bigger than a page, or the item doesn't fit on one
        u32 mPage = kNoPage;
        // Where each frame of the item is on the page, in the order of the item's frames
        std::vector<Position> mFrames;
    };

    struct Result final
    {
        // The same order as the items
        std::vector<Placement> mPlacements;
        u32 mPageCount = 0;
    };

    static Result Pack(const std::vector<Item>& items, u32 pageSize);
};

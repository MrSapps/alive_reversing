#include "SpriteAtlasPacker.hpp"
#include <algorithm>
#include <map>

struct PackerRect final
{
    u32 mX = 0;
    u32 mY = 0;
    u32 mW = 0;
    u32 mH = 0;
};

// A page being filled: the rectangles that are still empty (they overlap each other, each one is
// as big as it can be)
struct PackerPage final
{
    explicit PackerPage(u32 pageSize)
    {
        // The gap after a frame may hang over the edge of the page: only the frame has to be on it
        mFree.push_back({0, 0, pageSize + SpriteAtlasPacker::kGap, pageSize + SpriteAtlasPacker::kGap});
    }

    std::vector<PackerRect> mFree;
};

static bool Contains(const PackerRect& outer, const PackerRect& inner)
{
    return inner.mX >= outer.mX && inner.mY >= outer.mY && inner.mX + inner.mW <= outer.mX + outer.mW && inner.mY + inner.mH <= outer.mY + outer.mH;
}

// Takes the used rectangle out of every free one it cuts into
static void Occupy(PackerPage& page, const PackerRect& used)
{
    std::vector<PackerRect> result;
    result.reserve(page.mFree.size() + 4);
    for (const PackerRect& free : page.mFree)
    {
        if (used.mX >= free.mX + free.mW || used.mX + used.mW <= free.mX || used.mY >= free.mY + free.mH || used.mY + used.mH <= free.mY)
        {
            result.push_back(free);
            continue;
        }

        if (used.mX > free.mX)
        {
            result.push_back({free.mX, free.mY, used.mX - free.mX, free.mH});
        }
        if (used.mX + used.mW < free.mX + free.mW)
        {
            result.push_back({used.mX + used.mW, free.mY, free.mX + free.mW - (used.mX + used.mW), free.mH});
        }
        if (used.mY > free.mY)
        {
            result.push_back({free.mX, free.mY, free.mW, used.mY - free.mY});
        }
        if (used.mY + used.mH < free.mY + free.mH)
        {
            result.push_back({free.mX, used.mY + used.mH, free.mW, free.mY + free.mH - (used.mY + used.mH)});
        }
    }

    // Drop the rectangles that lie inside another one
    page.mFree.clear();
    for (std::size_t i = 0; i < result.size(); i++)
    {
        bool inside = false;
        for (std::size_t j = 0; j < result.size() && !inside; j++)
        {
            if (i != j && Contains(result[j], result[i]) && (!Contains(result[i], result[j]) || j < i))
            {
                inside = true;
            }
        }
        if (!inside)
        {
            page.mFree.push_back(result[i]);
        }
    }
}

// Puts a frame in the free rectangle it leaves the least room around (the shorter side first),
// the one nearest the top left if that's the same
static bool TryPlace(PackerPage& page, u32 width, u32 height, u32& x, u32& y)
{
    const u32 w = width + SpriteAtlasPacker::kGap;
    const u32 h = height + SpriteAtlasPacker::kGap;

    const PackerRect* pBest = nullptr;
    u32 bestShort = 0;
    u32 bestLong = 0;
    for (const PackerRect& free : page.mFree)
    {
        if (free.mW < w || free.mH < h)
        {
            continue;
        }

        const u32 leftW = free.mW - w;
        const u32 leftH = free.mH - h;
        const u32 shortSide = std::min(leftW, leftH);
        const u32 longSide = std::max(leftW, leftH);
        const bool better = !pBest || shortSide < bestShort || (shortSide == bestShort && longSide < bestLong)
                         || (shortSide == bestShort && longSide == bestLong && (free.mY < pBest->mY || (free.mY == pBest->mY && free.mX < pBest->mX)));
        if (better)
        {
            pBest = &free;
            bestShort = shortSide;
            bestLong = longSide;
        }
    }

    if (!pBest)
    {
        return false;
    }

    x = pBest->mX;
    y = pBest->mY;
    Occupy(page, {x, y, w, h});
    return true;
}

// All the frames of an item on one page or none of it (the page is left as it was). The frames
// go in biggest first.
static bool TryPlaceItem(PackerPage& page, const SpriteAtlasPacker::Item& item, SpriteAtlasPacker::Placement& placement)
{
    std::vector<u32> order;
    for (u32 f = 0; f < item.mFrames.size(); f++)
    {
        if (item.mFrames[f].mWidth != 0 && item.mFrames[f].mHeight != 0)
        {
            order.push_back(f);
        }
    }
    std::stable_sort(order.begin(), order.end(), [&](u32 a, u32 b)
        {
            const auto& fa = item.mFrames[a];
            const auto& fb = item.mFrames[b];
            return fa.mHeight != fb.mHeight ? fa.mHeight > fb.mHeight : fa.mWidth > fb.mWidth;
        });

    PackerPage attempt = page;
    std::vector<SpriteAtlasPacker::Position> positions(item.mFrames.size());
    for (const u32 f : order)
    {
        if (!TryPlace(attempt, item.mFrames[f].mWidth, item.mFrames[f].mHeight, positions[f].mX, positions[f].mY))
        {
            return false;
        }
    }

    page = std::move(attempt);
    placement.mFrames = std::move(positions);
    return true;
}

static u64 ItemArea(const SpriteAtlasPacker::Item& item)
{
    u64 area = 0;
    for (const auto& frame : item.mFrames)
    {
        if (frame.mWidth != 0 && frame.mHeight != 0)
        {
            area += static_cast<u64>(frame.mWidth + SpriteAtlasPacker::kGap) * (frame.mHeight + SpriteAtlasPacker::kGap);
        }
    }
    return area;
}

SpriteAtlasPacker::Result SpriteAtlasPacker::Pack(const std::vector<Item>& items, u32 pageSize)
{
    Result result;
    result.mPlacements.resize(items.size());

    // The items of each group, the biggest first so the page fills up evenly. Groups go in the
    // order of their ids, so the result doesn't depend on the order the items are in.
    std::map<u32, std::vector<u32>> groups;
    for (u32 i = 0; i < items.size(); i++)
    {
        // An item that can't be on a page, even an empty one, is left alone
        PackerPage empty(pageSize);
        Placement unused;
        if (ItemArea(items[i]) == 0 || !TryPlaceItem(empty, items[i], unused))
        {
            continue;
        }
        groups[items[i].mGroup].push_back(i);
    }

    std::vector<PackerPage> pages;
    constexpr u32 kNone = 0xFFFFFFFFu;
    // The page the next group tries first, while there is room: the one the last group ended on
    u32 sharedPage = kNone;

    for (auto& [group, indices] : groups)
    {
        std::stable_sort(indices.begin(), indices.end(), [&](u32 a, u32 b)
            { return ItemArea(items[a]) > ItemArea(items[b]); });

        u64 area = 0;
        for (const u32 index : indices)
        {
            area += ItemArea(items[index]);
        }

        // A group that fits on one page is tried on the shared page first, and if it doesn't
        // fit in what is left of it, on a new page
        const bool fitsOnePage = area <= static_cast<u64>(pageSize) * pageSize;
        if (fitsOnePage && sharedPage != kNone)
        {
            PackerPage attempt = pages[sharedPage];
            std::vector<Placement> placements(indices.size());
            bool ok = true;
            for (std::size_t k = 0; k < indices.size() && ok; k++)
            {
                ok = TryPlaceItem(attempt, items[indices[k]], placements[k]);
            }

            if (ok)
            {
                pages[sharedPage] = std::move(attempt);
                for (std::size_t k = 0; k < indices.size(); k++)
                {
                    placements[k].mPage = sharedPage;
                    result.mPlacements[indices[k]] = std::move(placements[k]);
                }
                continue;
            }
        }

        // The group's pages: it starts on what is left of the shared page (that room would be
        // wasted otherwise) and takes new pages as it needs them. Each item goes on the first of
        // its pages that it fits on.
        std::vector<u32> groupPages;
        if (!fitsOnePage && sharedPage != kNone)
        {
            groupPages.push_back(sharedPage);
        }

        for (const u32 index : indices)
        {
            Placement placement;
            bool placed = false;
            for (const u32 page : groupPages)
            {
                if (TryPlaceItem(pages[page], items[index], placement))
                {
                    placement.mPage = page;
                    placed = true;
                    break;
                }
            }

            if (!placed)
            {
                pages.emplace_back(pageSize);
                groupPages.push_back(static_cast<u32>(pages.size() - 1));
                TryPlaceItem(pages.back(), items[index], placement);
                placement.mPage = groupPages.back();
            }
            result.mPlacements[index] = std::move(placement);
        }

        // The page the group ended on is the one the next group tries
        sharedPage = groupPages.back();
    }

    result.mPageCount = static_cast<u32>(pages.size());
    return result;
}

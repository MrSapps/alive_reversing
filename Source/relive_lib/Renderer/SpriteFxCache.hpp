#pragma once

#include "../../relive_lib/Types.hpp"
#include "../../relive_lib/SpriteFx.hpp"
#include "../../relive_lib/FatalError.hpp"
#include <map>

// Which slot of the renderer's table of sprite effects each effect is drawn with, so the sprites
// that are drawn with the same one share it and it is only uploaded when it is new
class SpriteFxCache final
{
public:
    explicit SpriteFxCache(u32 maxCacheSize = 256)
        : mMaxCacheSize(maxCacheSize)
    {
    }

    struct AddResult final
    {
        u32 mIndex = 0;
        bool mAllocated = false;
    };

    AddResult Add(const SpriteFx& fx)
    {
        AddResult ret;

        // Check we don't already have this effect
        const u32 hash = fx.Hash();
        auto searchResult = mCache.find(hash);

        if (searchResult != std::end(mCache) && searchResult->second.mFx == fx)
        {
            searchResult->second.mInUse = true;
            ret.mIndex = searchResult->second.mIndex;
            return ret;
        }

        if (searchResult != std::end(mCache))
        {
            // A different effect with the same hash: it takes over the slot
            searchResult->second.mFx = fx;
            searchResult->second.mInUse = true;
            ret.mIndex = searchResult->second.mIndex;
            ret.mAllocated = true;
            return ret;
        }

        // Get an index for the new effect
        u32 nextIndex = static_cast<u32>(mCache.size());

        if (nextIndex >= mMaxCacheSize)
        {
            // Look for a unused slot
            auto iter = mCache.begin();
            while (iter != mCache.end())
            {
                if (!iter->second.mInUse)
                {
                    break;
                }
                iter++;
            }

            if (iter == std::end(mCache))
            {
                ALIVE_FATAL("Ran out of sprite effects!");
            }

            // Acquire the index we're taking over
            nextIndex = iter->second.mIndex;

            // Bin the old one
            mCache.erase(iter);
        }

        mCache[hash] = Entry{nextIndex, true, fx};

        ret.mAllocated = true;
        ret.mIndex = nextIndex;

        return ret;
    }

    void ResetUseFlags()
    {
        for (auto iter = mCache.begin(); iter != mCache.end(); iter++)
        {
            // Default all effects to unused for next draw
            iter->second.mInUse = false;
        }
    }

    void Clear()
    {
        mCache.clear();
    }

private:
    struct Entry final
    {
        u32 mIndex = 0;
        bool mInUse = false;
        SpriteFx mFx;
    };
    std::map<u32, Entry> mCache;
    u32 mMaxCacheSize = 0;
};

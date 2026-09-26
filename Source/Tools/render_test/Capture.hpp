#pragma once

#include "../../relive_lib/Types.hpp"
#include <string>
#include <vector>

class FileSystem;

// A frame read back from the renderer: RGBA32 rows, top row first
struct Capture final
{
    s32 mWidth = 0;
    s32 mHeight = 0;
    std::vector<u8> mPixels;

    bool IsEmpty() const
    {
        return mPixels.empty();
    }

    bool IsAllBlack() const;

    bool SavePng(FileSystem& fs, const std::string& path) const;
    static Capture LoadPng(FileSystem& fs, const std::string& path);
};

struct CaptureDiff final
{
    bool mSizesDiffer = false;
    // Pixels where a colour channel differs by more than the tolerance
    u32 mDifferingPixels = 0;
    u32 mTotalPixels = 0;
    // The biggest difference in any channel
    u8 mMaxDifference = 0;

    bool Identical() const
    {
        return !mSizesDiffer && mDifferingPixels == 0;
    }

    f64 DifferingPercent() const
    {
        return mTotalPixels ? 100.0 * mDifferingPixels / mTotalPixels : 100.0;
    }

    static CaptureDiff Compare(const Capture& a, const Capture& b, u8 tolerance = 0);

    // a dimmed to grey, with the pixels that differ from b in bright red
    static Capture MakeImage(const Capture& a, const Capture& b, u8 tolerance = 0);
};

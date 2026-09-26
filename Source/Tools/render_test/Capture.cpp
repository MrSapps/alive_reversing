#include "stdafx.h"
#include "Capture.hpp"
#include "../../relive_lib/data_conversion/file_system.hpp"
#include "../../relive_lib/data_conversion/PNGFile.hpp"
#include <algorithm>
#include <cstdlib>

bool Capture::IsAllBlack() const
{
    for (std::size_t i = 0; i < mPixels.size(); i += 4)
    {
        if (mPixels[i] || mPixels[i + 1] || mPixels[i + 2])
        {
            return false;
        }
    }
    return true;
}

bool Capture::SavePng(FileSystem& fs, const std::string& path) const
{
    if (IsEmpty())
    {
        return false;
    }

    PNGFile png;
    const std::vector<u8> encoded = png.Encode(reinterpret_cast<const u32*>(mPixels.data()), mWidth, mHeight);
    return fs.Save(path.c_str(), encoded);
}

Capture Capture::LoadPng(FileSystem& fs, const std::string& path)
{
    Capture capture;
    std::vector<u8> encoded;
    if (!fs.FileExists(path.c_str()) || !fs.LoadToVec(path.c_str(), encoded) || encoded.empty())
    {
        return capture;
    }

    u32 width = 0;
    u32 height = 0;
    PNGFile png;
    png.Decode(path, encoded, capture.mPixels, width, height);
    capture.mWidth = static_cast<s32>(width);
    capture.mHeight = static_cast<s32>(height);
    return capture;
}

static u8 ChannelDifference(u8 a, u8 b)
{
    return static_cast<u8>(std::abs(static_cast<s32>(a) - static_cast<s32>(b)));
}

CaptureDiff CaptureDiff::Compare(const Capture& a, const Capture& b, u8 tolerance)
{
    CaptureDiff diff;
    if (a.mWidth != b.mWidth || a.mHeight != b.mHeight || a.mPixels.size() != b.mPixels.size())
    {
        diff.mSizesDiffer = true;
        return diff;
    }

    diff.mTotalPixels = static_cast<u32>(a.mPixels.size() / 4);
    for (std::size_t i = 0; i < a.mPixels.size(); i += 4)
    {
        const u8 d = std::max({ChannelDifference(a.mPixels[i], b.mPixels[i]),
                               ChannelDifference(a.mPixels[i + 1], b.mPixels[i + 1]),
                               ChannelDifference(a.mPixels[i + 2], b.mPixels[i + 2])});
        diff.mMaxDifference = std::max(diff.mMaxDifference, d);
        if (d > tolerance)
        {
            diff.mDifferingPixels++;
        }
    }
    return diff;
}

Capture CaptureDiff::MakeImage(const Capture& a, const Capture& b, u8 tolerance)
{
    Capture image = a;
    const bool sameSize = a.mPixels.size() == b.mPixels.size();
    for (std::size_t i = 0; i < image.mPixels.size(); i += 4)
    {
        const u8 d = sameSize ? std::max({ChannelDifference(a.mPixels[i], b.mPixels[i]),
                                          ChannelDifference(a.mPixels[i + 1], b.mPixels[i + 1]),
                                          ChannelDifference(a.mPixels[i + 2], b.mPixels[i + 2])})
                              : 255;
        if (d > tolerance)
        {
            image.mPixels[i] = 255;
            image.mPixels[i + 1] = 0;
            image.mPixels[i + 2] = 0;
        }
        else
        {
            const u8 grey = static_cast<u8>((a.mPixels[i] + a.mPixels[i + 1] + a.mPixels[i + 2]) / 6);
            image.mPixels[i] = grey;
            image.mPixels[i + 1] = grey;
            image.mPixels[i + 2] = grey;
        }
        image.mPixels[i + 3] = 255;
    }
    return image;
}

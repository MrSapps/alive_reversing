#include "stdafx.h"
#include "EtaEstimator.hpp"
#include <cstdio>

namespace
{
    constexpr u32 kWindowMs = 8000;
}

std::string EtaEstimator::FormatDuration(u32 ms)
{
    u32 totalSeconds = ms / 1000;
    const u32 hours = totalSeconds / 3600;
    totalSeconds %= 3600;
    const u32 minutes = totalSeconds / 60;
    const u32 seconds = totalSeconds % 60;

    char buf[32];
    if (hours > 0)
    {
        snprintf(buf, sizeof(buf), "%uh%02um", hours, minutes);
    }
    else if (minutes > 0)
    {
        snprintf(buf, sizeof(buf), "%um%02us", minutes, seconds);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%us", seconds);
    }
    return buf;
}

void EtaEstimator::Update(u32 nowTicks, u64 totalCompleted, u64 totalItems)
{
    mSamples.emplace_back(nowTicks, totalCompleted);
    while (mSamples.size() > 1 && (nowTicks - mSamples.front().first) > kWindowMs)
    {
        mSamples.pop_front();
    }

    if (totalItems > 0 && totalCompleted >= totalItems)
    {
        mEtaStr = "almost done";
        return;
    }

    if (mSamples.size() < 2)
    {
        mEtaStr = "calculating...";
        return;
    }

    const auto& oldest = mSamples.front();
    const auto& newest = mSamples.back();
    const u32 windowMs = newest.first - oldest.first;
    const u64 windowCompleted = newest.second - oldest.second;
    if (windowMs == 0 || windowCompleted == 0)
    {
        mEtaStr = "calculating...";
        return;
    }

    const u64 remaining = totalItems - totalCompleted;
    const double msPerItem = static_cast<double>(windowMs) / static_cast<double>(windowCompleted);
    mEtaStr = FormatDuration(static_cast<u32>(msPerItem * static_cast<double>(remaining)));
}

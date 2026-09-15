#include "stdafx.h"
#include "ConversionProgress.hpp"
#include <algorithm>

float ConversionProgress::CategoryWeight(ConversionCategory cat)
{
    switch (cat)
    {
        case ConversionCategory::Paths:
            return 0.10f;
        case ConversionCategory::Animations:
            return 0.20f;
        case ConversionCategory::Cameras:
            return 0.15f;
        case ConversionCategory::Misc:
            return 0.05f;
        case ConversionCategory::Fmvs:
            return 0.50f;
        default:
            return 0.0f;
    }
}

void ConversionProgress::AddToTotal(ConversionCategory cat, u32 amount)
{
    mCategories[static_cast<size_t>(cat)].mTotal += amount;
}

void ConversionProgress::AddCompleted(ConversionCategory cat, u32 amount)
{
    mCategories[static_cast<size_t>(cat)].mCompleted += amount;
}

void ConversionProgress::ReportItemStarted(std::string itemName, u32 total)
{
    std::unique_lock lock(mLogMutex);
    mInProgressItems.push_back(InProgressItem{std::move(itemName), 0, total});
}

void ConversionProgress::UpdateItemProgress(const std::string& itemName, u32 current)
{
    std::unique_lock lock(mLogMutex);
    for (InProgressItem& item : mInProgressItems)
    {
        if (item.mName == itemName)
        {
            item.mCurrent = current;
            return;
        }
    }
}

void ConversionProgress::ReportItemFinished(const std::string& itemName)
{
    std::unique_lock lock(mLogMutex);

    const auto it = std::find_if(mInProgressItems.begin(), mInProgressItems.end(),
                                  [&itemName](const InProgressItem& item) { return item.mName == itemName; });
    if (it != mInProgressItems.end())
    {
        mInProgressItems.erase(it);
    }

    mRecentItems.push_front(itemName);
    constexpr size_t kMaxRecentItemsStored = 32;
    while (mRecentItems.size() > kMaxRecentItemsStored)
    {
        mRecentItems.pop_back();
    }
}

float ConversionProgress::OverallPercent() const
{
    float weightedSum = 0.0f;
    float weightWithWork = 0.0f;

    for (size_t i = 0; i < static_cast<size_t>(ConversionCategory::Count); i++)
    {
        const u32 total = mCategories[i].mTotal;
        if (total == 0)
        {
            continue;
        }

        const u32 completed = mCategories[i].mCompleted;
        const float pct = static_cast<float>(completed) / static_cast<float>(total);
        const float weight = CategoryWeight(static_cast<ConversionCategory>(i));

        weightedSum += pct * weight;
        weightWithWork += weight;
    }

    if (weightWithWork <= 0.0f)
    {
        // Nothing scanned yet, not "nothing to do" - DataConversionUI only ever exists when
        // ConversionRequired() was true, so by the time this is observable in practice, some
        // category's total will shortly become non-zero (see ConvertDataAE/AO's dry-run pass).
        // Reporting 0% here (rather than 100%) avoids a false "done" flash on screen/in logs
        // before that dry run has had a chance to run.
        return 0.0f;
    }

    return weightedSum / weightWithWork;
}

ConversionProgress::Snapshot ConversionProgress::GetSnapshot(size_t maxRecentItems) const
{
    Snapshot snapshot;
    snapshot.mOverallPercent = OverallPercent();

    for (size_t i = 0; i < static_cast<size_t>(ConversionCategory::Count); i++)
    {
        snapshot.mTotalItems += mCategories[i].mTotal;
        snapshot.mTotalCompleted += mCategories[i].mCompleted;
    }

    std::unique_lock lock(mLogMutex);
    snapshot.mInProgressItems.assign(mInProgressItems.begin(), mInProgressItems.end());

    const size_t count = std::min(maxRecentItems, mRecentItems.size());
    snapshot.mRecentItems.assign(mRecentItems.begin(), mRecentItems.begin() + count);

    return snapshot;
}

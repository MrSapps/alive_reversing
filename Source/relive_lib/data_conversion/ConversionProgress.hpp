#pragma once

#include "../Types.hpp"

#include <array>
#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

// Tracks weighted overall progress across the different data-conversion categories
// (paths/animations/cameras/misc/fmvs), plus a small log of recently-finished and
// currently-in-progress item names, so DataConversionUI can render a meaningful
// percentage/progress bar and activity list instead of a flat thread-pool job count
// (which only ever reflected cameras+fmvs, since everything else converts
// synchronously and never touches ThreadPool::AddJob).
//
// Threaded through the conversion call chain by reference (owned by DataConversion),
// same as ThreadPool& already is - no globals/statics.
enum class ConversionCategory : u8
{
    Paths,
    Animations,
    Cameras,
    Misc,
    Fmvs,
    Count
};

// An item currently being converted. mTotal is 0 for items with no meaningful sub-progress of
// their own (paths/animations/misc/cameras - each is just "in progress" or "done", nothing in
// between worth showing) - only fmvs currently set it, to the movie's own frame count, since a
// single fmv can take long enough on its own that "in progress" alone looks stalled.
struct InProgressItem final
{
    std::string mName;
    u32 mCurrent = 0;
    u32 mTotal = 0;
};

class ConversionProgress final
{
public:
    // Weights sum to 1.0: paths 10%, animations 20%, cameras 15%, misc (palettes/
    // saves/fonts/demos) 5%, fmvs 50% - fmvs dominate since they take by far the
    // longest to convert.
    static float CategoryWeight(ConversionCategory cat);

    void AddToTotal(ConversionCategory cat, u32 amount);

    // Cheap atomic bump, no locking - safe to call at high frequency (e.g. once per
    // encoded FMV video frame).
    void AddCompleted(ConversionCategory cat, u32 amount);

    // Adds itemName to the "in progress" list. Only worth calling for genuinely
    // long-running items (cameras, fmvs) - fast synchronous items (paths/anims/misc)
    // can skip straight to ReportItemFinished. total, when given, is the item's own
    // sub-progress ceiling (currently just an fmv's frame count).
    void ReportItemStarted(std::string itemName, u32 total = 0);

    // Updates itemName's current sub-progress (e.g. the frame just encoded) - a no-op if it was
    // never started with a nonzero total, or has already finished. Safe to call at high frequency
    // (once per encoded fmv frame), same as AddCompleted, though it does briefly lock mLogMutex
    // (unlike AddCompleted) since it has to find and update the matching in-progress entry.
    void UpdateItemProgress(const std::string& itemName, u32 current);

    // Removes itemName from "in progress" (harmless no-op if it was never started)
    // and pushes it onto the bounded, most-recent-first "recent items" log.
    void ReportItemFinished(const std::string& itemName);

    // Weighted average of per-category (completed/total) across categories whose
    // total > 0, renormalized over just those categories' weights - so a run that
    // doesn't need e.g. any fmv conversion this launch still reaches 100%, rather
    // than capping at (1 - fmv weight) forever. Returns 0 (not 1) if no category has
    // any total yet - a genuine "nothing to convert" run never reaches this class at
    // all (DataConversionUI only exists when ConversionRequired() was true), so this
    // state always means "the dry-run scan just hasn't populated any totals yet".
    [[nodiscard]] float OverallPercent() const;

    struct Snapshot final
    {
        float mOverallPercent = 0.0f;
        // Sum of every category's total/completed (paths+animations+cameras+misc+fmvs, the last
        // counted in frames) - a raw, mixed-unit "how much work overall" count. Mainly useful
        // for fmv-dominated runs where the weighted percentage alone barely seems to move.
        u64 mTotalCompleted = 0;
        u64 mTotalItems = 0;
        std::vector<std::string> mRecentItems;
        std::vector<InProgressItem> mInProgressItems;
    };
    [[nodiscard]] Snapshot GetSnapshot(size_t maxRecentItems = 8) const;

private:
    struct CategoryCounters final
    {
        std::atomic<u32> mTotal{0};
        std::atomic<u32> mCompleted{0};
    };
    std::array<CategoryCounters, static_cast<size_t>(ConversionCategory::Count)> mCategories;

    // Guards mInProgressItems/mRecentItems only - the hot path (AddCompleted) never
    // touches this mutex.
    mutable std::mutex mLogMutex;
    std::vector<InProgressItem> mInProgressItems;
    std::deque<std::string> mRecentItems;
};

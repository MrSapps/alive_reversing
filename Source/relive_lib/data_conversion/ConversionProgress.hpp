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
    // can skip straight to ReportItemFinished.
    void ReportItemStarted(std::string itemName);

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
        std::vector<std::string> mRecentItems;
        std::vector<std::string> mInProgressItems;
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
    std::vector<std::string> mInProgressItems;
    std::deque<std::string> mRecentItems;
};

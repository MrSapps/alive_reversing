// Unit tests for ConversionProgress (data_conversion/ConversionProgress.hpp) - specifically the
// weighted-percentage renormalization, which is the one bit of real logic in that class (the
// rest is atomic counters / a mutex-guarded log). Categories with total==0 (nothing to do this
// run) must be excluded from the denominator, or a run that e.g. only needs fmv conversion would
// cap at 50% forever instead of reaching 100%.

#include "../data_conversion/ConversionProgress.hpp"

#include <gtest/gtest.h>

TEST(ConversionProgress, FreshInstanceReportsZeroProgressNotFullProgress)
{
    // No category has any total yet - this means the dry-run scan just hasn't run, not that
    // there's nothing to do (DataConversionUI only exists when ConversionRequired() was true, so
    // a real "nothing to convert" run never reaches this class at all). Reporting 0% (not 100%)
    // avoids a false "done" flash before the scan populates real totals - caught via a live test
    // where the logged percentage briefly went 100% -> 9%.
    ConversionProgress progress;
    EXPECT_FLOAT_EQ(progress.OverallPercent(), 0.0f);
}

TEST(ConversionProgress, SingleCategoryPartialCompletion)
{
    ConversionProgress progress;
    progress.AddToTotal(ConversionCategory::Paths, 10);
    progress.AddCompleted(ConversionCategory::Paths, 4);

    // Only Paths has work, so the renormalized overall percent equals that category's own
    // fraction, independent of its absolute weight.
    EXPECT_FLOAT_EQ(progress.OverallPercent(), 0.4f);
}

TEST(ConversionProgress, MultipleCategoriesWeightedCorrectly)
{
    ConversionProgress progress;

    progress.AddToTotal(ConversionCategory::Paths, 10); // weight .10
    progress.AddCompleted(ConversionCategory::Paths, 10); // 100%

    progress.AddToTotal(ConversionCategory::Fmvs, 100); // weight .50
    progress.AddCompleted(ConversionCategory::Fmvs, 50); // 50%

    // Only Paths+Fmvs have work: weights .10 and .50, sum .60.
    // weighted = 1.0*.10 + 0.5*.50 = 0.35, renormalized over .60 -> 0.5833...
    const float expected = (1.0f * 0.10f + 0.5f * 0.50f) / (0.10f + 0.50f);
    EXPECT_NEAR(progress.OverallPercent(), expected, 0.0001f);
}

TEST(ConversionProgress, CategoryWithNoWorkThisRunDoesNotDragDownPercent)
{
    ConversionProgress progress;

    // Only Fmvs (weight .50) has any work this run - every other category stays at 0/0
    // (dv.ConvertX() was false for them). Once Fmvs finishes, overall should reach 100%, not
    // cap at 50%.
    progress.AddToTotal(ConversionCategory::Fmvs, 20);
    progress.AddCompleted(ConversionCategory::Fmvs, 20);

    EXPECT_FLOAT_EQ(progress.OverallPercent(), 1.0f);
}

TEST(ConversionProgress, ReportItemFinishedLogsToRecentItemsEvenWithoutStarted)
{
    ConversionProgress progress;

    // Fast synchronous items (paths/animations/misc) call ReportItemFinished directly without
    // ever calling ReportItemStarted - this must still log, and the removal from the
    // in-progress list must be a harmless no-op.
    progress.ReportItemFinished("some_path.json");

    const ConversionProgress::Snapshot snapshot = progress.GetSnapshot();
    ASSERT_EQ(snapshot.mRecentItems.size(), 1u);
    EXPECT_EQ(snapshot.mRecentItems[0], "some_path.json");
    EXPECT_TRUE(snapshot.mInProgressItems.empty());
}

TEST(ConversionProgress, StartedItemAppearsInProgressUntilFinished)
{
    ConversionProgress progress;

    progress.ReportItemStarted("movie_a.webm");
    progress.ReportItemStarted("movie_b.webm");

    {
        const ConversionProgress::Snapshot snapshot = progress.GetSnapshot();
        EXPECT_EQ(snapshot.mInProgressItems.size(), 2u);
        EXPECT_TRUE(snapshot.mRecentItems.empty());
    }

    progress.ReportItemFinished("movie_a.webm");

    {
        const ConversionProgress::Snapshot snapshot = progress.GetSnapshot();
        ASSERT_EQ(snapshot.mInProgressItems.size(), 1u);
        EXPECT_EQ(snapshot.mInProgressItems[0], "movie_b.webm");
        ASSERT_EQ(snapshot.mRecentItems.size(), 1u);
        EXPECT_EQ(snapshot.mRecentItems[0], "movie_a.webm");
    }
}

TEST(ConversionProgress, RecentItemsAreMostRecentFirstAndBoundedBySnapshotLimit)
{
    ConversionProgress progress;

    progress.ReportItemFinished("first");
    progress.ReportItemFinished("second");
    progress.ReportItemFinished("third");

    const ConversionProgress::Snapshot snapshot = progress.GetSnapshot(2);
    ASSERT_EQ(snapshot.mRecentItems.size(), 2u);
    EXPECT_EQ(snapshot.mRecentItems[0], "third");
    EXPECT_EQ(snapshot.mRecentItems[1], "second");
}

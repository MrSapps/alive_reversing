// Unit tests for EtaEstimator (data_conversion/EtaEstimator.hpp) - specifically its recent-rate
// sliding window, which exists because an average-since-the-whole-run-started estimate stays
// dragged down by an early fast phase (paths/animations/cameras/misc, typically done in seconds)
// for a long time into a slow one (fmvs, can take many minutes), making the ETA visibly climb
// instead of settle - confirmed live: ETA climbed from ~1min to 6min during fmv conversion.

#include "../data_conversion/EtaEstimator.hpp"

#include <gtest/gtest.h>

TEST(EtaEstimator, ReportsCalculatingWithFewerThanTwoSamples)
{
    EtaEstimator eta;
    eta.Update(0, 0, 1000);
    EXPECT_EQ(eta.EtaString(), "calculating...");
}

TEST(EtaEstimator, ReportsAlmostDoneWhenCompletedReachesTotal)
{
    EtaEstimator eta;
    eta.Update(1000, 100, 100);
    EXPECT_EQ(eta.EtaString(), "almost done");
}

TEST(EtaEstimator, EstimatesFromRecentRate)
{
    EtaEstimator eta;
    eta.Update(0, 0, 1000);
    // 100 items completed over 1000ms -> 10ms/item; 900 remaining -> 9000ms.
    eta.Update(1000, 100, 1000);
    EXPECT_EQ(eta.EtaString(), "9s");
}

TEST(EtaEstimator, RecentWindowIgnoresStaleFastBurstFromEarlierPhase)
{
    EtaEstimator eta;

    // A fast burst at the very start - 9000 items complete almost instantly (e.g. paths/
    // animations finishing before any fmv work has even begun).
    eta.Update(0, 0, 10000);
    eta.Update(1, 9000, 10000);

    // A single slow-phase sample far enough past the burst (8001ms later, past the 8000ms
    // window) that both burst samples age out of the window - too few samples left (1) to
    // compute a rate yet, so this must NOT reuse the burst's now-evicted fast rate.
    eta.Update(8002, 9001, 10000);
    EXPECT_EQ(eta.EtaString(), "calculating...");

    // A second slow-phase sample, 1000ms and 1 item after the last - now there are exactly 2
    // in-window samples, both from the slow phase, giving a clean 1000ms/item rate.
    eta.Update(9002, 9002, 10000);
    // remaining = 10000 - 9002 = 998 items * 1000ms/item = 998000ms = 16m38s.
    EXPECT_EQ(eta.EtaString(), "16m38s");
}

TEST(EtaEstimator, FormatsHoursMinutesAndSeconds)
{
    EtaEstimator eta;
    eta.Update(0, 0, 3661);
    // 1 item per 1000ms; 3661 remaining -> 3661000ms = 1h01m01s -> formatted "1h01m" (seconds
    // dropped once hours are shown, matching FormatDuration's precision falloff).
    eta.Update(1000, 1, 3661);
    EXPECT_EQ(eta.EtaString(), "1h01m");
}

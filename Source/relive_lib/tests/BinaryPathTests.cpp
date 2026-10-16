// Unit tests for BinaryPath's TLV spawn flags, which restart path relies on to bring back objects
// that removed themselves (an opened slam door) without reloading the path.

#include "BinaryPath.hpp"
#include "data_conversion/relive_tlvs.hpp"
#include <gtest/gtest.h>

static relive::Path_TLV* AddSlamDoor(BinaryPath::CamEntry& cam, bool lastInCamera)
{
    auto pTlv = std::make_unique<relive::Path_SlamDoor>();
    if (lastInCamera)
    {
        pTlv->mTlvFlags.Set(relive::TlvFlags::eBit3_End_TLV_List);
    }
    relive::Path_TLV* raw = pTlv.get();
    cam.AddTLV(std::move(pTlv));
    return raw;
}

static void MarkSpawnedAndGone(relive::Path_TLV* pTlv)
{
    // What the path loader does when it constructs an object
    pTlv->mTlvFlags.Set(relive::TlvFlags::eBit1_Created);
    pTlv->mTlvFlags.Set(relive::TlvFlags::eBit2_Destroyed);
}

TEST(BinaryPath, ClearSpawnedFlagsMakesEveryTlvSpawnableAgain)
{
    BinaryPath path("path.json", 1);
    path.GetCameras().push_back(std::make_unique<BinaryPath::CamEntry>());
    path.GetCameras().push_back(std::make_unique<BinaryPath::CamEntry>());

    relive::Path_TLV* a = AddSlamDoor(*path.GetCameras()[0], false);
    relive::Path_TLV* b = AddSlamDoor(*path.GetCameras()[0], true);
    relive::Path_TLV* c = AddSlamDoor(*path.GetCameras()[1], true);
    for (auto* pTlv : {a, b, c})
    {
        MarkSpawnedAndGone(pTlv);
    }

    path.ClearSpawnedFlags();

    for (auto* pTlv : {a, b, c})
    {
        EXPECT_FALSE(pTlv->mTlvFlags.Get(relive::TlvFlags::eBit1_Created));
        EXPECT_FALSE(pTlv->mTlvFlags.Get(relive::TlvFlags::eBit2_Destroyed));
    }
}

TEST(BinaryPath, ClearSpawnedFlagsKeepsTheEndOfListMarker)
{
    BinaryPath path("path.json", 1);
    path.GetCameras().push_back(std::make_unique<BinaryPath::CamEntry>());
    relive::Path_TLV* first = AddSlamDoor(*path.GetCameras()[0], false);
    relive::Path_TLV* last = AddSlamDoor(*path.GetCameras()[0], true);
    MarkSpawnedAndGone(first);
    MarkSpawnedAndGone(last);

    path.ClearSpawnedFlags();

    EXPECT_FALSE(first->mTlvFlags.Get(relive::TlvFlags::eBit3_End_TLV_List));
    EXPECT_TRUE(last->mTlvFlags.Get(relive::TlvFlags::eBit3_End_TLV_List));
}

TEST(BinaryPath, TlvsOfAnUntouchedCameraAreLeftAlone)
{
    BinaryPath path("path.json", 1);
    path.GetCameras().push_back(std::make_unique<BinaryPath::CamEntry>());
    relive::Path_TLV* pTlv = AddSlamDoor(*path.GetCameras()[0], true);
    pTlv->mTlvSpecificMeaning = 7;

    path.ClearSpawnedFlags();

    EXPECT_EQ(pTlv->mTlvSpecificMeaning, 7);
}

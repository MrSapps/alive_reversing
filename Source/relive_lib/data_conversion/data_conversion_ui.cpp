#include "data_conversion_ui.hpp"
#include "Primitives.hpp"
#include "data_conversion.hpp"
#include <functional>
#include <chrono>
#include <algorithm>
#include "../../AliveLibAE/Resources.hpp"
#include "../../AliveLibAE/Map.hpp"
#include "../../AliveLibAE/PsxRender.hpp"
#include "GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "AnimationConverter.hpp"

DataConversionUI::DataConversionUI(GameType gameType, ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseGameObject(FALSE, 0, resMan, map)
    , mGameType(gameType)
    , mDataConversion(std::make_unique<DataConversion>())
{
    mPoly.SetXYWH(0, 0, 640, 240);
    mPoly.SetRGB0(255, 0, 0);
    mPoly.SetRGB1(0, 255, 255);
    mPoly.SetRGB2(0, 0, 255);
    mPoly.SetRGB3(255, 0, 255);

    // Progress bar border (fixed, a couple px larger than the track on every side so it reads as
    // an outline behind it) + track (fixed) + fill (width recomputed every VUpdate() from the
    // weighted overall percentage - see ConversionProgress::OverallPercent).
    mProgressBarBorder.SetXYWH(18, 203, 604, 12);
    mProgressBarBorder.SetRGB0(200, 200, 200);
    mProgressBarBorder.SetRGB1(200, 200, 200);
    mProgressBarBorder.SetRGB2(200, 200, 200);
    mProgressBarBorder.SetRGB3(200, 200, 200);

    mProgressBarTrack.SetXYWH(20, 205, 600, 8);
    mProgressBarTrack.SetRGB0(40, 40, 40);
    mProgressBarTrack.SetRGB1(40, 40, 40);
    mProgressBarTrack.SetRGB2(40, 40, 40);
    mProgressBarTrack.SetRGB3(40, 40, 40);

    mProgressBarFill.SetXYWH(20, 205, 0, 8);
    mProgressBarFill.SetRGB0(80, 200, 120);
    mProgressBarFill.SetRGB1(80, 200, 120);
    mProgressBarFill.SetRGB2(80, 200, 120);
    mProgressBarFill.SetRGB3(80, 200, 120);

    mFontContext.LoadFontType(FontType::Debug, mResMan);

    PalResource palRes;
    palRes.mPal = mFontContext.mFntResource.mCurPal;
    // 2048 (was 512): VRender now draws the status message plus up to 12 activity-list lines
    // every frame (mFntPolyArray is a single shared, fixed-size array all of that frame's
    // DrawString calls write into via an accumulating offset - see VRender) - 512 was already
    // tight for just the status line at longer path/animation names, let alone 12 more lines.
    mFont.Load(2048,  palRes, &mFontContext);
    

    /*
    BaseAnimatedWithPhysicsGameObject::MakeArray();
    gBaseGameObjects = relive_new DynamicArrayT<BaseGameObject>(90);
    BaseAnimatedWithPhysicsGameObject::MakeArray(); // Makes drawables
    AnimationBase::CreateAnimationArray();
    CamResource nullCamRes;
    gScreenManager = relive_new ScreenManager(nullCamRes, &mMap.mCameraOffset);

    mLcdScreenParams.mTopLeftX = 100;
    mLcdScreenParams.mBottomRightX = 300;
    mLcdScreenParams.mBottomRightY = 40;
    mLcdScreenParams.mMessageId1 = 3;
    mLcdScreenParams.mMessageId2 = 4;
    Guid g;
    mLcd = std::make_unique<LCDScreen>(&mLcdScreenParams, g);

    mLcdStatusBoardParams.mTopLeftX = 50;
    mLcdStatusBoardParams.mTopLeftY = 110;
    mLcdStatusBoardParams.mNumberOfMuds = 123;
    mLcdStatusBoardParams.mHideBoard = false;
    mLcdStatusBoard = std::make_unique<LCDStatusBoard>(&mLcdStatusBoardParams, g);
    */
}

DataConversionUI::~DataConversionUI()
{
    TRACE_ENTRYEXIT;
    if (mThread && mThread->joinable())
    {
        mThread->join();
    }
}

void DataConversionUI::RequestCancel()
{
    mDataConversion->RequestCancel();
}

bool DataConversionUI::AsyncTasksInProgress() const
{
    return mDataConversion->AsyncTasksInProgress();
}

void DataConversionUI::ThreadFunc()
{
    TRACE_ENTRYEXIT;

    DataConversion::DataVersions zeroVersions;

#if 0
    // Dev hack to control which data files to convert (can force reconvert/force skip)
    zeroVersions = DataConversion::DataVersions::LatestVersion();
    zeroVersions.mFmvVersion = 0;
#endif

    if (mGameType == GameType::eAe)
    {
        mDataConversion->ConvertDataAE(mDataConversion->DataVersionAE().value_or(zeroVersions));
    }
    else
    {
        mDataConversion->ConvertDataAO(mDataConversion->DataVersionAO().value_or(zeroVersions));
    }

    // ConvertDataAE/AO only *dispatch* work onto the thread pool (FMVs, paths, ...), they don't
    // wait for any of it - don't exit (or, below, declare data_version.json up to date) till it's
    // actually finished.
    while (mDataConversion->AsyncTasksInProgress())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Only stamp data_version.json with the versions we were actually converting towards once
    // nothing got cancelled partway through (see ThreadPool::RequestCancel/Engine::Run()'s quit
    // handling) - otherwise this would claim the whole conversion completed when some of it
    // didn't, and a later launch's DataVersions::ConvertFmvs()/ConvertPaths()/etc checks would
    // wrongly skip reconverting the parts that never actually finished. FMV conversion tracks its
    // own finer-grained per-movie progress regardless (see FmvConversionManifest in
    // fmv_converter.cpp), so it alone can still resume efficiently next launch even though this
    // stays unwritten - other categories don't have that yet, so they just fully redo their work
    // next launch, same as a version bump would make them do anyway.
    if (!mDataConversion->IsCancelRequested())
    {
        FileSystem::Path dataDir;
        dataDir.Append("relive_data");
        dataDir.Append(mGameType == GameType::eAe ? "ae" : "ao");
        DataConversion::DataVersions::LatestVersion().Save(dataDir);
    }

    mDone = true;
}

void DataConversionUI::VUpdate()
{
    if (!mThread)
    {
        mThread = std::make_unique<std::thread>(std::bind(&DataConversionUI::ThreadFunc, this));
    }

    if (mDone)
    {
        SetDead(true);
    }

    mPoly.SetRGB0(0, 0, 0);
    mPoly.SetRGB1(0, 0, 0);
    mPoly.SetRGB2(0, 0, 0);
    mPoly.SetRGB3(0, 0, 0);

    //mLcdStatusBoard->VUpdate();
    //mLcd->VUpdate();

    mLastSnapshot = mDataConversion->ProgressSnapshot();
    const s32 overallPercent = static_cast<s32>(mLastSnapshot.mOverallPercent * 100.0f + 0.5f);

    const s16 trackWidth = 600;
    const s16 fillWidth = static_cast<s16>(trackWidth * std::clamp(mLastSnapshot.mOverallPercent, 0.0f, 1.0f));
    mProgressBarFill.SetXYWH(20, 205, fillWidth, 8);

    // Pad to a fixed width so the "X%" text doesn't shift left/right as the "..." ellipsis
    // animates.
    std::string dotsPadded = mDots;
    dotsPadded.resize(3, ' ');

    mCurMessage = "Data conversion in progress" + dotsPadded + " " + std::to_string(overallPercent) + "%";
    mTimer++;

    // Cheap sanity-check log for headless/log-based debugging - only fires when the weighted
    // percentage actually changes, not every frame.
    if (overallPercent != mLastLoggedPercent)
    {
        LOG_INFO("DataConversion progress: %d%%", overallPercent);
        mLastLoggedPercent = overallPercent;
    }

    if (mTimer > 5)
    {
        mTimer = 0;

        mDots += ".";
        
        if (mDots.length() > 3)
        {
            mDots.clear();
        }
    }
}

void DataConversionUI::VRender(OrderingTable& ot)
{
    //mLcdStatusBoard->VRender(ot);
    //mLcd->VRender(ot);

    gFontDrawScreenSpace = true;

    // polyOffset must track the NEW absolute offset AliveFont::DrawString returns (polyOffset +
    // however many characters it actually rendered) - NOT be added on top of via +=, since the
    // return value already includes the incoming polyOffset. Using += here double-counted it
    // every call (new = old + (old + rendered) = 2*old + rendered), so polyOffset shot up
    // exponentially across just a handful of calls (confirmed live: 43 -> 104 -> 228 -> 478 ->
    // 976 -> 1983 -> 3984 in one frame) instead of the ~13-line status+list actually needing well
    // under a hundred. That's also what was silently corrupting mFntPolyArray past its end before
    // the bounds check below existed - every character in a frame's later DrawString calls was
    // written far outside the array. DebugFont::PSX_DrawDebugTextBuffers (PsxDisplay.cpp) has the
    // same += pattern - harmless there so far only because it's never given a large starting
    // polyOffset, but it's the same latent bug.
    s32 polyOffset = 0;
    polyOffset = mFont.DrawString(ot, mCurMessage.c_str(), 20, (240) - 15, relive::TBlendModes::eBlend_0, 0, 0, Layer::eLayer_0, 127, 127, 127, polyOffset, FP_FromInteger(1), 640, 0);

    // Activity list: in-progress items first (warmer color - these are the long-running ones,
    // cameras/fmvs, that can otherwise sit unchanged for a while), then most-recently-finished
    // items (paths/animations/etc fly past quickly early on, settling into "converting: X.webm"
    // entries once only fmvs are left) - drawn oldest-of-the-shown-window at the top, newest at
    // the bottom, so new completions append below rather than pushing everything down from the
    // top. Sized to fill the space between the status line and the progress bar (y=20..~195,
    // border top at 203) rather than an arbitrary small count.
    s16 listY = 20;
    constexpr s16 kLineHeight = 10;
    constexpr s16 kMaxListLines = 17;
    constexpr size_t kMaxLineChars = 70;
    s32 linesDrawn = 0;

    // Keeps each line's contribution to this frame's shared mFntPolyArray budget bounded and
    // predictable, on top of DrawString's own out-of-bounds guard - belt and suspenders.
    const auto truncate = [](const std::string& s) -> std::string
    {
        return s.size() > kMaxLineChars ? (s.substr(0, kMaxLineChars - 3) + "...") : s;
    };

    for (const std::string& inProgress : mLastSnapshot.mInProgressItems)
    {
        if (linesDrawn >= kMaxListLines)
        {
            break;
        }
        const std::string line = "> " + truncate(inProgress);
        polyOffset = mFont.DrawString(ot, line.c_str(), 20, listY, relive::TBlendModes::eBlend_0, 0, 0, Layer::eLayer_0, 255, 200, 100, polyOffset, FP_FromInteger(1), 640, 0);
        listY += kLineHeight;
        linesDrawn++;
    }

    // mRecentItems is most-recent-first (see ConversionProgress::ReportItemFinished) - walk
    // backwards through the slice we're about to show so the oldest of that slice draws first
    // (top) and the newest draws last (bottom).
    const size_t recentBudget = (linesDrawn < kMaxListLines) ? static_cast<size_t>(kMaxListLines - linesDrawn) : 0;
    const size_t recentToShow = std::min(recentBudget, mLastSnapshot.mRecentItems.size());
    for (size_t i = 0; i < recentToShow; i++)
    {
        const std::string recent = truncate(mLastSnapshot.mRecentItems[recentToShow - 1 - i]);
        polyOffset = mFont.DrawString(ot, recent.c_str(), 20, listY, relive::TBlendModes::eBlend_0, 0, 0, Layer::eLayer_0, 150, 150, 150, polyOffset, FP_FromInteger(1), 640, 0);
        listY += kLineHeight;
        linesDrawn++;
    }

    // OrderingTable::Add prepends (mOrderingTable[layer] = pPrim; pPrim->mNext = <previous head>),
    // and DrawOTag() draws head-first - so whatever gets Add()'ed FIRST ends up drawn LAST (i.e.
    // painted on top), and whatever gets Add()'ed LAST is drawn FIRST (underneath). Add order here
    // (fill, track, border, mPoly) matches the desired front-to-back visual stack: fill on top of
    // track, track on top of the border "ring" around it, mPoly (opaque full-screen background)
    // underneath everything.
    ot.Add(Layer::eLayer_0, &mProgressBarFill);
    ot.Add(Layer::eLayer_0, &mProgressBarTrack);
    ot.Add(Layer::eLayer_0, &mProgressBarBorder);
    ot.Add(Layer::eLayer_0, &mPoly);

    gFontDrawScreenSpace = false;
}

bool DataConversionUI::ConversionRequired()
{
    DataConversion dataConversion;
    DataConversion::DataVersions zeroVersions;
    if (mGameType == GameType::eAe)
    {
        return dataConversion.DataVersionAE().value_or(zeroVersions).AnyConversionRequired();
    }
    else
    {
        return dataConversion.DataVersionAO().value_or(zeroVersions).AnyConversionRequired();
    }
}

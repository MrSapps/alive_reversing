#pragma once

#include "../GameObjects/BaseGameObject.hpp"
#include "../Primitives.hpp"
#include "../../relive_lib/Font.hpp"
#include "../../AliveLibAE/LCDScreen.hpp"
#include "../../AliveLibAE/LCDStatusBoard.hpp"
#include "GameType.hpp"
#include "data_conversion.hpp"
#include <thread>
#include <atomic>

class DataConversionUI final : public BaseGameObject
{
public:
    explicit DataConversionUI(GameType gameType, ResourceManagerWrapper& resMan, BaseMap& map);
    ~DataConversionUI();

    void VUpdate() override;

    void VRender(OrderingTable& ot) override;

    bool ConversionRequired();

    // Lets Engine::Run() (the only thing that owns a DataConversionUI, and so the only thing
    // that can reach one to begin with - deliberately not exposed any more broadly than that,
    // e.g. via a global) request cancellation of any FMV conversion job still running in the
    // background if the user quits while dcu.GetDead() hasn't fired yet, and wait for that to
    // actually happen before exiting for real. See ThreadPool::RequestCancel and
    // FmvConv::Convert's periodic IsCancelRequested() check.
    void RequestCancel();
    [[nodiscard]] bool AsyncTasksInProgress() const;

private:
    void ThreadFunc();

    GameType mGameType = GameType::eAe;
    Poly_G4 mPoly;
    Poly_G4 mProgressBarBorder;
    Poly_G4 mProgressBarTrack;
    Poly_G4 mProgressBarFill;
    std::unique_ptr<std::thread> mThread;
    std::atomic<bool> mDone{false};
    std::unique_ptr<DataConversion> mDataConversion;

    FontContext mFontContext;
    AliveFont mFont;

    u32 mTimer = 0;
    std::string mCurMessage;
    std::string mDots;
    ConversionProgress::Snapshot mLastSnapshot;
    s32 mLastLoggedPercent = -1;
    /*
    relive::Path_LCDScreen mLcdScreenParams = {};
    std::unique_ptr<LCDScreen> mLcd;

    relive::Path_LCDStatusBoard mLcdStatusBoardParams = {};
    std::unique_ptr<LCDStatusBoard> mLcdStatusBoard;
    */
};

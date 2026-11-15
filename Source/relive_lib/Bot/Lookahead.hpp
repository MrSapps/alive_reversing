#pragma once

#include "../Types.hpp"

namespace Bot {

// What the bot tries from a save before it plays on: the same thing begun a little later each time, against the
// real game, until one try gets to the next save unhurt. Nothing of the game is modelled: a try is the game itself
// run on from the save, and what failed is taken back. Only the try that worked is in the inputs a run leaves.
//
// This is the bookkeeping of one save's tries, apart from the game so it can be tested alone.
class Lookahead final
{
public:
    // How long at most a try holds back before it begins, and how fine the last pass over that is
    static constexpr s32 kWindow = 320;
    static constexpr s32 kCoarsest = 32;
    static constexpr s32 kFinest = 8;
    // This many tries that fail alike, however much later they began, and it is not a matter of when
    static constexpr s32 kAlike = 6;

    // The delays in the order they are tried: every 32nd frame of the window, then the ones half way between
    // those, and so on down to every 8th, none twice. -1 past the last.
    static s32 Delay(s32 index);
    static s32 Count();

    struct Stretch final
    {
        // The delay of the try under way (the first is to play straight on), and the next to try
        s32 mDelay = 0;
        s32 mNext = 1;
        // A try held back this long or longer fails: one was hurt that many frames in while it still held back,
        // and so will any other that does nothing for as long
        s32 mCeiling = kWindow + 1;
        // How often it has been begun again with a way ruled out
        s32 mRounds = 0;
        // The last try that failed once it had begun: how long after it began and where, and how many in a row
        // have now failed like that
        s32 mAlike = 0;
        s32 mAfter = 0;
        s32 mPlace = 0;
    };

    // The try under way failed that many frames after the save, at that place (any number that tells places
    // apart). The delay of the next try, or -1 when there is none left or none worth making: kAlike tries in a row, each
    // begun at another moment, that all failed as long after they began and at the same place would fail
    // whenever they began.
    static s32 Failed(Stretch& stretch, s32 frames, s32 place);
    // Something was ruled out: every delay is worth trying once more (what is known to be too long still is)
    static s32 Again(Stretch& stretch);
};

} // namespace Bot

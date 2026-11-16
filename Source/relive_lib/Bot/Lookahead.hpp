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

    // What a try does once it begins. When no moment gets the same thing through, another thing is tried at
    // each moment: the order they come in.
    enum class Way : u8
    {
        eSame,  // the buttons of the first try again, frame for frame (every coarsest step of the window)
        eAgain, // planned anew from what is found at that moment (the coarse moments, as for the next two)
        eQuiet, // planned anew, and every step of the way sneaked
        eFast,  // planned anew, and every step of the way run
        eFine,  // the buttons of the first try again, at the moments between the coarse ones
    };
    // Whether a try that way presses what the first try pressed
    static bool Repeats(Way way);

    struct Stretch final
    {
        // What the try under way does and its delay (the first is to play straight on), and the next to try
        Way mWay = Way::eSame;
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
    // apart). The delay of the next try that does the same thing, or -1 when there is none left or none worth
    // making: kAlike tries in a row, each begun at another moment, that all failed as long after they began and at
    // the same place would fail whenever they began.
    static s32 Failed(Stretch& stretch, s32 frames, s32 place);
    // No moment got that through: on to the next thing to try (stretch.mWay). Its first delay, or -1 when
    // everything has been tried.
    static s32 NextWay(Stretch& stretch);
    // Something was ruled out: every delay is worth trying once more (what is known to be too long still is)
    static s32 Again(Stretch& stretch);
};

} // namespace Bot

#pragma once

#include "../Types.hpp"

namespace Bot {

// The bot's own random numbers (xorshift64*), seeded by the run's seed. The game's random numbers are never touched,
// so a seed only changes what the bot chooses: which of two equal routes, how long it dawdles, which side it talks from.
class BotRng final
{
public:
    explicit BotRng(u64 seed = 1)
    {
        Seed(seed);
    }

    void Seed(u64 seed)
    {
        // Never zero, and nearby seeds start far apart
        mState = (seed + 0x9E3779B97F4A7C15ull) * 0xBF58476D1CE4E5B9ull;
        if (mState == 0)
        {
            mState = 0x2545F4914F6CDD1Dull;
        }
        Next();
        Next();
    }

    u64 Next()
    {
        mState ^= mState >> 12;
        mState ^= mState << 25;
        mState ^= mState >> 27;
        return mState * 0x2545F4914F6CDD1Dull;
    }

    // 0 .. count - 1, 0 for a count of 0
    u32 Below(u32 count)
    {
        return count == 0 ? 0 : static_cast<u32>((Next() >> 16) % count);
    }

    // lo .. hi, both included
    s32 Range(s32 lo, s32 hi)
    {
        return hi <= lo ? lo : lo + static_cast<s32>(Below(static_cast<u32>(hi - lo + 1)));
    }

    // True percent times in a hundred
    bool Chance(u32 percent)
    {
        return Below(100) < percent;
    }

    u64 State() const
    {
        return mState;
    }

    void SetState(u64 state)
    {
        mState = state;
    }

private:
    u64 mState = 0;
};

} // namespace Bot

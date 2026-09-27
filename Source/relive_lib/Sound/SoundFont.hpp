#pragma once

#include "../../relive_lib/Types.hpp"
#include <string>
#include <vector>

// A SoundFont 2 (SF2) file: reads and writes the parts the games' sound banks need. Mono 16 bit
// samples, no modulators (the default ones apply).
//
// Generators are kept as raw 16 bit amounts in file order, including numbers the SF2 spec
// doesn't define: the spec says players must ignore those, which is what the converted VABs
// use to keep the PS1 values SF2 can't hold (see VabSoundFont.hpp).
class SoundFont final
{
public:
    // SF2 generator numbers (SF2 2.04 section 8.1.2)
    enum Gen : u16
    {
        eReverbEffectsSend = 16,
        ePan = 17,
        eDelayVolEnv = 33,
        eAttackVolEnv = 34,
        eHoldVolEnv = 35,
        eDecayVolEnv = 36,
        eSustainVolEnv = 37,
        eReleaseVolEnv = 38,
        eInstrument = 41,
        eKeyRange = 43,
        eVelRange = 44,
        eInitialAttenuation = 48,
        eCoarseTune = 51,
        eFineTune = 52,
        eSampleId = 53,
        eSampleModes = 54,
        eOverridingRootKey = 58,
    };

    struct Generator final
    {
        u16 mOper = 0;
        u16 mAmount = 0;

        s16 Signed() const
        {
            return static_cast<s16>(mAmount);
        }
        u8 Lo() const
        {
            return static_cast<u8>(mAmount & 0xFF);
        }
        u8 Hi() const
        {
            return static_cast<u8>(mAmount >> 8);
        }
    };

    // A preset or instrument zone. An instrument zone ends with eSampleId and a preset zone
    // with eInstrument, except for a global zone, which may only come first.
    struct Zone final
    {
        std::vector<Generator> mGens;

        const Generator* Find(u16 oper) const;
        void Add(u16 oper, u16 amount)
        {
            mGens.push_back({oper, amount});
        }
        void AddRange(u16 oper, u8 lo, u8 hi)
        {
            mGens.push_back({oper, static_cast<u16>(lo | (hi << 8))});
        }
        bool IsGlobal(u16 terminalOper) const;
    };

    struct Sample final
    {
        std::string mName;
        std::vector<s16> mPcm;
        u32 mSampleRate = 44100;
        u8 mOriginalPitch = 60;
        s8 mPitchCorrection = 0;
        // Relative to the start of mPcm
        u32 mLoopStart = 0;
        u32 mLoopEnd = 0;
    };

    struct Instrument final
    {
        std::string mName;
        std::vector<Zone> mZones;
    };

    struct Preset final
    {
        std::string mName;
        u16 mProgram = 0;
        u16 mBank = 0;
        std::vector<Zone> mZones;
    };

    std::string mName;
    std::string mComment; // INFO ICMT
    std::vector<Sample> mSamples;
    std::vector<Instrument> mInstruments;
    std::vector<Preset> mPresets;

    std::vector<u8> Write() const;

    // False (with the reason in error) if data isn't an SF2 this can read
    static bool Read(const std::vector<u8>& data, SoundFont& out, std::string& error);
};

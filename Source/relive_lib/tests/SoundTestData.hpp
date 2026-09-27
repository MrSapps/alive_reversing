#pragma once

#include "Types.hpp"
#include <string>
#include <vector>

// Synthetic sound data in the games' own formats (VH, VB, sounds.dat, SEQ), so the sound code
// can be tested without the game files.
namespace SoundTestData {

// One VagAtr of a VH
struct Tone final
{
    s8 mPriority = 7;
    s8 mMode = 0;
    s8 mVol = 100;
    s8 mPan = 64;
    u8 mCentre = 60;
    u8 mShift = 0;
    s8 mMin = 0;
    s8 mMax = 127;
    s16 mAdsr1 = 0;
    s16 mAdsr2 = 0;
    s16 mVag = 1; // 1 based sample index
};

struct Program final
{
    s32 mNumber = 0;
    std::vector<Tone> mTones; // At most 16
};

struct Sample final
{
    std::vector<s16> mPcm;
    bool mLoop = false;
};

struct Vab final
{
    s32 mId = 0;
    std::vector<Program> mPrograms;
    std::vector<Sample> mSamples;
};

std::vector<u8> BuildVh(const Vab& vab);

// AE: each VB record points into sounds.dat, the samples are appended to soundsDat
std::vector<u8> BuildVbAe(const Vab& vab, std::vector<u8>& soundsDat);

// AO: the samples are in the VB
std::vector<u8> BuildVbAo(const Vab& vab);

// The VAB the sound gold tests use: several programs, key splits, layers, loops and envelopes
Vab LevelVab();
// A small VAB with id 0, standing in for MONK.VH/VB
Vab MonkVab();

std::vector<s16> Sine(s32 length, f64 period, s16 amplitude);
std::vector<s16> Saw(s32 length, s32 period, s16 amplitude);
std::vector<s16> Noise(s32 length, u32 seed, s16 amplitude);

// Builds a PSX SEQ ("pQES" header + SMF style event stream)
class SeqBuilder final
{
public:
    SeqBuilder(u16 resolution, u32 tempoUsPerQuarter);

    // Delta time (in ticks) before the next event
    SeqBuilder& Wait(u32 ticks);
    SeqBuilder& NoteOn(u8 channel, u8 note, u8 velocity);
    SeqBuilder& NoteOff(u8 channel, u8 note);
    // Note on with running status (the status byte of the previous event is reused)
    SeqBuilder& RunningNoteOn(u8 note, u8 velocity);
    SeqBuilder& ProgramChange(u8 channel, u8 program);
    SeqBuilder& Controller(u8 channel, u8 controller, u8 value);
    SeqBuilder& PitchBend(u8 channel, s32 bend); // -8192 to 8191
    // libsnd loop markers: NRPN 99 = 20/30 then data entry (CC 6)
    SeqBuilder& LoopStart(u8 channel, u8 count);
    SeqBuilder& LoopEnd(u8 channel);
    // Ends the event stream, no more events can be added
    std::vector<u8> End();

private:
    void PutVarLen(u32 value);

    std::vector<u8> mData;
    u32 mDelta = 0;
};

} // namespace SoundTestData

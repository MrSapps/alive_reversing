#pragma once

#include "SoundFont.hpp"
#include <string>
#include <vector>

// Converts the games' VAB sound banks (a VH header + the samples from the VB, and from
// sounds.dat for AE) to SF2 and back.
//
// Each VAB program is a preset (bank 0) with one instrument, whose zones are the program's tones.
// The zones carry the best standard SF2 equivalent of each tone, so the file plays in any SF2
// player, plus the tone's raw PS1 values in private generators (numbers the SF2 spec doesn't
// define, which players ignore). The header values go in the INFO comment. Converting back uses
// the raw values when they are there, so VAB -> SF2 -> VAB gives the engine the same tones, and
// derives them from the standard generators when they aren't (an SF2 made elsewhere). See
// SOUND_FORMATS.md.
class VabSoundFont final
{
public:
    // Private generator numbers
    enum PsxGen : u16
    {
        // Instrument zone: one per VagAtr field, and which of the program's 16 tones it is
        ePsxTone = 0x5000,
        ePsxPriority,
        ePsxMode,
        ePsxVol,
        ePsxPan,
        ePsxCentre,
        ePsxShift,
        ePsxMin,
        ePsxMax,
        ePsxVibW,
        ePsxVibT,
        ePsxPorW,
        ePsxPorT,
        ePsxPitchBendMin,
        ePsxPitchBendMax,
        ePsxAdsr1,
        ePsxAdsr2,
        ePsxVag,

        // Preset global zone: the ProgAtr fields
        ePsxProgTones = 0x5020,
        ePsxProgVol,
        ePsxProgPriority,
        ePsxProgMode,
        ePsxProgPan,
        ePsxProgAttr,
    };

    struct Sample final
    {
        std::vector<s16> mPcm;
        bool mLoop = false;
    };

    // What the engine loads: the VH file and each VAG's samples
    struct Vab final
    {
        std::vector<u8> mVh;
        std::vector<Sample> mSamples;
    };

    // The samples of a VB, as the games read them. AE's VB records point into sounds.dat, AO's
    // hold the samples. A missing or broken sample is empty. False if vh isn't a VH.
    static bool ReadVbAe(const std::vector<u8>& vh, const std::vector<u8>& vb, const std::vector<u8>& soundsDat, std::vector<Sample>& samples);
    static bool ReadVbAo(const std::vector<u8>& vh, const std::vector<u8>& vb, std::vector<Sample>& samples);

    static SoundFont ToSoundFont(const Vab& vab, const std::string& name);

    // A VH with the SF2's tones and its samples. False (with the reason in error) if it can't be
    // converted.
    static bool FromSoundFont(const SoundFont& sf2, Vab& vab, std::string& error);

    // The number of VAGs the engine loads for this VH, -1 if it isn't a VH
    static s32 VagCount(const std::vector<u8>& vh);
};

#pragma once

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdint.h>
#include <vector>
#include "Types.hpp"
#include <array>

class PSXADPCMDecoder final
{
public:
    PSXADPCMDecoder() = default;
    void DecodeFrameToPCM(std::array<s16, 4032>& out, uint8_t* arg_adpcm_frame);
    void DecodeFrameToPCM(std::vector<s16>& out, uint8_t* arg_adpcm_frame);

private:
    // ADPCM predictor history - must persist across successive DecodeFrameToPCM calls
    // for the same stream, but is scoped per-instance so two decoders (e.g. running on
    // different threads for different movies) don't stomp on each other's state.
    f64 mOldLeft = 0;
    f64 mOlderLeft = 0;
    f64 mOldRight = 0;
    f64 mOlderRight = 0;

public:
#pragma pack(push)
#pragma pack(1)
    struct SoundFrame final
    {
        struct SoundGroup final
        {
            uint8_t sound_parameters[16];
            uint8_t audio_sample_bytes[112];
        };

        SoundGroup sound_groups[18];
        uint8_t unused_for_adpcm[20];
        uint8_t edc[4];
    };

#pragma pack(pop)
};

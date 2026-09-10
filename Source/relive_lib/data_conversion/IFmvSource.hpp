#pragma once

#include <vector>
#include "../Types.hpp"

namespace relive
{
// Common interface implemented by both the AE (DDV/Masher) and AO (PSX STR) FMV
// sources so FmvConv::Convert's AV1 encode/mux pipeline can be shared by both games.
class IFmvSource
{
public:
    virtual ~IFmvSource() = default;

    [[nodiscard]] virtual bool ReadInfo() = 0;
    virtual bool StepFrame() = 0;

    [[nodiscard]] virtual u32 FrameWidth() = 0;
    [[nodiscard]] virtual u32 FrameHeight() = 0;
    [[nodiscard]] virtual u32 FrameRate() = 0;
    [[nodiscard]] virtual u32 TotalVideoFrames() = 0;
    [[nodiscard]] virtual u32 AudioSampleRate() = 0;
    [[nodiscard]] virtual u32 AudioChannels() = 0;
    [[nodiscard]] virtual u32 AudioBitsPerSample() = 0;

    [[nodiscard]] virtual const std::vector<u8>& GetPixels() const = 0;
    virtual std::vector<u8> GetAudioFrames() = 0;
};
} // namespace relive

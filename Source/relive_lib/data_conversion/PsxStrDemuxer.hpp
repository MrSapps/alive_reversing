#pragma once

#include "IFmvSource.hpp"
#include "AutoFile.hpp"
#include "../PSXMDECDecoder.h"
#include "../PSXADPCMDecoder.h"
#include <string>
#include <vector>

class FileSystem;

namespace relive
{
// Demuxes/decodes AO's original PSX "STR" FMV format (PC sector layout, "MOIR"/"VALE"
// sector markers) into RGBA video frames + PCM audio, mirroring the shape of DDVAe so
// FmvConv::Convert can drive either source through IFmvSource.
class PsxStrDemuxer final : public IFmvSource
{
public:
    PsxStrDemuxer(FileSystem& fs, const char_type* pStrName);

    [[nodiscard]] bool ReadInfo() override;
    bool StepFrame() override;

    [[nodiscard]] u32 FrameWidth() override { return static_cast<u32>(mFrameW); }
    [[nodiscard]] u32 FrameHeight() override { return static_cast<u32>(mFrameH); }
    [[nodiscard]] u32 FrameRate() override { return 15; }
    // Unknown up-front for a raw STR stream (no frame-count header field like DDV has) -
    // FmvConv::Convert loops on StepFrame() returning false to detect EOF instead.
    [[nodiscard]] u32 TotalVideoFrames() override { return 0; }
    [[nodiscard]] u32 AudioSampleRate() override { return 37800; }
    [[nodiscard]] u32 AudioChannels() override { return 1; }
    [[nodiscard]] u32 AudioBitsPerSample() override { return 16; }

    [[nodiscard]] const std::vector<u8>& GetPixels() const override { return mPixels; }
    std::vector<u8> GetAudioFrames() override;

private:
#pragma pack(push, 1)
    struct PsxVideoFrameHeader final
    {
        u16 mNumMdecCodes;
        u16 m3800Magic;
        u16 mQuantizationLevel;
        u16 mVersion;
    };

    struct PsxStrHeader final
    {
        u32 mSectorType; // "MOIR" or "VALE"
        u32 mSectorNumber;
        u32 mAkikMagic; // "AKIK" - PC replacement for the PSX 0x80010160 marker
        u16 mSectorNumberInFrame;
        u16 mNumSectorsInFrame;
        u32 mFrameNum;
        u32 mFrameDataLen;
        u16 mWidth;
        u16 mHeight;
        PsxVideoFrameHeader mVideoFrameHeader;
        u32 mNulls;
        u8 frame[2296];
    };
#pragma pack(pop)

    FileSystem& mFs;
    std::string mStrName;
    AutoFILE mFile;

    std::vector<u8> mDemuxBuffer;
    std::vector<u8> mPixels;
    std::vector<s16> mAdpcmOut;
    std::vector<u8> mPendingAudio;

    PSXMDECDecoder mMdec;
    PSXADPCMDecoder mAdpcm;

    s32 mFrameW = 0;
    s32 mFrameH = 0;
};
} // namespace relive

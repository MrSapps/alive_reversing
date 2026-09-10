#include "stdafx.h"
#include "PsxStrDemuxer.hpp"
#include "file_system.hpp"

#include <cstring>

namespace relive
{
static constexpr u32 kXaFrameDataSize = 2016;
static constexpr u32 kMoir = 0x52494f4d; // "MOIR"
static constexpr u32 kVale = 0x454c4156; // "VALE"
static constexpr u32 kAkik = 0x4b494b41; // "AKIK"

PsxStrDemuxer::PsxStrDemuxer(FileSystem& fs, const char_type* pStrName)
    : mFs(fs)
    , mStrName(pStrName)
{
}

bool PsxStrDemuxer::ReadInfo()
{
    if (mStrName.empty())
    {
        return false;
    }

    mFile = mFs.OpenFile(mStrName.c_str(), "rb");
    return mFile.GetFile() != nullptr;
}

std::vector<u8> PsxStrDemuxer::GetAudioFrames()
{
    std::vector<u8> tmp = std::move(mPendingAudio);
    mPendingAudio.clear();
    return tmp;
}

bool PsxStrDemuxer::StepFrame()
{
    if (mDemuxBuffer.empty())
    {
        mDemuxBuffer.resize(1024 * 1024);
    }

    mPendingAudio.clear();

    for (;;)
    {
        PsxStrHeader w = {};
        if (!mFile.Read(reinterpret_cast<u8*>(&w), sizeof(w)))
        {
            // EOF
            return false;
        }

        if (w.mSectorType == kMoir)
        {
            if (w.mAkikMagic != kAkik)
            {
                // Corrupt/unexpected sector - stop converting this movie rather than
                // crashing the whole batch conversion.
                return false;
            }

            const u16 frameW = w.mWidth;
            const u16 frameH = w.mHeight;

            u32 bytesToCopy = w.mFrameDataLen - w.mSectorNumberInFrame * kXaFrameDataSize;
            if (bytesToCopy > 0)
            {
                if (bytesToCopy > kXaFrameDataSize)
                {
                    bytesToCopy = kXaFrameDataSize;
                }

                std::memcpy(mDemuxBuffer.data() + w.mSectorNumberInFrame * kXaFrameDataSize, w.frame, bytesToCopy);
            }

            if (w.mSectorNumberInFrame == w.mNumSectorsInFrame - 1)
            {
                // Always resize as its possible for a stream to change its frame size to be
                // smaller or larger (this happens in the AE PSX MI.MOV streams).
                mPixels.resize(static_cast<size_t>(frameW) * frameH * 4); // 4 bytes per pixel

                mFrameW = frameW;
                mFrameH = frameH;

                mMdec.DecodeFrameToRGBA32(reinterpret_cast<uint16_t*>(mPixels.data()), reinterpret_cast<uint16_t*>(mDemuxBuffer.data()), frameW, frameH);
                return true;
            }
        }
        else if (w.mSectorType == kVale)
        {
            mAdpcmOut.resize(2016 * 2);
            mAdpcm.DecodeFrameToPCM(mAdpcmOut, reinterpret_cast<uint8_t*>(&w.mAkikMagic));

            const size_t offset = mPendingAudio.size();
            mPendingAudio.resize(offset + (mAdpcmOut.size() * sizeof(s16)));
            std::memcpy(mPendingAudio.data() + offset, mAdpcmOut.data(), mAdpcmOut.size() * sizeof(s16));
        }
        else
        {
            // Either the end-of-stream marker (0x10101010) or an unexpected sector -
            // stop converting this movie rather than crashing the whole batch conversion.
            return false;
        }
    }
}
} // namespace relive

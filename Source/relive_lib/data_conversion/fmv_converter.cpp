#include "fmv_converter.hpp"
#include "../../AliveLibAE/PathData.hpp"
#include "../../AliveLibAO/PathData.hpp"
#include "../FatalError.hpp"
#include "PNGFile.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4505)
#endif
#include "aom/aom_encoder.h"
#include "aom/aomcx.h"
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

#include "aom/third_party/libwebm/mkvmuxer/mkvmuxer.h"
#include "aom/third_party/libwebm/mkvmuxer/mkvmuxerutil.h"
#include "aom/third_party/libwebm/mkvmuxer/mkvwriter.h"

#include "aom/common/av1_config.h"

#include "../Masher.hpp"
#include "rgb_conversion.hpp"
#include "file_system.hpp"

#include "DDVAe.hpp"
#include "PsxStrDemuxer.hpp"
#include "ThreadPool.hpp"

// Some FMV sources (e.g. AO's raw PSX STR streams, which have no header - each frame's
// width/height comes from its own MOIR sector) don't know their true frame size or frame
// count until every frame has been decoded once. For those, the caller does a throwaway
// pass with ScanFmvSource() first to find the largest frame size and the frame count -
// also handy groundwork for real conversion progress reporting later.
struct FmvScanResult final
{
    u32 mMaxWidth = 0;
    u32 mMaxHeight = 0;
    u32 mFrameCount = 0;
};

static FmvScanResult ScanFmvSource(relive::IFmvSource& source)
{
    FmvScanResult result;
    if (!source.ReadInfo())
    {
        return result;
    }

    while (source.StepFrame())
    {
        result.mMaxWidth = std::max(result.mMaxWidth, source.FrameWidth());
        result.mMaxHeight = std::max(result.mMaxHeight, source.FrameHeight());
        ++result.mFrameCount;
    }

    return result;
}

class FmvConv final
{
public:
    explicit FmvConv(FileSystem& fs)
        : mFs(fs)
    {

    }

    // pScan, when given, overrides the source's own (possibly unknown/0) frame size and
    // frame count - see ScanFmvSource() above.
    void Convert(relive::IFmvSource& source, std::string fName, const FileSystem::Path& outDir, const FmvScanResult* pScan = nullptr)
    {
        TRACE_ENTRYEXIT;

        mCurrentMovieName = fName;

        if (!source.ReadInfo())
        {
            LOG_WARNING("Failed to open FMV source '%s'", fName.c_str());
            // At least one retail FMV is missing for some reason
            return;
        }

        const u32 width = (pScan && pScan->mMaxWidth > 0) ? pScan->mMaxWidth : (source.FrameWidth() > 0 ? source.FrameWidth() : 640u);
        const u32 height = (pScan && pScan->mMaxHeight > 0) ? pScan->mMaxHeight : (source.FrameHeight() > 0 ? source.FrameHeight() : 240u);

        LOG_INFO("FMV '%s' dimensions: %ux%u (header reported %ux%u)", fName.c_str(), width, height,
                 source.FrameWidth(), source.FrameHeight());

        // TODO: FIX ME - hack to 15
        const u32 frameRate = 15; //source.FrameRate() > 0 ? source.FrameRate() : 15u;
        const u32 audioSampleRate = source.AudioSampleRate() > 0 ? source.AudioSampleRate() : 44100u;
        const u32 audioChannels = source.AudioChannels() > 0 ? source.AudioChannels() : 2u;
        const u32 audioBitsPerSample = source.AudioBitsPerSample() > 0 ? source.AudioBitsPerSample() : 16u;
        std::vector<u8> frameBuffer(width * height * sizeof(u32));

        aom_codec_iface_t* encoder = &aom_codec_av1_cx_algo;
        if (!encoder)
        {
            ALIVE_FATAL("Unsupported codec.");
        }

        LOG_INFO("Encoding AV1 FMV: %ux%u @ %u fps (real-time settings)", width, height, frameRate > 0 ? frameRate : 15u);

        aom_image_t rawImageFrameData;
        if (!aom_img_alloc(&rawImageFrameData, AOM_IMG_FMT_I420, width, height, 1))
        {
            ALIVE_FATAL("Failed to allocate image.");
        }

        aom_codec_enc_cfg_t cfg = {};
        if (aom_codec_enc_config_default(encoder, &cfg, AOM_USAGE_REALTIME))
        {
            ALIVE_FATAL("Failed to get default codec config.");
        }
        cfg.g_w = width;
        cfg.g_h = height;
        cfg.g_timebase.num = 1;
        cfg.g_timebase.den = frameRate > 0 ? frameRate : 15; // fps
        cfg.g_lag_in_frames = 0;
        cfg.g_threads = std::max(1u, std::thread::hardware_concurrency() / 2u);

        // AOM_USAGE_REALTIME's default kf_mode is AOM_KF_DISABLED - we were only ever getting
        // the single forced keyframe at frame 0 below, and nothing else for the rest of the
        // movie (confirmed via ffprobe: exactly one K frame in a 530-frame file). Besides making
        // every seek in an external player replay the whole movie from the start to get there,
        // that's also more fragile than it needs to be - losing/corrupting any single P-frame
        // breaks every frame after it for the rest of the movie, instead of just until the next
        // keyframe.
        cfg.kf_mode = AOM_KF_AUTO;
        cfg.kf_min_dist = 0;
        // 1s, matching normal seek-to-the-second expectations from typical video files. The
        // usual reason to space keyframes out further (they're much bigger than a P-frame, so
        // more of them costs real bitrate) barely applies at this tiny 320x240/~15fps size - a
        // keyframe here only runs about 3.8KB more than an average P-frame (measured via
        // ffprobe), so a 1s cadence instead of 2s only adds a few % to typical FMV file sizes.
        cfg.kf_max_dist = (frameRate > 0 ? frameRate : 15) * 1;

        aom_codec_ctx_t codec = {};
        if (aom_codec_enc_init(&codec, encoder, &cfg, 0))
        {
            ALIVE_FATAL("Failed to initialize encoder");
        }

        const int speed = 8;
        if (aom_codec_control(&codec, AOME_SET_CPUUSED, speed))
        {
            ALIVE_FATAL("Failed to set cpu-used");
        }

        FileSystem::Path outPathBuilder = outDir;
        outPathBuilder.Append(fName + ".webm");
        const std::string outFileName = outPathBuilder.GetPath();
        FILE* outFile = fopen(outFileName.c_str(), "wb");
        if (!outFile)
        {
            ALIVE_FATAL("Failed to open output file '%s'", outFileName.c_str());
        }

        {
            mkvmuxer::MkvWriter writer(outFile);
            mkvmuxer::Segment segment;
            mAudioSampleRate = audioSampleRate;
            mAudioChannels = audioChannels;
            mAudioBitsPerSample = audioBitsPerSample;
            mkv_init(&writer, &segment, &cfg, &codec);

            const auto clamp_to_u8 = [](s32 value) -> u8
            {
                if (value < 0)
                {
                    return 0;
                }
                if (value > 255)
                {
                    return 255;
                }
                return static_cast<u8>(value);
            };

            const auto convert_rgba_to_i420 = [&]()
            {
                for (u32 y = 0; y < height; ++y)
                {
                    u8* yPlane = rawImageFrameData.planes[0] + (y * rawImageFrameData.stride[0]);
                    for (u32 x = 0; x < width; ++x)
                    {
                        const RGBA32& p = ((RGBA32*)frameBuffer.data())[(y * width) + x];
                        const s32 r = p.r;
                        const s32 g = p.g;
                        const s32 b = p.b;
                        const s32 yVal = (77 * r + 150 * g + 29 * b) >> 8;
                        yPlane[x] = clamp_to_u8(yVal);
                    }
                }

                const u32 chromaWidth = (width + 1) / 2;
                const u32 chromaHeight = (height + 1) / 2;
                for (u32 y = 0; y < chromaHeight; ++y)
                {
                    u8* uPlane = rawImageFrameData.planes[1] + (y * rawImageFrameData.stride[1]);
                    u8* vPlane = rawImageFrameData.planes[2] + (y * rawImageFrameData.stride[2]);
                    for (u32 x = 0; x < chromaWidth; ++x)
                    {
                        const u32 sampleX = x * 2;
                        const u32 sampleY = y * 2;
                        s32 rTotal = 0;
                        s32 gTotal = 0;
                        s32 bTotal = 0;
                        const u32 sampleCount = 4;
                        for (u32 dy = 0; dy < 2 && sampleY + dy < height; ++dy)
                        {
                            for (u32 dx = 0; dx < 2 && sampleX + dx < width; ++dx)
                            {
                                const RGBA32& p = ((RGBA32*)frameBuffer.data())[((sampleY + dy) * width) + (sampleX + dx)];
                                rTotal += p.r;
                                gTotal += p.g;
                                bTotal += p.b;
                            }
                        }

                        const s32 rAvg = rTotal / sampleCount;
                        const s32 gAvg = gTotal / sampleCount;
                        const s32 bAvg = bTotal / sampleCount;
                        const s32 uVal = (-43 * rAvg - 85 * gAvg + 128 * bAvg) >> 8;
                        const s32 vVal = (128 * rAvg - 107 * gAvg - 21 * bAvg) >> 8;
                        uPlane[x] = clamp_to_u8(uVal + 128);
                        vPlane[x] = clamp_to_u8(vVal + 128);
                    }
                }
            };

            const u32 totalFrames = (pScan && pScan->mFrameCount > 0) ? pScan->mFrameCount : source.TotalVideoFrames();
            u32 frame_index = 0;
            while (totalFrames == 0 || frame_index < totalFrames)
            {
                if (!source.StepFrame())
                {
                    break;
                }

                const std::vector<u8>& srcPixels = source.GetPixels();
                const u32 frameW = source.FrameWidth();
                const u32 frameH = source.FrameHeight();

                if (frameW == width && frameH == height)
                {
                    frameBuffer = srcPixels;
                }
                else
                {
                    // The source changed frame size mid-stream (seen in some AO PSX STR
                    // streams). Composite into the top-left of the canvas sized to the
                    // largest frame seen (see ScanFmvSource()), clearing first so a
                    // shrinking frame doesn't leave stale pixels behind; a frame somehow
                    // bigger than the canvas gets truncated to fit it.
                    std::fill(frameBuffer.begin(), frameBuffer.end(), 0);
                    const u32 copyW = std::min(frameW, width);
                    const u32 copyH = std::min(frameH, height);
                    for (u32 y = 0; y < copyH; ++y)
                    {
                        std::memcpy(frameBuffer.data() + (static_cast<size_t>(y) * width * sizeof(u32)),
                                    srcPixels.data() + (static_cast<size_t>(y) * frameW * sizeof(u32)),
                                    static_cast<size_t>(copyW) * sizeof(u32));
                    }
                }

                const std::vector<u8> audioFrames = source.GetAudioFrames();
                if (!audioFrames.empty() && mAudioTrackNumber != 0)
                {
                    const u32 bytesPerSampleFrame = (mAudioBitsPerSample / 8u) * mAudioChannels;
                    if (bytesPerSampleFrame > 0 && (audioFrames.size() % bytesPerSampleFrame) == 0)
                    {
                        const u64 sampleFrameCount = static_cast<u64>(audioFrames.size() / bytesPerSampleFrame);
                        // PTS from a running total of samples actually written so far, not
                        // callCount * thisChunkSize - AO's audio chunks aren't fixed size (a
                        // variable number of VALE sectors can land between two video frames),
                        // so that would make the PTS jump around non-monotonically.
                        const u64 audioPtsNs = (mAudioSamplesWritten * 1000000000ULL) / static_cast<u64>(mAudioSampleRate);
                        const bool ok = segment.AddFrame(audioFrames.data(), static_cast<uint64_t>(audioFrames.size()), mAudioTrackNumber, audioPtsNs, false);
                        if (!ok)
                        {
                            fprintf(stderr, "webmenc> AddAudioFrame failed.\n");
                        }
                        mAudioSamplesWritten += sampleFrameCount;
                    }
                }

                // frameBuffer

                /*
                if (!ddv.ReadVideoFrame(frameBuffer.data()))
                {
                    break;
                }*/


                convert_rgba_to_i420();
                const int flags = (frame_index == 0) ? AOM_EFLAG_FORCE_KF : 0;

                // Tie video timestamps to the same real (sample-counted) audio clock rather
                // than an assumed-constant frame rate - AO's true playback rate isn't a clean
                // 15fps (see the frameRate hack above), so the two would otherwise drift apart
                // over a long enough movie until a video frame's assumed-rate timestamp landed
                // behind audio's real-rate timestamp, which the muxer rejects as non-monotonic.
                const int64_t videoPtsNs = (mAudioTrackNumber != 0)
                    ? static_cast<int64_t>((mAudioSamplesWritten * 1000000000ULL) / static_cast<u64>(mAudioSampleRate))
                    : -1;
                encode_frame(&segment, &cfg, &codec, &rawImageFrameData, static_cast<int>(frame_index), flags, videoPtsNs);

                if (totalFrames > 0)
                {
                    const double percentDone = (static_cast<double>(frame_index + 1u) / static_cast<double>(totalFrames)) * 100.0;
                    LOG_INFO("Video frame %u/%u (%.2f%%)", frame_index + 1u, totalFrames, percentDone);
                }
                else
                {
                    LOG_INFO("Video frame %u (total unknown)", frame_index + 1u);
                }

                ++frame_index;
            }

            while (encode_frame(&segment, &cfg, &codec, nullptr, -1, 0))
            {
                continue;
            }

            const bool ok = segment.Finalize();
            if (!ok)
            {
                fprintf(stderr, "webmenc> Segment::Finalize failed.\n");
            }

            fclose(outFile);
        }

        aom_img_free(&rawImageFrameData);
    }

private:

    // videoPtsNs, when >= 0, overrides the timestamp derived from the encoder's own pts.
    // The caller uses this to tie video timestamps to the same real (sample-counted)
    // audio clock as mkv audio frames - see the call site for why.
    int mkv_write_block(mkvmuxer::Segment* segment, const aom_codec_enc_cfg_t* cfg, const aom_codec_cx_pkt_t* pkt, int64_t videoPtsNs)
    {
        int64_t pts_ns = videoPtsNs >= 0 ? videoPtsNs : (pkt->data.frame.pts * 1000000000ll * cfg->g_timebase.num / cfg->g_timebase.den);
        if (pts_ns <= mLast_pts_ns)
        {
            pts_ns = mLast_pts_ns + 1000000;
        }

        mLast_pts_ns = pts_ns;

        if (!segment->AddFrame(static_cast<uint8_t*>(pkt->data.frame.buf),
                               pkt->data.frame.sz, kVideoTrackNumber, pts_ns,
                               pkt->data.frame.flags & AOM_FRAME_IS_KEY))
        {
            fprintf(stderr, "webmenc> AddFrame failed.\n");
            return -1;
        }
        return 0;
    }

    int mkv_init(mkvmuxer::MkvWriter* writer, mkvmuxer::Segment* segment, aom_codec_enc_cfg_t* cfg, aom_codec_ctx_t* codec)
    {
        mLast_pts_ns = 0;

        bool ok = segment->Init(writer);
        if (!ok)
        {
            fprintf(stderr, "webmenc> mkvmuxer Init failed.\n");
            return -1;
        }

        segment->set_mode(mkvmuxer::Segment::kFile);
        segment->OutputCues(true);

        if (mAudioSampleRate > 0 && mAudioChannels > 0 && mAudioBitsPerSample > 0)
        {
            const uint64_t audio_track_id = segment->AddAudioTrack(static_cast<int32_t>(mAudioSampleRate), static_cast<int32_t>(mAudioChannels), kAudioTrackNumber);
            mkvmuxer::AudioTrack* const audio_track = static_cast<mkvmuxer::AudioTrack*>(segment->GetTrackByNumber(audio_track_id));
            if (!audio_track)
            {
                fprintf(stderr, "webmenc> Audio track creation failed.\n");
                return -1;
            }
            // Matroska PCM uses the standard codec ID for little-endian integer PCM.
            // "A_PCM" by itself is not a recognized codec string for most players.
            audio_track->set_codec_id("A_PCM/INT/LIT");
            audio_track->set_bit_depth(mAudioBitsPerSample);
            mAudioTrackNumber = audio_track_id;
        }

        mkvmuxer::SegmentInfo* const info = segment->GetSegmentInfo();
        if (!info)
        {
            fprintf(stderr, "webmenc> Cannot retrieve Segment Info.\n");
            return -1;
        }

        const uint64_t kTimecodeScale = 1000000;
        info->set_timecode_scale(kTimecodeScale);
        std::string version = "aomenc";
        /*
        if (!webm_ctx->debug)
        {
            version.append(std::string(" ") + aom_codec_version_str());
        }*/

        info->set_writing_app(version.c_str());


        const uint64_t video_track_id = segment->AddVideoTrack(static_cast<int>(cfg->g_w),
                                                               static_cast<int>(cfg->g_h), kVideoTrackNumber);
        mkvmuxer::VideoTrack* const video_track = static_cast<mkvmuxer::VideoTrack*>(
            segment->GetTrackByNumber(video_track_id));

        if (!video_track)
        {
            fprintf(stderr, "webmenc> Video track creation failed.\n");
            return -1;
        }

        ok = false;
        aom_fixed_buf_t* obu_sequence_header = aom_codec_get_global_headers(codec);
        if (obu_sequence_header)
        {
            Av1Config av1_config;
            if (get_av1config_from_obu(
                    reinterpret_cast<const uint8_t*>(obu_sequence_header->buf),
                    obu_sequence_header->sz, false, &av1_config)
                == 0)
            {
                uint8_t av1_config_buffer[4] = {0};
                size_t bytes_written = 0;
                if (write_av1config(&av1_config, sizeof(av1_config_buffer),
                                    &bytes_written, av1_config_buffer)
                    == 0)
                {
                    ok = video_track->SetCodecPrivate(av1_config_buffer,
                                                      sizeof(av1_config_buffer));
                }
            }
            free(obu_sequence_header->buf);
            free(obu_sequence_header);
        }
        if (!ok)
        {
            fprintf(stderr, "webmenc> Unable to set AV1 config.\n");
            return -1;
        }

        ok = video_track->SetStereoMode(1); // STEREO_FORMAT_LEFT_RIGHT
        if (!ok)
        {
            fprintf(stderr, "webmenc> Unable to set stereo mode.\n");
            return -1;
        }

        video_track->set_codec_id("V_AV1");

        /*
        // Default to 1:1 pixel aspect ratio.
        input->pixel_aspect_ratio.numerator = 1;
        input->pixel_aspect_ratio.denominator = 1;

        if (par->numerator > 1 || par->denominator > 1)
        {
            const uint64_t display_width = static_cast<uint64_t>(((cfg->g_w * par->numerator * 1.0) / par->denominator) + .5);
            video_track->set_display_width(display_width);
            video_track->set_display_height(cfg->g_h);
        }
        */

        /*
        if (encoder_settings != nullptr)
        {
            mkvmuxer::Tag* tag = segment->AddTag();
            if (tag == nullptr)
            {
                fprintf(stderr, "webmenc> Unable to allocate memory for encoder settings tag.\n");
                return -1;
            }
            ok = tag->add_simple_tag("ENCODER_SETTINGS", encoder_settings);
            if (!ok)
            {
                fprintf(stderr, "webmenc> Unable to allocate memory for encoder settings tag.\n");
                return -1;
            }
        }*/

        /*
        if (webm_ctx->debug)
        {
            video_track->set_uid(kDebugTrackUid);
        }*/

        // webm_ctx->writer = writer.release();
        // webm_ctx->segment = segment.release();
        return 0;
    }

    bool encode_frame(mkvmuxer::Segment* segment, const aom_codec_enc_cfg_t* cfg, aom_codec_ctx_t* codec, aom_image_t* img, int frame_index, int flags, int64_t videoPtsNs = -1)
    {
        bool got_pkts = false;
        aom_codec_iter_t iter = nullptr;
        const aom_codec_cx_pkt_t* pkt = nullptr;
        const auto start = std::chrono::steady_clock::now();
        const aom_codec_err_t res = aom_codec_encode(codec, img, frame_index, 1, flags);
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

        if (res != AOM_CODEC_OK)
        {
            const char* const detail = aom_codec_error_detail(codec);
            ALIVE_FATAL("Failed to encode frame %d of FMV '%s': %s (%s)", frame_index, mCurrentMovieName.c_str(),
                        aom_codec_err_to_string(res), detail ? detail : aom_codec_error(codec));
        }

        if (elapsedMs > 1000)
        {
            LOG_INFO("Encoding frame %d took %lld ms", frame_index, static_cast<long long>(elapsedMs));
        }

        while ((pkt = aom_codec_get_cx_data(codec, &iter)) != nullptr)
        {
            got_pkts = true;

            if (pkt->kind == AOM_CODEC_CX_FRAME_PKT)
            {
                const int keyframe = (pkt->data.frame.flags & AOM_FRAME_IS_KEY) != 0;

                if (mkv_write_block(segment, cfg, pkt, videoPtsNs) != 0)
                {
                    ALIVE_FATAL("Failed to write compressed frame %d of FMV '%s'", frame_index, mCurrentMovieName.c_str());
                }
                LOG_INFO(keyframe ? "K" : ".");
            }
        }

        return got_pkts;
    }

private:
    FileSystem& mFs;
    std::string mCurrentMovieName;
    const int kVideoTrackNumber = 1;
    const int kAudioTrackNumber = 2;
    int64_t mLast_pts_ns = 0;
    uint64_t mAudioTrackNumber = 0;
    uint64_t mAudioSamplesWritten = 0;
    uint32_t mAudioSampleRate = 0;
    uint32_t mAudioChannels = 0;
    uint32_t mAudioBitsPerSample = 0;
};

class ConvertFmvJob final : public IJob
{
public:
    ConvertFmvJob(FileSystem& fs, std::string movieName, FileSystem::Path outDir, bool isAo)
        : mFs(fs)
        , mMovieName(std::move(movieName))
        , mOutDir(std::move(outDir))
        , mIsAo(isAo)
    {
    }

    void Execute() override
    {
        LOG_INFO("ConvertFmvJob: starting '%s' (isAo=%d)", mMovieName.c_str(), mIsAo ? 1 : 0);

        FmvConv fmvConv(mFs);
        if (mIsAo)
        {
            // AO's raw STR frames have no stream header - the true max frame size and
            // frame count are only known after decoding every frame once, so do that as
            // a throwaway pre-pass before the real (encoding) pass.
            relive::PsxStrDemuxer scanSource(mFs, mMovieName.c_str());
            const FmvScanResult scan = ScanFmvSource(scanSource);
            LOG_INFO("ConvertFmvJob: '%s' scanned max %ux%u over %u frames", mMovieName.c_str(),
                     scan.mMaxWidth, scan.mMaxHeight, scan.mFrameCount);

            relive::PsxStrDemuxer source(mFs, mMovieName.c_str());
            fmvConv.Convert(source, mMovieName, mOutDir, &scan);
        }
        else
        {
            // AE's DDV header already declares a fixed frame size and frame count up
            // front, so no pre-pass is needed here.
            relive::DDVAe source(mFs, mMovieName.c_str(), nullptr);
            fmvConv.Convert(source, mMovieName, mOutDir);
        }
    }

private:
    FileSystem& mFs;
    std::string mMovieName;
    FileSystem::Path mOutDir;
    bool mIsAo = false;
};

void ConvertFMVs(ThreadPool& tp, FileSystem& fs, const FileSystem::Path& dataDir, bool isAo)
{
    FileSystem::Path fmvOutDir = dataDir;
    fmvOutDir.Append("fmvs");
    fs.CreateDirectory(fmvOutDir);

    // Real, per-level FMV filenames straight from each game's own reversed FmvInfo
    // tables (AliveLibAE/AliveLibAO PathData.cpp) rather than a separately hand
    // maintained list here.
    const std::vector<std::string> movieNames = isAo ? AO::Path_GetAllFmvNames() : ::Path_GetAllFmvNames();

    for (const auto& movieName : movieNames)
    {
        tp.AddJob(std::make_unique<ConvertFmvJob>(fs, movieName, fmvOutDir, isAo));
    }
}

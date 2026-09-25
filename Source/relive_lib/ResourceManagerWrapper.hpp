#pragma once

#include "AnimResources.hpp"
#include <mutex>
#include <condition_variable>
#include <set>

enum class AnimId;
enum class EReliveLevelIds : s16;

class BinaryPath;
struct AnimationPal;

class UniqueResId final
{
public:
    UniqueResId()
        : mId(NextGlobalId())
    {
    }

    u32 Id() const
    {
        return mId;
    }

private:
    static u32 NextGlobalId()
    {
        mGlobalId++;
        return mGlobalId;
    }

    u32 mId = 0;
    static u32 mGlobalId;
};

struct PngData final
{
    std::shared_ptr<AnimationPal> mPal;
    std::vector<u8> mPixels;
    u32 mWidth;
    u32 mHeight;
};

struct Point32 final
{
    s32 x = 0;
    s32 y = 0;
};

struct IndexedPoint final
{
    u32 mIndex = 0;
    Point32 mPoint;
};

struct PerFrameInfo final
{
    s32 mXOffset = 0;
    s32 mYOffset = 0;
    u32 mWidth = 0;
    u32 mHeight = 0;
    u32 mSpriteWidth = 0;
    u32 mSpriteHeight = 0;
    u32 mSpriteSheetX = 0;
    u32 mSpriteSheetY = 0;
    Point32 mBoundMin;
    Point32 mBoundMax;
    u32 mPointCount = 0;
    IndexedPoint mPoints[2] = {};
};

struct AnimAttributes final
{
    u32 mFrameRate;
    bool mFlipX;
    bool mFlipY;
    bool mLoop;
    u32 mLoopStartFrame;
    u32 mMaxWidth;
    u32 mMaxHeight;
};

class AnimationAttributesAndFrames final
{
public:
    explicit AnimationAttributesAndFrames(const std::string& jsonData);
    AnimAttributes mAttributes;
    std::vector<PerFrameInfo> mFrames;
};

class PalResource final
{
public:
    PalId mId = PalId::Default;
    std::shared_ptr<AnimationPal> mPal;
};

class RgbaData final
{
public:
    u32 mWidth = 0;
    u32 mHeight = 0;
    std::shared_ptr<std::vector<u8>> mPixels;
};

class CamResource final
{
public:
    UniqueResId mUniqueId;
    RgbaData mData;
};

class Fg1Layer final
{
public:
    UniqueResId mUniqueId;
    RgbaData mImage;
};

class Fg1Resource final
{
public:
    u32 mFg1ResBlockCount = 0;
    Fg1Layer mFg;
    Fg1Layer mFgWell;
    Fg1Layer mBg;
    Fg1Layer mBgWell;

    bool Any() const
    {
        return mFg.mImage.mPixels || mFgWell.mImage.mPixels || mBg.mImage.mPixels || mBgWell.mImage.mPixels;
    }
};

class AnimResource final
{
public:
    AnimResource()
    {

    }

    AnimResource(AnimId id, std::shared_ptr<AnimationAttributesAndFrames>& jsonRes, std::shared_ptr<PngData>& pngRes)
        : mId(id)
        , mJsonPtr(jsonRes)
        , mPngPtr(pngRes)
    {
        mCurPal = mPngPtr->mPal;
    }

    void Clear()
    {
        mId = AnimId::None;
        mJsonPtr = nullptr;
        mPngPtr = nullptr;
    }

public:
    UniqueResId mUniqueId;
    AnimId mId = AnimId::None;
    std::shared_ptr<AnimationAttributesAndFrames> mJsonPtr;
    std::shared_ptr<PngData> mPngPtr;
    // TODO: weak_ptr
    std::shared_ptr<AnimationPal> mCurPal;
};

enum class FontType
{
    None,
    PauseMenu,
    LcdFont,
    Debug,
};

class FontResource final
{
public:
    FontResource() = default;

    FontResource(FontType id, std::shared_ptr<PngData>& pngPtr)
        : mId(id), mPngPtr(pngPtr)
    {
        mCurPal = pngPtr->mPal;
    }

    UniqueResId mUniqueId;
    FontType mId = FontType::None;
    // TODO: Font atlas json ptpr
    std::shared_ptr<PngData> mPngPtr;

    // TODO: Really should be a weak_ptr
    std::shared_ptr<AnimationPal> mCurPal;
};


class ThreadPool;
class FileSystem;
class IniFile;

// Temp adapter interface
// When the main loop shows the loading icon while waiting, see
// ResourceManagerWrapper::RequestLoadingWait
enum class LoadingIcon
{
    eIfSlow, // once the wait has taken a while
    eNow,    // straight away, even if nothing is loading
};

class ResourceManagerWrapper final
{
public:
    ResourceManagerWrapper(FileSystem& fs, const std::string& modPath);

    // The user's settings - input bindings and display settings - in relive.ini in the game
    // directory (not looked up through the search paths).
    // The raw text is for GameAutoPlayer::RestoreFileBuffer, so recordings replay the settings
    // they were recorded with.
    std::string LoadSettingsIniText();
    IniFile LoadSettingsIni();
    bool SaveSettingsIni(const IniFile& ini);

    // A demo's recorded input (PLAYBK*.JOY.json) or starting save (ATTR*.SAV.json), in the game
    // directory. Empty if it's missing.
    std::string LoadDemoFile(const std::string& fileName);
    ~ResourceManagerWrapper();

    // TODO: Remove/unify when both games resource managers are merged into one object
    enum ResourceType : u32
    {
        Resource_PBuf = 0x66754250,
        Resource_CHNK = 0x4B4E4843,
        Resource_DecompressionBuffer = 0x66754244,
        Resource_VLC = 0x20434C56,
        Resource_Animation = 0x6D696E41,
        Resource_VabHeader = 0x48424156,
        Resource_VabBody = 0x42424156,
        Resource_Font = 0x746E6F46,
        Resource_Path = 0x68746150,
        Resource_Palt = 0x746C6150,
        Resource_FG1 = 0x20314746,
        Resource_Bits = 0x73746942,
        Resource_Blood = 0x64756C42,
        Resource_Sprx = 0x78727053,
        Resource_FntP = 0x50746E46,
        Resource_3DGibs = 0x65444433,
        Resource_HintFly = 0x796C4648,
        Resource_Spline = 0x6e6c7053,
        Resource_Wave = 0x65766157,
        Resource_Free = 0x65657246,
        Resource_Pend = 0x646E6550,
        Resource_End = 0x21646E45,
        Resource_Plbk = 0x6B626C50,
        Resource_Play = 0x79616C50,
        Resource_Seq = 0x20716553,
        Resource_SEQp = 0x53455170,
        Resource_Pxtd = 0x50787464, // Added for relive path extension blocks
    };


    // == new res manager interface ==

    std::string FmvPath(const std::string& fmvName);

    // Starts loading anim on a worker thread. Whatever needs it asks the main loop to wait for
    // it with RequestLoadingWait, and uses it with LoadAnimation once that's done.
    void PendAnimation(AnimId anim, const std::string& theme = "");
    // Waits for anim if it's still being loaded by PendAnimation, or loads it right now if
    // nothing pended it
    AnimResource LoadAnimation(AnimId anim, const std::string& themeName = "");

    PalResource LoadPal(PalId pal);

    CamResource LoadCam(EReliveLevelIds lvlId, u32 pathNumber, u32 camNumber);
    Fg1Resource LoadFg1(EReliveLevelIds lvlId, u32 pathNumber, u32 camNumber);

    FontResource LoadFont(FontType fontId);

    std::vector<std::unique_ptr<BinaryPath>> LoadPaths(EReliveLevelIds lvlId);

    // Loads a VH/VB/SEQ sound file from the given theme's shared sounds/<soundTheme>/ dir (see
    // PathSoundInfo::mSoundTheme) - not the current level's own dir.
    std::vector<u8> LoadSoundFile(const char_type* pFileName, const std::string& soundTheme);

    // The vh_file/vb_file/seq_files a sounds/<soundTheme>/sound_info.json declares - every path
    // sharing a theme shares one of these too, so it's cached per theme name rather than
    // re-read/re-parsed for every path loaded (see LoadPaths).
    struct SoundThemeInfo final
    {
        std::string mVhFile;
        std::string mVbFile;
        std::vector<std::string> mSeqFiles;
    };
    const SoundThemeInfo& LoadSoundThemeInfo(const std::string& soundTheme);

    // Asks the main loop to wait for everything pended so far before it carries on (see
    // Engine::Game_Loop). Whatever needs those resources carries on from its next update, or
    // returns ScreenChangeResult::eWaiting for a screen change.
    void RequestLoadingWait(LoadingIcon icon = LoadingIcon::eIfSlow);
    bool LoadingWaitRequested() const
    {
        return mLoadingWaitRequested;
    }
    bool ShowLoadingIconNow() const
    {
        return mShowLoadingIconNow;
    }
    // Called by the main loop once nothing is loading any more
    void EndLoadingWait();

    // True while any pended resource is still loading
    bool IsLoading();

    // For testing slow storage: each resource loaded on a worker thread takes at least this long
    void SetDebugLoadDelay(u32 delayMs)
    {
        mDebugLoadDelayMs = delayMs;
    }

    // Thread-safe: records a resource that should always exist (an animation, etc) but
    // couldn't be found at any of the given locations, instead of raising a message box
    // immediately. PendAnimation's jobs run in parallel on ThreadPool worker threads, so
    // several failing around the same time would otherwise pop up one modal message box per
    // job, all at once - callers report here and a later FlushMissingResourceReports() call
    // (from the main thread) shows everything collected so far as a single dialog.
    void ReportMissingResource(std::string description, std::vector<std::string> searchedPaths);

    // Must be called from the main thread. Collates every ReportMissingResource() call made
    // since the last flush into one message box - each resource listed with every location
    // searched for it, in the order they were tried - then fatally aborts if anything was
    // recorded. EndLoadingWait already calls this once a batch of async loading has finished;
    // call it from elsewhere too (e.g. a VUpdate) if reports need to surface sooner.
    void FlushMissingResourceReports();

    // Stateless helper, no instance state is used
    static s32 SEQ_HashName(const char_type* seqFileName);


    template <typename T, int size>
    void PendAnims(const T (&anims)[size])
    {
        for (const auto& anim : anims)
        {
            if (anim != AnimId::None)
            {
                PendAnimation(anim);
            }
        }
    }
    
    std::vector<std::string> mSearchPaths;
private:

    struct AnimCache final
    {
        // TODO: Need to be weak_ptrs
        std::shared_ptr<AnimationAttributesAndFrames> mAnimAttributes;
        std::shared_ptr<PngData> mAnimPng;
        UniqueResId mAnimUniqueId;
    };

    bool Exists(AnimId animId, const std::string& theme);
    AnimCache LookUp(AnimId animId, const std::string& theme);

    void AddSearchPaths(const std::string& modPath);

    static constexpr const char* kSettingsIniPath = "relive.ini";

    struct MissingResourceReport final
    {
        std::string mDescription;
        std::vector<std::string> mSearchedPaths;
    };

    std::mutex mMissingResourcesMutex;
    std::vector<MissingResourceReport> mMissingResources;

public:
    std::mutex mLoadedAnimationsMutex;
    // TODO: Remove dead entries at some point

    using AnimCacheKey = std::pair<std::string, AnimId>;
    std::map<AnimCacheKey, AnimCache> mLoadedAnimations;

    // Pended animations still loading, guarded by mLoadedAnimationsMutex. mAnimationLoaded is
    // signalled each time one finishes (whether it was found or not).
    std::set<AnimCacheKey> mPendingAnimations;
    std::condition_variable mAnimationLoaded;
    u32 mDebugLoadDelayMs = 0;

    // FileSystem has no state, so sharing this reference across ThreadPool worker threads is safe.
    FileSystem& mFs;


private:
    // unique_ptr to avoid bringing the header in
    std::unique_ptr<ThreadPool> mThreadPool;

    bool mLoadingWaitRequested = false;
    bool mShowLoadingIconNow = false;

    std::map<std::string, SoundThemeInfo> mSoundThemeInfoCache;
};


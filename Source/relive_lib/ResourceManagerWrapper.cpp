#include "stdafx.h"
#include "ResourceManagerWrapper.hpp"
#include "GameType.hpp"
#include "Psx.hpp"

#include "data_conversion/file_system.hpp"

#include "data_conversion/data_conversion.hpp"
#include "data_conversion/AnimConversionInfo.hpp"
#include "data_conversion/PNGFile.hpp"
#include "data_conversion/AnimationConverter.hpp"

#include "BinaryPath.hpp"
#include "BaseGameAutoPlayer.hpp"
#include "FmvInfo.hpp"
#include "nlohmann/json.hpp"
#include "Sys.hpp"
#include "ThreadPool.hpp"
#include <FatalError.hpp>
#include <string>
#include <chrono>
#include <thread>
#include "IniFile.hpp"

std::atomic<u32> UniqueResId::mGlobalId{1};

ResourceManagerWrapper::ResourceManagerWrapper(FileSystem& fs, const std::string& modPath)
    : mFs(fs)
    , mThreadPool(std::make_unique<ThreadPool>())
{
    AddSearchPaths(modPath);
}

std::string ResourceManagerWrapper::LoadSettingsIniText()
{
    return mFs.LoadToString(kSettingsIniPath);
}

IniFile ResourceManagerWrapper::LoadSettingsIni()
{
    return IniFile::Parse(LoadSettingsIniText());
}

bool ResourceManagerWrapper::SaveSettingsIni(const IniFile& ini)
{
    return ini.Save(mFs, kSettingsIniPath);
}

std::string ResourceManagerWrapper::LoadDemoFile(const std::string& fileName)
{
    return mFs.LoadToString(fileName.c_str());
}

void ResourceManagerWrapper::AddSearchPaths(const std::string& modPath)
{
    // Root of all data
    FileSystem::Path reliveDataPath;
    reliveDataPath.Append("relive_data");

    // Where the base dir of the game type we are running is
    FileSystem::Path primaryBaseGamePath = reliveDataPath;

    // Where the base dir of the opposite game might be - we check here if primary fails
    FileSystem::Path backupBaseGamePath = reliveDataPath;
    if (GetGameType() == GameType::eAe)
    {
        primaryBaseGamePath.Append("ae");
        backupBaseGamePath.Append("ao");
    }
    else
    {
        primaryBaseGamePath.Append("ao");
        backupBaseGamePath.Append("ae");
    }

    if (!modPath.empty())
    {
        mSearchPaths.push_back(modPath);
    }

    mSearchPaths.push_back(primaryBaseGamePath.GetPath());
    mSearchPaths.push_back(backupBaseGamePath.GetPath());
}


// Out of line so unique_ptr<ThreadPool> can be destroyed with an incomplete ThreadPool type
ResourceManagerWrapper::~ResourceManagerWrapper() = default;

static std::string LvlDirName(EReliveLevelIds lvlId)
{
    if (GetGameType() == GameType::eAe)
    {
        return ToString(MapWrapper::ToAE(lvlId));
    }
    return ToString(MapWrapper::ToAO(lvlId));
}

static FileSystem::Path PerLvlBasePath(const std::string& basePath, EReliveLevelIds lvlId)
{
    FileSystem::Path filePath(basePath);
    filePath.Append("levels");
    filePath.Append(LvlDirName(lvlId));
    return filePath;
}

class AnimationLoaderJob final : public IJob
{
private:
    static std::string GetAnimPath(const std::string& basePath, AnimId animId, const std::string& themeName)
    {
        // One huge blocking func for now - needs to work like OG res man
        FileSystem::Path filePath(basePath);

        filePath.Append("animations");

        const char_type* groupName = AnimRecGroupName(animId);
        filePath.Append(groupName);

        if (!themeName.empty())
        {
            filePath.Append(themeName);
        }

        const char_type* animName = AnimRecName(animId);
        filePath.Append(animName);

        return filePath.GetPath();
    }

    std::string Describe() const
    {
        std::string desc = std::string("Animation \"") + AnimRecName(mAnimId) + "\"";
        if (!mThemeName.empty())
        {
            desc += " (theme \"" + mThemeName + "\")";
        }
        return desc;
    }

public:
    explicit AnimationLoaderJob(ResourceManagerWrapper* pResMan, AnimId anim, const std::string& themeName)
        : mResMan(pResMan), mAnimId(anim), mThemeName(themeName)
    {

    }

    void Execute() override
    {
        if (mResMan->mDebugLoadDelayMs)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(mResMan->mDebugLoadDelayMs));
        }

        Load();

        {
            std::unique_lock<std::mutex> lock(mResMan->mLoadingMutex);
            mResMan->mPendingAnimations.erase(std::make_pair(mThemeName, mAnimId));
        }
        mResMan->mResourceLoaded.notify_all();
    }

    void Load()
    {
        // One huge blocking func for now - needs to work like OG res man

        FileSystem& fs = mResMan->mFs;
        std::string jsonStr;
        std::string filePath;
        std::vector<std::string> searchedJsonPaths;
        for (const auto& basePath : mResMan->mSearchPaths)
        {
            filePath = GetAnimPath(basePath, mAnimId, mThemeName);
            const std::string jsonPath = filePath + ".json";
            searchedJsonPaths.push_back(jsonPath);
            jsonStr = fs.LoadToString(jsonPath.c_str());
            if (!jsonStr.empty())
            {
                break;
            }
        }

        // Not found under any search path - report it (rather than letting PNGFile::Load below
        // hard-abort the process, possibly from one of several ThreadPool worker threads at
        // once) and bail out without touching the (non-existent) png or parsing an empty json
        // string. LookUp() will keep reporting this animation as not loaded, same as
        // if this job had never run.
        if (jsonStr.empty())
        {
            mResMan->ReportMissingResource(Describe(), searchedJsonPaths);
            return;
        }

        const std::string pngPath = filePath + ".png";
        if (!fs.FileExists(pngPath.c_str()))
        {
            mResMan->ReportMissingResource(Describe(), {pngPath});
            return;
        }

        auto pPngData = std::make_shared<PngData>();
        PNGFile pngFile;
        pPngData->mPal = std::make_shared<AnimationPal>();
        pngFile.Load(fs, pngPath.c_str(), *pPngData->mPal, pPngData->mPixels, pPngData->mWidth, pPngData->mHeight);

        auto pAnimationAttributesAndFrames = std::make_shared<AnimationAttributesAndFrames>(jsonStr);

        AnimResource newRes;
        newRes.mId = mAnimId;
        newRes.mJsonPtr = pAnimationAttributesAndFrames;
        newRes.mPngPtr = pPngData;
        newRes.mCurPal = newRes.mPngPtr->mPal;

        std::unique_lock<std::mutex> lock(mResMan->mLoadingMutex);

        const auto key = std::make_pair(mThemeName, mAnimId);
        mResMan->mLoadedAnimations[key] = {pAnimationAttributesAndFrames, pPngData, {}};

        // Pinned while it loaded, keep it loaded
        auto pin = mResMan->mPinnedAnimations.find(key);
        if (pin != mResMan->mPinnedAnimations.end())
        {
            pin->second.mAnimAttributes = pAnimationAttributesAndFrames;
            pin->second.mAnimPng = pPngData;
        }
    }

private:
    ResourceManagerWrapper* mResMan = nullptr;
    AnimId mAnimId;
    std::string mThemeName;
};


inline void from_json(const nlohmann::json& j, Point32& p)
{
    j.at("x").get_to(p.x);
    j.at("y").get_to(p.y);
}

inline void from_json(const nlohmann::json& j, IndexedPoint& p)
{
    j.at("index").get_to(p.mIndex);
    j.at("point").get_to(p.mPoint);
}

inline void from_json(const nlohmann::json& j, PerFrameInfo& p)
{
    j.at("x_offset").get_to(p.mXOffset);
    j.at("y_offset").get_to(p.mYOffset);
    j.at("width").get_to(p.mWidth);
    j.at("height").get_to(p.mHeight);
    j.at("sprite_width").get_to(p.mSpriteWidth);
    j.at("sprite_height").get_to(p.mSpriteHeight);
    j.at("sprite_sheet_x").get_to(p.mSpriteSheetX);
    j.at("sprite_sheet_y").get_to(p.mSpriteSheetY);
    j.at("bound_max").get_to(p.mBoundMax);
    j.at("bound_min").get_to(p.mBoundMin);
    j.at("points_count").get_to(p.mPointCount);

    if (p.mPointCount > 0)
    {
        j.at("points").get_to(p.mPoints);
    }
}

inline void from_json(const nlohmann::json& j, AnimAttributes& p)
{
    j.at("frame_rate").get_to(p.mFrameRate);
    j.at("flip_x").get_to(p.mFlipX);
    j.at("flip_y").get_to(p.mFlipY);
    j.at("loop").get_to(p.mLoop);
    j.at("loop_start_frame").get_to(p.mLoopStartFrame);
    j.at("max_width").get_to(p.mMaxWidth);
    j.at("max_height").get_to(p.mMaxHeight);
}

AnimationAttributesAndFrames::AnimationAttributesAndFrames(const std::string& jsonData)
{
    nlohmann::json j = nlohmann::json::parse(jsonData);
    mFrames.resize(j["frames"].size());
    u32 i = 0;
    for (auto& frame : j["frames"])
    {
        from_json(frame, mFrames[i]);
        i++;
    }

    from_json(j["attributes"], mAttributes);
}

void ResourceManagerWrapper::PendAnimation(AnimId animId, const std::string& theme)
{
    {
        std::unique_lock<std::mutex> lock(mLoadingMutex);
        const AnimCacheKey key = std::make_pair(theme, animId);
        PinAnim(key);
        if (mActiveAnimPins)
        {
            mActiveAnimPins->push_back(key);
        }

        AnimResource res;
        if (LookUp(key, res) || !mPendingAnimations.insert(key).second)
        {
            // Already loaded or loading
            return;
        }
    }

    mThreadPool->AddJob(std::make_unique<AnimationLoaderJob>(this, animId, theme));
}

void ResourceManagerWrapper::PinAnim(const AnimCacheKey& key)
{
    AnimPin& pin = mPinnedAnimations[key];
    pin.mCount++;
    if (!pin.mAnimPng)
    {
        // Already loaded? Otherwise AnimationLoaderJob sets these once it is.
        AnimResource res;
        if (LookUp(key, res))
        {
            pin.mAnimAttributes = res.mJsonPtr;
            pin.mAnimPng = res.mPngPtr;
        }
    }
}

void ResourceManagerWrapper::BeginAnimPins(AnimPins& pins)
{
    mActiveAnimPins = &pins;
}

void ResourceManagerWrapper::EndAnimPins()
{
    mActiveAnimPins = nullptr;
}

void ResourceManagerWrapper::UnpinAnims(AnimPins& pins)
{
    std::unique_lock<std::mutex> lock(mLoadingMutex);
    for (const auto& key : pins)
    {
        auto pin = mPinnedAnimations.find(key);
        if (pin != mPinnedAnimations.end() && --pin->second.mCount == 0)
        {
            // Freed now unless something still uses it
            mPinnedAnimations.erase(pin);
        }
    }
    pins.clear();
}

void ResourceManagerWrapper::RequestLoadingWait(LoadingIcon icon)
{
    mLoadingWaitRequested = true;
    if (icon == LoadingIcon::eNow)
    {
        mShowLoadingIconNow = true;
    }
}

void ResourceManagerWrapper::EndLoadingWait()
{
    mLoadingWaitRequested = false;
    mShowLoadingIconNow = false;

    // This batch of async loading has fully finished (successfully or not), so this is the
    // natural place to surface anything PendAnimation's worker-thread jobs couldn't find.
    FlushMissingResourceReports();
}

bool ResourceManagerWrapper::IsLoading()
{
    std::unique_lock<std::mutex> lock(mLoadingMutex);
    return !mPendingAnimations.empty() || !mPendingCams.empty() || !mPendingFg1s.empty() || !mPendingLevelPaths.empty() || !mPendingSoundFiles.empty() || !mPendingFonts.empty();
}

std::string ResourceManagerWrapper::FmvPath(const std::string& fmvName)
{
    const std::string webmName = relive::FmvNameWithoutExtension(fmvName) + ".webm";
    for (const auto& basePath : mSearchPaths)
    {
        FileSystem::Path filePath(basePath);
        filePath.Append("fmvs");
        filePath.Append(webmName);
        if (mFs.FileExists(filePath.GetPath().c_str()))
        {
            return filePath.GetPath();
        }
    }
    return fmvName;
}

void ResourceManagerWrapper::AddAnimation(AnimId anim, std::shared_ptr<AnimationAttributesAndFrames> attributesAndFrames, std::shared_ptr<PngData> png)
{
    std::unique_lock<std::mutex> lock(mLoadingMutex);
    const AnimCacheKey key = std::make_pair(std::string(), anim);

    AnimCache& cached = mLoadedAnimations[key];
    cached.mAnimAttributes = attributesAndFrames;
    cached.mAnimPng = png;

    // Pinned, as nothing could load it again once it's freed
    AnimPin& pin = mPinnedAnimations[key];
    pin.mCount++;
    pin.mAnimAttributes = std::move(attributesAndFrames);
    pin.mAnimPng = std::move(png);
}

AnimResource ResourceManagerWrapper::LoadAnimation(AnimId anim, const std::string& themeName)
{
    const AnimCacheKey key = std::make_pair(themeName, anim);
    AnimResource res;
    {
        // Still loading on a worker thread, wait for it rather than loading it again
        std::unique_lock<std::mutex> lock(mLoadingMutex);
        mResourceLoaded.wait(lock, [&]() { return mPendingAnimations.count(key) == 0; });
        if (LookUp(key, res))
        {
            return res;
        }

        // TODO: Remove this when all of factory etc is updated (since it will always already be loaded here)
        if (static_cast<s32>(anim) <= 908) // ignore background animations for now
        {
            LOG_ERROR("Animation %d wasn't pended (or was freed) before calling LoadAnimation", static_cast<s32>(anim));
        }

        // Nothing pended it, so like PendAnimation with no pin list it stays loaded for the rest
        // of the game
        PinAnim(key);
        mPendingAnimations.insert(key);
    }

    AnimationLoaderJob hack(this, anim, themeName);
    hack.Execute();

    // hack.Execute() only records a report and returns if the resource is missing (see
    // AnimationLoaderJob::Execute) - unlike PendAnimation's async jobs, this can't wait for
    // the next loading wait to surface it, since the caller needs the animation right now, so
    // flush (and fatally abort, listing every location searched) immediately if it did.
    FlushMissingResourceReports();

    std::unique_lock<std::mutex> lock(mLoadingMutex);
    if (LookUp(key, res))
    {
        return res;
    }

    ALIVE_FATAL("Json or PNG resources have gone out of scope");
}

PalResource ResourceManagerWrapper::LoadPal(PalId pal)
{
    // TODO: Cache these
    PalResource newRes;
    newRes.mId = pal;
    newRes.mPal = std::make_shared<AnimationPal>();

    FileSystem::Path filePath;
    std::vector<std::string> searchedPaths;
    bool found = false;
    for (auto& basePath : mSearchPaths)
    {
        filePath = FileSystem::Path(basePath);
        filePath.Append(ToString(newRes.mId));
        searchedPaths.push_back(filePath.GetPath());
        if (mFs.FileExists(filePath.GetPath().c_str()))
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        // The caller needs the palette right now, so (like LoadAnimation) flush and fatally abort
        // immediately instead of waiting for the next loading wait.
        ReportMissingResource(std::string("Palette \"") + ToString(newRes.mId) + "\"", std::move(searchedPaths));
        FlushMissingResourceReports();
    }

    auto palData = mFs.LoadToVec(filePath.GetPath().c_str());
    if (palData.size() != 1024) // 256 RGBA entries
    {
        ALIVE_FATAL("Bad pal data size %d but expected 1024", palData.size());
    }

    memcpy(newRes.mPal->mPal, palData.data(), palData.size());

    return newRes;
}


static FileSystem::Path CamBaseName(const std::string& basePath, EReliveLevelIds lvlId, u32 pathNumber, u32 camNumber)
{
    // No separate "paths" subdir under the level - a level's content already *is* its paths.
    FileSystem::Path filePath = PerLvlBasePath(basePath, lvlId);
    filePath.Append(std::to_string(pathNumber));
    filePath.Append(std::to_string(camNumber));
    return filePath;
}

static RgbaData LoadPng(FileSystem& fs, const std::string& filePath)
{
    std::vector<u8> vec;
    unsigned int w = 0;
    unsigned int h = 0;
    PNGFile png;

    png.Load(fs, filePath.c_str(), vec, w, h);

    RgbaData data;
    data.mWidth = w;
    data.mHeight = h;
    data.mPixels = std::make_shared<std::vector<u8>>(std::move(vec));
    return data;
}

static std::string DescribeCam(EReliveLevelIds lvlId, u32 pathNumber, u32 camNumber)
{
    return "Camera " + std::to_string(camNumber) + " of path " + std::to_string(pathNumber) + " of level \"" + LvlDirName(lvlId) + "\"";
}

class CamLoaderJob final : public IJob
{
public:
    CamLoaderJob(ResourceManagerWrapper* pResMan, EReliveLevelIds lvlId, u32 pathNumber, u32 camNumber)
        : mResMan(pResMan), mLvlId(lvlId), mPathNumber(pathNumber), mCamNumber(camNumber)
    {

    }

    void Execute() override
    {
        if (mResMan->mDebugLoadDelayMs)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(mResMan->mDebugLoadDelayMs));
        }

        RgbaData data = Load();

        {
            // Not pending any more means FreeCam was called while it loaded, so drop it. Missing
            // cameras aren't cached, LoadCam reports them.
            std::unique_lock<std::mutex> lock(mResMan->mLoadingMutex);
            const auto key = std::make_tuple(mLvlId, mPathNumber, mCamNumber);
            if (mResMan->mPendingCams.erase(key) && data.mPixels)
            {
                mResMan->mLoadedCams[key].mData = std::move(data);
            }
        }
        mResMan->mResourceLoaded.notify_all();
    }

private:
    RgbaData Load()
    {
        std::vector<std::string> searchedPaths;
        for (const auto& basePath : mResMan->mSearchPaths)
        {
            const std::string pngPath = CamBaseName(basePath, mLvlId, mPathNumber, mCamNumber).GetPath() + ".png";
            searchedPaths.push_back(pngPath);
            if (mResMan->mFs.FileExists(pngPath.c_str()))
            {
                return LoadPng(mResMan->mFs, pngPath);
            }
        }

        mResMan->ReportMissingResource(DescribeCam(mLvlId, mPathNumber, mCamNumber), std::move(searchedPaths));
        return {};
    }

    ResourceManagerWrapper* mResMan = nullptr;
    EReliveLevelIds mLvlId;
    u32 mPathNumber = 0;
    u32 mCamNumber = 0;
};

void ResourceManagerWrapper::PendCam(EReliveLevelIds lvlId, u32 pathNumber, u32 camNumber)
{
    {
        std::unique_lock<std::mutex> lock(mLoadingMutex);
        const CamCacheKey key = std::make_tuple(lvlId, pathNumber, camNumber);
        if (mLoadedCams.count(key) || !mPendingCams.insert(key).second)
        {
            // Already loaded or loading
            return;
        }
    }

    mThreadPool->AddJob(std::make_unique<CamLoaderJob>(this, lvlId, pathNumber, camNumber));
}

CamResource ResourceManagerWrapper::LoadCam(EReliveLevelIds lvlId, u32 pathNumber, u32 camNumber)
{
    const CamCacheKey key = std::make_tuple(lvlId, pathNumber, camNumber);
    {
        // Still loading on a worker thread, wait for it rather than loading it again
        std::unique_lock<std::mutex> lock(mLoadingMutex);
        mResourceLoaded.wait(lock, [&]() { return mPendingCams.count(key) == 0; });
        auto it = mLoadedCams.find(key);
        if (it != mLoadedCams.end())
        {
            return it->second;
        }
    }

    // The job couldn't find it
    FlushMissingResourceReports();

    LOG_ERROR("Camera %d of path %d wasn't pended before calling LoadCam", camNumber, pathNumber);

    {
        // Pending so the job keeps what it loads
        std::unique_lock<std::mutex> lock(mLoadingMutex);
        mPendingCams.insert(key);
    }
    CamLoaderJob hack(this, lvlId, pathNumber, camNumber);
    hack.Execute();

    // The caller needs the camera right now, so (like LoadAnimation) flush and fatally abort
    // immediately instead of waiting for the next loading wait.
    FlushMissingResourceReports();

    std::unique_lock<std::mutex> lock(mLoadingMutex);
    return mLoadedCams[key];
}

void ResourceManagerWrapper::FreeCam(EReliveLevelIds lvlId, u32 pathNumber, u32 camNumber)
{
    // A camera still loading is dropped once it's loaded (see CamLoaderJob::Execute). If it's
    // pended again before then, a second job loads it and whichever finishes first is kept.
    std::unique_lock<std::mutex> lock(mLoadingMutex);
    const CamCacheKey key = std::make_tuple(lvlId, pathNumber, camNumber);
    mLoadedCams.erase(key);
    mPendingCams.erase(key);
}

class Fg1LoaderJob final : public IJob
{
public:
    Fg1LoaderJob(ResourceManagerWrapper* pResMan, EReliveLevelIds lvlId, u32 pathNumber, u32 camNumber)
        : mResMan(pResMan), mLvlId(lvlId), mPathNumber(pathNumber), mCamNumber(camNumber)
    {

    }

    void Execute() override
    {
        if (mResMan->mDebugLoadDelayMs)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(mResMan->mDebugLoadDelayMs));
        }

        Fg1Resource newRes = Load();

        {
            // Not pending any more means FreeFg1 was called while it loaded, so drop it
            std::unique_lock<std::mutex> lock(mResMan->mLoadingMutex);
            const auto key = std::make_tuple(mLvlId, mPathNumber, mCamNumber);
            if (mResMan->mPendingFg1s.erase(key))
            {
                mResMan->mLoadedFg1s[key] = std::move(newRes);
            }
        }
        mResMan->mResourceLoaded.notify_all();
    }

private:
    Fg1Resource Load()
    {
        FileSystem& fs = mResMan->mFs;
        Fg1Resource newRes;

        // Load the json manifest
        for (const auto& basePath : mResMan->mSearchPaths)
        {
            FileSystem::Path filePath = CamBaseName(basePath, mLvlId, mPathNumber, mCamNumber);
            const std::string jsonStr = fs.LoadToString((filePath.GetPath() + ".json").c_str());
            if (!jsonStr.empty())
            {
                nlohmann::json j = nlohmann::json::parse(jsonStr);
                newRes.mFg1ResBlockCount = j["fg1_block_count"];

                // TODO: Make this more sane later
                for (auto& fg1File : j["layers"])
                {
                    std::string s = fg1File;
                    if (s.find("fg_well") != std::string::npos)
                    {
                        newRes.mFgWell.mImage = LoadPng(fs, filePath.GetPath() + "fg_well.png");
                    }
                    else if (s.find("bg_well") != std::string::npos)
                    {
                        newRes.mBgWell.mImage = LoadPng(fs, filePath.GetPath() + "bg_well.png");
                    }
                    else if (s.find("fg") != std::string::npos)
                    {
                        newRes.mFg.mImage = LoadPng(fs, filePath.GetPath() + "fg.png");
                    }
                    else if (s.find("bg") != std::string::npos)
                    {
                        newRes.mBg.mImage = LoadPng(fs, filePath.GetPath() + "bg.png");
                    }
                }
                break;
            }
        }
        return newRes;
    }

    ResourceManagerWrapper* mResMan = nullptr;
    EReliveLevelIds mLvlId;
    u32 mPathNumber = 0;
    u32 mCamNumber = 0;
};

void ResourceManagerWrapper::PendFg1(EReliveLevelIds lvlId, u32 pathNumber, u32 camNumber)
{
    {
        std::unique_lock<std::mutex> lock(mLoadingMutex);
        const CamCacheKey key = std::make_tuple(lvlId, pathNumber, camNumber);
        if (mLoadedFg1s.count(key) || !mPendingFg1s.insert(key).second)
        {
            // Already loaded or loading
            return;
        }
    }

    mThreadPool->AddJob(std::make_unique<Fg1LoaderJob>(this, lvlId, pathNumber, camNumber));
}

Fg1Resource ResourceManagerWrapper::LoadFg1(EReliveLevelIds lvlId, u32 pathNumber, u32 camNumber)
{
    const CamCacheKey key = std::make_tuple(lvlId, pathNumber, camNumber);
    {
        // Still loading on a worker thread, wait for it rather than loading it again
        std::unique_lock<std::mutex> lock(mLoadingMutex);
        mResourceLoaded.wait(lock, [&]() { return mPendingFg1s.count(key) == 0; });
        auto it = mLoadedFg1s.find(key);
        if (it != mLoadedFg1s.end())
        {
            return it->second;
        }
    }

    LOG_ERROR("FG1 of camera %d of path %d wasn't pended before calling LoadFg1", camNumber, pathNumber);

    {
        // Pending so the job keeps what it loads
        std::unique_lock<std::mutex> lock(mLoadingMutex);
        mPendingFg1s.insert(key);
    }
    Fg1LoaderJob hack(this, lvlId, pathNumber, camNumber);
    hack.Execute();

    std::unique_lock<std::mutex> lock(mLoadingMutex);
    return mLoadedFg1s[key];
}

void ResourceManagerWrapper::FreeFg1(EReliveLevelIds lvlId, u32 pathNumber, u32 camNumber)
{
    // Like FreeCam
    std::unique_lock<std::mutex> lock(mLoadingMutex);
    const CamCacheKey key = std::make_tuple(lvlId, pathNumber, camNumber);
    mLoadedFg1s.erase(key);
    mPendingFg1s.erase(key);
}

static std::string FontName(FontType fontId)
{
    switch (fontId)
    {
        case FontType::LcdFont:
            return "lcd_font";

        case FontType::PauseMenu:
            return "pause_menu_font";

        default:
            ALIVE_FATAL("Can't load font type %d", static_cast<s32>(fontId));
    }
}

class FontLoaderJob final : public IJob
{
public:
    FontLoaderJob(ResourceManagerWrapper* pResMan, FontType fontId)
        : mResMan(pResMan), mFontId(fontId)
    {

    }

    void Execute() override
    {
        if (mResMan->mDebugLoadDelayMs)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(mResMan->mDebugLoadDelayMs));
        }

        std::shared_ptr<PngData> png = Load();

        {
            // Missing fonts aren't cached, LoadFont reports them
            std::unique_lock<std::mutex> lock(mResMan->mLoadingMutex);
            if (png)
            {
                mResMan->mLoadedFonts[mFontId] = std::move(png);
            }
            mResMan->mPendingFonts.erase(mFontId);
        }
        mResMan->mResourceLoaded.notify_all();
    }

private:
    std::shared_ptr<PngData> Load()
    {
        FileSystem& fs = mResMan->mFs;
        const std::string fontName = FontName(mFontId);

        auto pPngData = std::make_shared<PngData>();
        pPngData->mPal = std::make_shared<AnimationPal>();
        std::vector<std::string> searchedPaths;
        for (const auto& basePath : mResMan->mSearchPaths)
        {
            FileSystem::Path filePath(basePath);
            filePath.Append(fontName);

            const std::string pngPath = filePath.GetPath() + ".png";
            searchedPaths.push_back(pngPath);
            if (!fs.FileExists(pngPath.c_str()))
            {
                continue;
            }

            PNGFile pngFile;
            pngFile.Load(fs, pngPath.c_str(), *pPngData->mPal, pPngData->mPixels, pPngData->mWidth, pPngData->mHeight);
            if (!pPngData->mPixels.empty())
            {
                return pPngData;
            }
        }

        mResMan->ReportMissingResource("Font \"" + fontName + "\"", std::move(searchedPaths));
        return nullptr;
    }

    ResourceManagerWrapper* mResMan = nullptr;
    FontType mFontId;
};

void ResourceManagerWrapper::PendFont(FontType fontId)
{
    {
        std::unique_lock<std::mutex> lock(mLoadingMutex);
        if (mLoadedFonts.count(fontId) || !mPendingFonts.insert(fontId).second)
        {
            // Already loaded or loading
            return;
        }
    }

    mThreadPool->AddJob(std::make_unique<FontLoaderJob>(this, fontId));
}

FontResource ResourceManagerWrapper::LoadFont(FontType fontId)
{
    bool pended = false;
    {
        // Still loading on a worker thread, wait for it rather than loading it again
        std::unique_lock<std::mutex> lock(mLoadingMutex);
        pended = mPendingFonts.count(fontId) != 0;
        mResourceLoaded.wait(lock, [&]() { return mPendingFonts.count(fontId) == 0; });
        pended = pended || mLoadedFonts.count(fontId) != 0;
    }

    if (!pended)
    {
        LOG_ERROR("Font %d wasn't pended before calling LoadFont", static_cast<s32>(fontId));
        {
            std::unique_lock<std::mutex> lock(mLoadingMutex);
            mPendingFonts.insert(fontId);
        }
        FontLoaderJob hack(this, fontId);
        hack.Execute();
    }

    {
        // A copy, callers used to get their own image and palette from each load
        std::unique_lock<std::mutex> lock(mLoadingMutex);
        auto it = mLoadedFonts.find(fontId);
        if (it != mLoadedFonts.end())
        {
            auto pPngData = std::make_shared<PngData>(*it->second);
            pPngData->mPal = std::make_shared<AnimationPal>(*it->second->mPal);
            return FontResource(fontId, pPngData);
        }
    }

    // The caller needs the font right now, so (like LoadAnimation) flush and fatally abort
    // immediately instead of waiting for the next loading wait.
    FlushMissingResourceReports();
    return {};
}

class PathsLoaderJob final : public IJob
{
public:
    PathsLoaderJob(ResourceManagerWrapper* pResMan, EReliveLevelIds lvlId)
        : mResMan(pResMan), mLvlId(lvlId)
    {

    }

    void Execute() override
    {
        if (mResMan->mDebugLoadDelayMs)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(mResMan->mDebugLoadDelayMs));
        }

        auto paths = Load();

        {
            std::unique_lock<std::mutex> lock(mResMan->mLoadingMutex);
            mResMan->mLoadedLevelPaths[mLvlId] = std::move(paths);
            mResMan->mPendingLevelPaths.erase(mLvlId);
        }
        mResMan->mResourceLoaded.notify_all();
    }

private:
    std::vector<std::unique_ptr<BinaryPath>> Load()
    {
        std::vector<std::unique_ptr<BinaryPath>> ret;
        std::vector<std::string> searchedLevelInfoPaths;
        bool foundLevelInfo = false;

        for (const auto& basePath : mResMan->mSearchPaths)
        {
            // TODO: Load level_info.json so we know which path jsons to load for this level
            FileSystem::Path pathDir = PerLvlBasePath(basePath, mLvlId);

            FileSystem::Path levelInfo = pathDir;
            levelInfo.Append("level_info.json");
            searchedLevelInfoPaths.push_back(levelInfo.GetPath());

            const std::string jsonStr = mResMan->mFs.LoadToString(levelInfo);
            if (!jsonStr.empty())
            {
                nlohmann::json j = nlohmann::json::parse(jsonStr);
                const auto& paths = j["paths"];
                for (const auto& path : paths)
                {
                    const std::string pathId = path["path_id"];

                    FileSystem::Path pathJsonFile = pathDir;
                    pathJsonFile.Append(pathId).Append("path.json");
                    const std::string pathJsonStr = mResMan->mFs.LoadToString(pathJsonFile);

                    // level_info.json listed this path so it should exist, and it can only live next
                    // to that level_info.json - there is no other search path to fall back to.
                    if (pathJsonStr.empty())
                    {
                        mResMan->ReportMissingResource("Path " + pathId + " of level \"" + LvlDirName(mLvlId) + "\"", {pathJsonFile.GetPath()});
                        continue;
                    }

                    // TODO: set the res ptrs to the parsed json data
                    // TODO: Handle exception on bad data

                    nlohmann::json pathJson = nlohmann::json::parse(pathJsonStr);
                    LOG_INFO("Cam count %d", pathJson["map"]["cameras"].size());

                    auto pathBuffer = std::make_unique<BinaryPath>(pathJsonFile.GetPath(), pathJson["map"]["path_id"]);
                    pathBuffer->CreateFromJson(pathJson);

                    PathSoundInfo& soundInfo = *pathBuffer->GetSoundInfo();
                    const ResourceManagerWrapper::SoundThemeInfo& themeInfo = mResMan->LoadSoundThemeInfo(soundInfo.mSoundTheme);
                    soundInfo.mVhFile = themeInfo.mVhFile;
                    soundInfo.mVbFile = themeInfo.mVbFile;
                    soundInfo.mSeqFiles = themeInfo.mSeqFiles;

                    ret.emplace_back(std::move(pathBuffer));
                }
                foundLevelInfo = true;
                break;
            }
        }

        if (!foundLevelInfo)
        {
            mResMan->ReportMissingResource("Level info of level \"" + LvlDirName(mLvlId) + "\"", std::move(searchedLevelInfoPaths));
        }

        return ret;
    }

    ResourceManagerWrapper* mResMan = nullptr;
    EReliveLevelIds mLvlId;
};

void ResourceManagerWrapper::PendPaths(EReliveLevelIds lvlId)
{
    {
        std::unique_lock<std::mutex> lock(mLoadingMutex);
        if (mLoadedLevelPaths.count(lvlId) || !mPendingLevelPaths.insert(lvlId).second)
        {
            // Already loaded or loading
            return;
        }
    }

    mThreadPool->AddJob(std::make_unique<PathsLoaderJob>(this, lvlId));
}

std::vector<std::unique_ptr<BinaryPath>> ResourceManagerWrapper::LoadPaths(EReliveLevelIds lvlId)
{
    {
        // Still loading on a worker thread, wait for it rather than loading it again
        std::unique_lock<std::mutex> lock(mLoadingMutex);
        mResourceLoaded.wait(lock, [&]() { return mPendingLevelPaths.count(lvlId) == 0; });
        auto it = mLoadedLevelPaths.find(lvlId);
        if (it != mLoadedLevelPaths.end())
        {
            auto paths = std::move(it->second);
            mLoadedLevelPaths.erase(it);
            return paths;
        }
    }

    LOG_ERROR("Paths of level %d weren't pended before calling LoadPaths", static_cast<s32>(lvlId));
    PathsLoaderJob hack(this, lvlId);
    hack.Execute();

    std::unique_lock<std::mutex> lock(mLoadingMutex);
    auto paths = std::move(mLoadedLevelPaths[lvlId]);
    mLoadedLevelPaths.erase(lvlId);
    return paths;
}

// Doesn't report anything itself - callers decide whether an empty result is fatal right now
// (LoadSoundFile) or just gets recorded for later (LoadSoundThemeInfo, see there).
static std::vector<u8> FindSoundFile(FileSystem& fs, const std::vector<std::string>& searchPaths, const char_type* pFileName, const std::string& soundTheme, std::vector<std::string>& searchedPaths)
{
    for (const auto& basePath : searchPaths)
    {
        FileSystem::Path soundFilePath(basePath);
        soundFilePath.Append("sounds").Append(soundTheme).Append(pFileName);
        searchedPaths.push_back(soundFilePath.GetPath());
        auto vec = fs.LoadToVec(soundFilePath.GetPath().c_str());
        if (!vec.empty())
        {
            return vec;
        }
    }
    return {};
}

static std::string DescribeSoundFile(const std::string& fileName, const std::string& soundTheme)
{
    return "Sound file \"" + fileName + "\" of sound theme \"" + soundTheme + "\"";
}

class SoundFileLoaderJob final : public IJob
{
public:
    SoundFileLoaderJob(ResourceManagerWrapper* pResMan, const std::string& fileName, const std::string& soundTheme)
        : mResMan(pResMan), mFileName(fileName), mSoundTheme(soundTheme)
    {

    }

    void Execute() override
    {
        if (mResMan->mDebugLoadDelayMs)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(mResMan->mDebugLoadDelayMs));
        }

        std::vector<std::string> searchedPaths;
        std::vector<u8> data = FindSoundFile(mResMan->mFs, mResMan->mSearchPaths, mFileName.c_str(), mSoundTheme, searchedPaths);
        if (data.empty())
        {
            mResMan->ReportMissingResource(DescribeSoundFile(mFileName, mSoundTheme), std::move(searchedPaths));
        }

        {
            // Missing files aren't cached, LoadSoundFile reports them
            std::unique_lock<std::mutex> lock(mResMan->mLoadingMutex);
            const auto key = std::make_pair(mSoundTheme, mFileName);
            if (!data.empty())
            {
                mResMan->mLoadedSoundFiles[key] = std::move(data);
            }
            mResMan->mPendingSoundFiles.erase(key);
        }
        mResMan->mResourceLoaded.notify_all();
    }

private:
    ResourceManagerWrapper* mResMan = nullptr;
    std::string mFileName;
    std::string mSoundTheme;
};

void ResourceManagerWrapper::PendSoundFile(const std::string& fileName, const std::string& soundTheme)
{
    {
        std::unique_lock<std::mutex> lock(mLoadingMutex);
        const SoundFileKey key = std::make_pair(soundTheme, fileName);
        if (mLoadedSoundFiles.count(key) || !mPendingSoundFiles.insert(key).second)
        {
            // Already loaded or loading
            return;
        }
    }

    mThreadPool->AddJob(std::make_unique<SoundFileLoaderJob>(this, fileName, soundTheme));
}

std::vector<u8> ResourceManagerWrapper::LoadSoundFile(const std::string& fileName, const std::string& soundTheme)
{
    const SoundFileKey key = std::make_pair(soundTheme, fileName);
    bool pended = false;
    {
        // Still loading on a worker thread, wait for it rather than loading it again
        std::unique_lock<std::mutex> lock(mLoadingMutex);
        pended = mPendingSoundFiles.count(key) != 0;
        mResourceLoaded.wait(lock, [&]() { return mPendingSoundFiles.count(key) == 0; });
        auto it = mLoadedSoundFiles.find(key);
        if (it != mLoadedSoundFiles.end())
        {
            std::vector<u8> data = std::move(it->second);
            mLoadedSoundFiles.erase(it);
            return data;
        }
    }

    if (!pended)
    {
        LOG_ERROR("Sound file %s wasn't pended before calling LoadSoundFile", fileName.c_str());
        SoundFileLoaderJob hack(this, fileName, soundTheme);
        hack.Execute();

        std::unique_lock<std::mutex> lock(mLoadingMutex);
        auto it = mLoadedSoundFiles.find(key);
        if (it != mLoadedSoundFiles.end())
        {
            std::vector<u8> data = std::move(it->second);
            mLoadedSoundFiles.erase(it);
            return data;
        }
    }

    // Missing. The callers (VH/VB/SEQ loading) dereference the data straight away, so like
    // LoadAnimation flush and fatally abort immediately instead of waiting for the next
    // loading wait.
    FlushMissingResourceReports();
    return {};
}

const ResourceManagerWrapper::SoundThemeInfo& ResourceManagerWrapper::LoadSoundThemeInfo(const std::string& soundTheme)
{
    // References to std::map entries stay valid after the lock is released
    std::unique_lock<std::mutex> lock(mSoundThemeInfoMutex);
    const auto existing = mSoundThemeInfoCache.find(soundTheme);
    if (existing != mSoundThemeInfoCache.end())
    {
        return existing->second;
    }

    SoundThemeInfo info;
    std::vector<std::string> searchedPaths;
    const std::vector<u8> bytes = FindSoundFile(mFs, mSearchPaths, "sound_info.json", soundTheme, searchedPaths);
    if (!bytes.empty())
    {
        const nlohmann::json j = nlohmann::json::parse(bytes.begin(), bytes.end());
        j.at("vh_file").get_to(info.mVhFile);
        j.at("vb_file").get_to(info.mVbFile);
        j.at("seq_files").get_to(info.mSeqFiles);
    }
    else
    {
        LOG_ERROR("Missing sound_info.json for sound theme '%s'", soundTheme.c_str());

        // Report only, no flush: our caller (PendPaths' job) runs on a worker thread, where a
        // modal + abort isn't safe - the game's callers flush right after LoadPaths on the main
        // thread.
        ReportMissingResource("Sound info of sound theme \"" + soundTheme + "\"", std::move(searchedPaths));
    }

    return mSoundThemeInfoCache.emplace(soundTheme, std::move(info)).first->second;
}


void ResourceManagerWrapper::ReportMissingResource(std::string description, std::vector<std::string> searchedPaths)
{
    std::unique_lock<std::mutex> lock(mMissingResourcesMutex);
    mMissingResources.push_back({std::move(description), std::move(searchedPaths)});
}

void ResourceManagerWrapper::FlushMissingResourceReports()
{
    std::vector<MissingResourceReport> reports;
    {
        std::unique_lock<std::mutex> lock(mMissingResourcesMutex);
        if (mMissingResources.empty())
        {
            return;
        }
        reports = std::move(mMissingResources);
        mMissingResources.clear();
    }

    std::string message = "The following resources should exist but could not be found:\n";
    for (const auto& report : reports)
    {
        message += "\n" + report.mDescription + "\nSearched (in order):\n";
        for (const auto& path : report.mSearchedPaths)
        {
            message += "  " + path + "\n";
        }
    }

    // A batch listing every location searched for several missing resources can easily be
    // longer than ALIVE_FATAL's stack buffer - it falls back to a heap one sized to fit rather
    // than truncating, so passing the whole message through here is safe.
    ALIVE_FATAL("%s", message.c_str());
}


s32 ResourceManagerWrapper::SEQ_HashName(const char_type* seqFileName)
{
    // Clamp max len
    size_t seqFileNameLength = strlen(seqFileName) - 1;
    if (seqFileNameLength > 8)
    {
        seqFileNameLength = 8;
    }

    // Iterate each s8 to calculate hash
    u32 hashId = 0;
    for (size_t index = 0; index < seqFileNameLength; index++)
    {
        char_type letter = seqFileName[index];
        if (letter == '.')
        {
            break;
        }

        const u32 temp = 10 * hashId;
        if (letter < '0' || letter > '9')
        {
            if (letter >= 'a')
            {
                if (letter <= 'z')
                {
                    letter -= ' ';
                }
            }
            hashId = letter % 10 + temp;
        }
        else
        {
            hashId = index || letter != '0' ? temp + letter - '0' : temp + 9;
        }
    }
    return hashId;
}

bool ResourceManagerWrapper::LookUp(const AnimCacheKey& key, AnimResource& res)
{
    auto it = mLoadedAnimations.find(key);
    if (it == std::end(mLoadedAnimations))
    {
        return false;
    }

    auto jsonPtr = it->second.mAnimAttributes.lock();
    auto pngPtr = it->second.mAnimPng.lock();
    if (!jsonPtr || !pngPtr)
    {
        // Freed
        mLoadedAnimations.erase(it);
        return false;
    }

    res = AnimResource(key.second, jsonPtr, pngPtr);
    res.mUniqueId = it->second.mAnimUniqueId;
    return true;
}

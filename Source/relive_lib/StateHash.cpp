#include "StateHash.hpp"
#include "BaseMap.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#if defined(__SANITIZE_ADDRESS__)
// From sanitizer/allocator_interface.h, which not every compiler ships
extern "C" std::size_t __sanitizer_get_allocated_size(const volatile void* p);
#endif
#include <map>
#include <sstream>
#include <typeinfo>
#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#include <cstdlib>
#endif
#include "SerializedObjectData.hpp"
#include "WorldStatic.hpp"
#include "Events.hpp"
#include "SwitchStates.hpp"
#include "GameObjects/BaseAliveGameObject.hpp"
#include "GameObjects/PlatformBase.hpp"
#include "Animation.hpp"
#include "BinaryPath.hpp"
#include "data_conversion/relive_tlvs.hpp"
#include "GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"

extern u32 sGnFrame;
extern u8 sRandomSeed; // Math.cpp

// FNV-1a, 64 bit
class Hasher final
{
public:
    void Bytes(const void* pData, std::size_t size)
    {
        const u8* p = static_cast<const u8*>(pData);
        for (std::size_t i = 0; i < size; i++)
        {
            mValue = (mValue ^ p[i]) * 0x100000001b3ull;
        }
    }

    template <typename T>
    void Add(const T& value)
    {
        Bytes(&value, sizeof(T));
    }

    void Add(const FP& value)
    {
        Add(value.fpValue);
    }

    u64 Value() const
    {
        return mValue;
    }

private:
    u64 mValue = 0xcbf29ce484222325ull;
};

// Where the process has memory that isn't from a file (heap, stacks, bss...), so a word holding an address of
// something that is somewhere else in another world can be told from a value. Addresses in the executable's own
// tables and code (vtables, animation callbacks) are the same in every world, so they count as values.
// Empty where that can't be read.
class MappedRanges final
{
public:
    MappedRanges()
    {
        FILE* pFile = fopen("/proc/self/maps", "r");
        if (!pFile)
        {
            return;
        }
        char line[512] = {};
        while (fgets(line, sizeof(line), pFile))
        {
            unsigned long long start = 0;
            unsigned long long end = 0;
            char perms[8] = {};
            unsigned long long offset = 0;
            char device[16] = {};
            unsigned long long inode = 0;
            int pathStart = 0;
            if (sscanf(line, "%llx-%llx %7s %llx %15s %llu %n", &start, &end, perms, &offset, device, &inode, &pathStart) >= 6)
            {
                // No path is anonymous memory; [heap] and [stack] are named
                const char* path = line + pathStart;
                const bool anonymous = *path == '\0' || *path == '\n' || strncmp(path, "[heap]", 6) == 0 || strncmp(path, "[stack", 6) == 0;
                if (anonymous)
                {
                    mRanges.push_back({start, end});
                }
                else if (*path != '\0' && *path != '\n' && *path != '[')
                {
                    // The first mapping of a file is where it was loaded, which ASLR moves from run to run
                    std::string file(path);
                    while (!file.empty() && file.back() == '\n')
                    {
                        file.pop_back();
                    }
                    auto it = mFileBases.find(file);
                    if (it == mFileBases.end())
                    {
                        it = mFileBases.emplace(file, start).first;
                    }
                    mFileRanges.push_back({start, end, it->second});
                }
            }
        }
        fclose(pFile);
    }

    bool Contains(u64 value) const
    {
        for (const Range& range : mRanges)
        {
            if (value >= range.mStart && value < range.mEnd)
            {
                return true;
            }
        }
        return false;
    }

    // A word that is an address in the executable or a library, as the offset from where that file is loaded: the
    // same in every run, where the address itself isn't
    u64 Normalize(u64 value) const
    {
        for (const FileRange& range : mFileRanges)
        {
            if (value >= range.mStart && value < range.mEnd)
            {
                return value - range.mBase;
            }
        }
        return value;
    }

    bool Empty() const
    {
        return mRanges.empty();
    }

private:
    struct Range final
    {
        u64 mStart;
        u64 mEnd;
    };
    std::vector<Range> mRanges;
    struct FileRange final
    {
        u64 mStart;
        u64 mEnd;
        u64 mBase;
    };
    std::vector<FileRange> mFileRanges;
    std::map<std::string, u64> mFileBases;
};

// Not game state, so left out of comparing worlds:
// - ScreenManager, LCDScreen, LCDStatusBoard, FG1, CameraSwapper hold renderer handles (texture and cache ids)
//   that depend on how far the background loading had got, so two runs of the same game differ. They draw, and
//   keep no game state of their own that a save lacks.
// - MusicController works out what to play from the wall clock (SysGetTicks), and nothing in the game reads
//   it back: it is sound output.
static bool IsRenderOrSoundOnlyClass(const std::string& className)
{
    static const char* const kRenderOnly[] = {"ScreenManager", "LCDScreen", "LCDStatusBoard", "FG1", "CameraSwapper", "MusicController"};
    // AO's have the namespace in front
    const std::string name = className.rfind("AO::", 0) == 0 ? className.substr(4) : className;
    for (const char* renderOnly : kRenderOnly)
    {
        if (name == renderOnly)
        {
            return true;
        }
    }
    return false;
}

// The object's own memory, word by word, as a hash: this is what finds a timer or a counter that no save and no
// accessor shows. Words that are addresses are left out, as the same object is somewhere else in another world,
// and what they point at isn't looked at (ids are what link objects). Needs the allocator to say how big the
// object is, so only where AddressSanitizer is on, which is every Linux Debug build; otherwise there's no hash.
// pDump, if given, gets the words as hex ("?" for an address, which is left out).
static u64 HashRawMemory(const BaseGameObject& obj, const MappedRanges& mapped, std::string* pDump)
{
#if defined(__SANITIZE_ADDRESS__)
    if (mapped.Empty())
    {
        return 0;
    }

    const std::string className = StateHash::ClassName(obj);
    if (IsRenderOrSoundOnlyClass(className))
    {
        return 0;
    }

    const std::size_t size = __sanitizer_get_allocated_size(&obj);
    // What an animation drew with last (its poly) is output, its resource is handles and caches made when it is
    // drawn, and its colour is set as it is drawn: none is state (which animation it is, is hashed with the
    // object's fields). Every animation the object holds is looked for in the list of animations.
    struct Skip final
    {
        std::size_t mStart;
        std::size_t mEnd;
    };
    std::vector<Skip> skips;
    const u8* pObjStart = reinterpret_cast<const u8*>(&obj);
    for (s32 i = 0; i < AnimationBase::gAnimations->Size(); i++)
    {
        AnimationBase* pAnim = AnimationBase::gAnimations->ItemAt(i);
        if (!pAnim)
        {
            break;
        }
        const u8* pAnimBytes = reinterpret_cast<const u8*>(pAnim);
        if (pAnimBytes < pObjStart || pAnimBytes >= pObjStart + size)
        {
            continue;
        }
        auto* pAnimation = dynamic_cast<Animation*>(pAnim);
        if (!pAnimation)
        {
            continue;
        }
        for (const Animation::Memory& memory : pAnimation->RenderOnlyMemory())
        {
            const std::size_t start = reinterpret_cast<const u8*>(memory.mStart) - pObjStart;
            skips.push_back({start, start + memory.mSize});
        }
    }

    // The animation an object has of its own, which may be out of the list of animations (it is looked for there above)
    if (obj.GetIsBaseAnimatedWithPhysicsObj())
    {
        Animation& ownAnimation = static_cast<BaseAnimatedWithPhysicsGameObject&>(const_cast<BaseGameObject&>(obj)).GetAnimation();
        for (const Animation::Memory& memory : ownAnimation.RenderOnlyMemory())
        {
            const std::size_t start = reinterpret_cast<const u8*>(memory.mStart) - pObjStart;
            skips.push_back({start, start + memory.mSize});
        }
    }

    for (const BaseGameObject::NotStateMemory& memory : const_cast<BaseGameObject&>(obj).VNotStateMemory())
    {
        const std::size_t start = reinterpret_cast<const u8*>(memory.mStart) - pObjStart;
        skips.push_back({start, start + memory.mSize});
    }

    Hasher h;
    char hex[20] = {};
    for (std::size_t offset = 0; offset + sizeof(u64) <= size; offset += sizeof(u64))
    {
        bool skipped = false;
        for (const Skip& skip : skips)
        {
            skipped = skipped || (offset + sizeof(u64) > skip.mStart && offset < skip.mEnd);
        }
        if (skipped)
        {
            if (pDump)
            {
                *pDump += "_ ";
            }
            continue;
        }
        u64 word = 0;
        std::memcpy(&word, reinterpret_cast<const u8*>(&obj) + offset, sizeof(u64));
        // Aligned, as an address is: three colour values side by side can look like one in the huge ranges the sanitizer reserves
        // (and above the low ranges the sanitizer reserves, where two small values side by side look like one: a real
        // address, of the heap, the stack and the executable, is far above them)
        const bool isAddress = word >= (1ull << 44) && (word & 3) == 0 && mapped.Contains(word);
        if (isAddress)
        {
            word = 0;
        }
        else
        {
            word = mapped.Normalize(word);
        }
        if (pDump)
        {
            // What is hashed, so that a dump of the same state is the same text (a vtable is an address of the binary)
            if (isAddress)
            {
                *pDump += "? ";
            }
            else
            {
                snprintf(hex, sizeof(hex), "%llx ", static_cast<unsigned long long>(word));
                *pDump += hex;
            }
        }
        h.Add(word);
    }
    return h.Value();
#else
    (void) obj;
    (void) mapped;
    (void) pDump;
    return 0;
#endif
}

static s64 ToS64(const FP& value)
{
    return value.fpValue;
}

template <typename T>
static s64 ToS64(const T& value)
{
    return static_cast<s64>(value);
}

// saveStateHash: what the object writes for a quicksave, kept apart to tell a field that
// differs from a save that does. pText, if given, gets the fields and the save as text.
static u64 HashObject(BaseGameObject& obj, u64& saveStateHash, std::string* pText, std::string* pSaveText)
{
    Hasher h;
    saveStateHash = 0;

    auto add = [&](const char* name, const auto& value)
    {
        h.Add(value);
        if (pText)
        {
            *pText += std::string(name) + "=" + std::to_string(ToS64(value)) + " ";
        }
    };

    add("type", static_cast<s32>(obj.Type()));
    add("dead", obj.GetDead());
    add("updatable", obj.GetUpdatable());
    add("update_delay", obj.UpdateDelay());

    if (obj.GetIsBaseAnimatedWithPhysicsObj())
    {
        auto& anim = static_cast<BaseAnimatedWithPhysicsGameObject&>(obj);
        add("x", anim.mXPos);
        add("y", anim.mYPos);
        add("vel_x", anim.mVelX);
        add("vel_y", anim.mVelY);
        add("path", anim.mCurrentPath);
        add("level", static_cast<s32>(anim.mCurrentLevel));
        add("sprite_scale", anim.GetSpriteScale());
        add("frame", anim.GetAnimation().GetCurrentFrame());
        add("frame_change_counter", anim.GetAnimation().GetFrameChangeCounter());
        add("frame_delay", anim.GetAnimation().GetFrameDelay());
        add("flip_x", anim.GetAnimation().GetFlipX());
        add("render", anim.GetAnimation().GetRender());
        add("animate", anim.GetAnimation().GetAnimate());
        add("loop", anim.GetAnimation().GetLoop());
        add("is_last_frame", anim.GetAnimation().GetIsLastFrame());
        add("forward_loop_completed", anim.GetAnimation().GetForwardLoopCompleted());
        add("anim_id", static_cast<s32>(anim.GetAnimation().mAnimRes.mId));
    }

    if (obj.GetIsBaseAliveGameObject())
    {
        auto& alive = static_cast<BaseAliveGameObject&>(obj);
        add("health", alive.mHealth);
        add("last_line_y", alive.BaseAliveGameObjectLastLineYPos);
        add("previous_motion", alive.VGetMotion(eMotionType::ePreviousMotion));
        add("motion", alive.VGetMotion(eMotionType::eCurrentMotion));
        add("next_motion", alive.VGetMotion(eMotionType::eNextMotion));
    }

    if (!obj.GetDead())
    {
        SerializedObjectData saved;
        Animation::SetSaveStateWithoutColour(true);
        obj.SaveState(saved);
        Animation::SetSaveStateWithoutColour(false);
        Hasher saveHasher;
        saveHasher.Bytes(saved.Data(), saved.Size());
        saveStateHash = saveHasher.Value();
        if (pSaveText)
        {
            char hex[3] = {};
            for (std::size_t k = 0; k < saved.Size(); k++)
            {
                snprintf(hex, sizeof(hex), "%02x", saved.Data()[k]);
                *pSaveText += hex;
            }
        }
    }
    return h.Value();
}

std::string StateHash::RawMemoryDump()
{
    const MappedRanges mapped;
    std::string result;
    for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
    {
        BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
        if (!pObj)
        {
            break;
        }
        std::string words;
        HashRawMemory(*pObj, mapped, &words);
        std::string header = "obj[" + std::to_string(i) + "] " + ClassName(*pObj);
        if (pObj->GetIsBaseAnimatedWithPhysicsObj())
        {
            // Where the animation's parts are, to read the words by
            Animation& anim = static_cast<BaseAnimatedWithPhysicsGameObject*>(pObj)->GetAnimation();
            const u8* pStart = reinterpret_cast<const u8*>(pObj);
            header += " [animation at " + std::to_string(reinterpret_cast<const u8*>(&anim) - pStart) + ", anim res at " + std::to_string(reinterpret_cast<const u8*>(&anim.mAnimRes) - pStart) + ", anim res size " + std::to_string(sizeof(AnimResource)) + ", rgb at " + std::to_string(reinterpret_cast<const u8*>(&anim.GetRgb()) - pStart) + ", render layer at " + std::to_string(reinterpret_cast<const u8*>(&anim.GetRenderLayer()) - pStart) + ", blend mode at " + std::to_string(reinterpret_cast<const u8*>(&anim.GetBlendMode()) - pStart) + ", sizeof animation " + std::to_string(sizeof(Animation)) + ", poly at " + std::to_string(reinterpret_cast<const u8*>(&anim.mPoly) - pStart) + "]";
        }
        result += header + "\n" + words + "\n";
    }
    return result;
}

std::string StateHash::ClassName(const BaseGameObject& obj)
{
    const char* name = typeid(obj).name();
#if defined(__GNUC__) || defined(__clang__)
    int status = 0;
    char* demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
    if (demangled)
    {
        std::string result = demangled;
        std::free(demangled);
        return result;
    }
#endif
    return name;
}

StateHash StateHash::Compute(BaseMap& map, bool perPart)
{
    StateHash result;
    Hasher total;
    const MappedRanges mapped;

    auto addPart = [&](const std::string& name, u64 hash, const std::string& text = std::string(), bool inTotal = true)
    {
        if (inTotal)
        {
            total.Add(hash);
        }
        if (perPart)
        {
            result.mParts.push_back({name, hash, text});
        }
    };

    {
        Hasher h;
        h.Add(sGnFrame);
        addPart("frame", h.Value());
    }
    {
        Hasher h;
        h.Add(sRandomSeed);
        addPart("rng", h.Value());
    }
    {
        Hasher h;
        h.Add(Guid::GetNextGuidCounter());
        addPart("guid_counter", h.Value());
    }
    {
        Hasher h;
        h.Add(static_cast<s32>(map.mCurrentLevel));
        h.Add(map.mCurrentPath);
        h.Add(map.mCurrentCamera);
        addPart("camera", h.Value());
    }
    {
        Hasher h;
        h.Bytes(gSwitchStates.mData, sizeof(gSwitchStates.mData));
        std::string text;
        if (perPart)
        {
            for (u32 i = 0; i < ALIVE_COUNTOF(gSwitchStates.mData); i++)
            {
                if (gSwitchStates.mData[i])
                {
                    text += std::to_string(i) + "=" + std::to_string(gSwitchStates.mData[i]) + " ";
                }
            }
        }
        addPart("switches", h.Value(), text);
    }

    {
        // What the objects have done to the TLVs of the paths: their flags, and what a TLV keeps for its object
        Hasher h;
        std::string text;
        for (std::unique_ptr<BinaryPath>& pBinPath : map.GetLoadedPaths())
        {
            if (!pBinPath)
            {
                continue;
            }
            for (auto& cam : pBinPath->GetCameras())
            {
                for (auto& pTlv : cam->mTlvs.mTlvs)
                {
                    // A save keeps the flags of the TLVs with an attribute only: the others are as the path was loaded. A demo spawn
                    // point and a slurg spawner (the game never makes it) make no object, so nothing reads their flags
                    if (pTlv->mAttribute == relive::QuiksaveAttribute::eDoNothing_0 || pTlv->mTlvType == ReliveTypes::eDemoSpawnPoint || pTlv->mTlvType == ReliveTypes::eSlurgSpawner)
                    {
                        continue;
                    }
                    const u8 flags = pTlv->mTlvFlags.Raw().all;
                    const u8 meaning = pTlv->mTlvSpecificMeaning;
                    h.Bytes(&flags, sizeof(flags));
                    h.Bytes(&meaning, sizeof(meaning));
                    if (perPart)
                    {
                        char line[64];
                        const Guid::TlvOffsetLevelIdPathId info = pTlv->mId.GetTlvInfo();
                        snprintf(line, sizeof(line), "tlv %04x:%02x:%02x type=%d flags=%02x meaning=%02x\n", info.tlvOffset, info.levelId, info.pathId, static_cast<s32>(pTlv->mTlvType), flags, meaning);
                        text += line;
                    }
                }
            }
        }
        addPart("tlvs", h.Value(), text);
    }
    {
        // What was broadcast for the next frame
        Hasher h;
        const EventsSaveState events = EventsGetSaveState();
        h.Bytes(&events.mEventsToUse, sizeof(events.mEventsToUse));
        for (const auto& buffer : events.mEventObjectIds)
        {
            for (const Guid& id : buffer)
            {
                h.Bytes(&id, sizeof(id));
            }
        }
        addPart("events", h.Value());
    }
    {
        // The variables outside any object
        Hasher h;
        const std::string text = WorldStatics::Describe();
        h.Bytes(text.data(), text.size());
        addPart("statics", h.Value(), text);
    }
    {
        // The order they update in
        Hasher h;
        std::string text;
        for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
        {
            BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
            if (!pObj)
            {
                break;
            }
            // An object that is dead is gone once the frame ends: a save leaves it out
            if (pObj->GetDead())
            {
                continue;
            }
            // Every object is in the list, also those that draw or play sound only: the list closes up over a destroyed
            // object with the last, so where they are decides where the others end up
            const std::string className = ClassName(*pObj);
            h.Bytes(className.data(), className.size());
            h.Add(pObj->mBaseGameObjectId);
            text += className + " " + pObj->mBaseGameObjectId.ToString().substr(10, 8) + "\n";
        }
        addPart("order", h.Value(), text);
    }
    {
        // The order of the other lists the update and the drawing go through
        Hasher hd;
        std::string textDraw;
        for (s32 i = 0; i < gObjListDrawables->Size(); i++)
        {
            BaseGameObject* pObj = gObjListDrawables->ItemAt(i);
            if (!pObj)
            {
                break;
            }
            if (pObj->GetDead())
            {
                continue;
            }
            hd.Add(pObj->mBaseGameObjectId);
            textDraw += ClassName(*pObj) + " " + pObj->mBaseGameObjectId.ToString().substr(10, 8) + "\n";
        }
        addPart("draw_order", hd.Value(), textDraw);

        Hasher ha;
        std::string textAlive;
        for (s32 i = 0; i < gBaseAliveGameObjects->Size(); i++)
        {
            BaseAliveGameObject* pObj = gBaseAliveGameObjects->ItemAt(i);
            if (!pObj)
            {
                break;
            }
            if (pObj->GetDead())
            {
                continue;
            }
            ha.Add(pObj->mBaseGameObjectId);
            textAlive += ClassName(*pObj) + " " + pObj->mBaseGameObjectId.ToString().substr(10, 8) + "\n";
        }
        addPart("alive_order", ha.Value(), textAlive);

        Hasher hn;
        std::string textAnim;
        for (s32 i = 0; i < AnimationBase::gAnimations->Size(); i++)
        {
            AnimationBase* pAnim = AnimationBase::gAnimations->ItemAt(i);
            if (!pAnim)
            {
                break;
            }
            auto* pAnimation = dynamic_cast<Animation*>(pAnim);
            BaseGameObject* pOwner = pAnimation ? pAnimation->GetOwner() : nullptr;
            if (pOwner && !pOwner->GetDead())
            {
                hn.Add(pOwner->mBaseGameObjectId);
                textAnim += ClassName(*pOwner) + " " + pOwner->mBaseGameObjectId.ToString().substr(10, 8) + "\n";
            }
        }
        addPart("anim_order", hn.Value(), textAnim);

        Hasher hp;
        std::string textPlat;
        for (s32 i = 0; i < PlatformBase::Platforms().Size(); i++)
        {
            BaseGameObject* pObj = PlatformBase::Platforms().ItemAt(i);
            if (!pObj)
            {
                break;
            }
            if (!pObj->GetDead())
            {
                hp.Add(pObj->mBaseGameObjectId);
                textPlat += ClassName(*pObj) + " " + pObj->mBaseGameObjectId.ToString().substr(10, 8) + "\n";
            }
        }
        addPart("platform_order", hp.Value(), textPlat);
    }

    for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
    {
        BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
        if (!pObj)
        {
            break;
        }
        if (pObj->GetDead())
        {
            continue;
        }
        u64 saveStateHash = 0;
        std::string text;
        std::string saveText;
        const u64 objHash = HashObject(*pObj, saveStateHash, perPart ? &text : nullptr, perPart ? &saveText : nullptr);
        const std::string name = perPart ? "obj[" + std::to_string(i) + "] " + ClassName(*pObj) : std::string();
        // Where they are in the list doesn't matter, and what is in them isn't game state
        const bool inTotal = !IsRenderOrSoundOnlyClass(ClassName(*pObj));
        addPart(name, objHash, text, inTotal);
        addPart(perPart ? name + " save" : std::string(), saveStateHash, saveText, inTotal);
        std::string rawText;
        addPart(perPart ? name + " raw" : std::string(), HashRawMemory(*pObj, mapped, perPart ? &rawText : nullptr), rawText, inTotal);
    }

    result.mTotal = total.Value();
    return result;
}

std::string StateHash::Differences(const StateHash& other, const std::vector<std::string>& ignoreClasses) const
{
    std::string result;

    // "obj[3] Dove save" -> "Dove save"
    auto classOf = [](const std::string& name)
    {
        const std::size_t close = name.find("] ");
        return name.rfind("obj[", 0) == 0 && close != std::string::npos ? name.substr(close + 2) : std::string();
    };
    auto ignored = [&](const std::string& cls)
    {
        // Render and sound objects are not part of the total, so they don't differ either
        for (const char* suffix : {"", " save", " raw"})
        {
            const std::string suffixText = suffix;
            if (cls.size() >= suffixText.size() && cls.compare(cls.size() - suffixText.size(), suffixText.size(), suffixText) == 0 && IsRenderOrSoundOnlyClass(cls.substr(0, cls.size() - suffixText.size())))
            {
                return true;
            }
        }
        for (const std::string& ignoreClass : ignoreClasses)
        {
            if (cls == ignoreClass || cls == ignoreClass + " save" || cls == ignoreClass + " raw")
            {
                return true;
            }
        }
        return false;
    };

    std::map<std::pair<std::string, u64>, s32> counts;
    for (const Part& part : mParts)
    {
        const std::string cls = classOf(part.mName);
        if (cls.empty())
        {
            // frame, rng, camera, switches
            auto it = std::find_if(other.mParts.begin(), other.mParts.end(), [&](const Part& p) { return p.mName == part.mName; });
            if (it == other.mParts.end() || it->mHash != part.mHash)
            {
                result += part.mName + " differs; ";
                if (part.mName == "switches" && it != other.mParts.end())
                {
                    result += "(was set: " + part.mText + "now set: " + it->mText + "); ";
                }
                if (part.mName == "statics" && it != other.mParts.end())
                {
                    // Which of them differ
                    std::map<std::string, std::string> now;
                    std::size_t pos = 0;
                    while (pos < it->mText.size())
                    {
                        const std::size_t end = it->mText.find('\n', pos);
                        const std::string line = it->mText.substr(pos, end - pos);
                        now[line.substr(0, line.find('='))] = line;
                        pos = end + 1;
                    }
                    pos = 0;
                    while (pos < part.mText.size())
                    {
                        const std::size_t end = part.mText.find('\n', pos);
                        const std::string line = part.mText.substr(pos, end - pos);
                        const std::string name = line.substr(0, line.find('='));
                        if (now[name] != line)
                        {
                            result += "(" + line + " vs " + now[name] + "); ";
                        }
                        pos = end + 1;
                    }
                }
                if (part.mName == "tlvs" && it != other.mParts.end())
                {
                    // The first TLV that differs
                    std::size_t a = 0;
                    std::size_t b = 0;
                    s32 shown = 0;
                    while (a < part.mText.size() && b < it->mText.size() && shown < 4)
                    {
                        const std::size_t endA = part.mText.find('\n', a);
                        const std::size_t endB = it->mText.find('\n', b);
                        const std::string left = part.mText.substr(a, endA - a);
                        const std::string right = it->mText.substr(b, endB - b);
                        if (left != right)
                        {
                            result += "(tlv " + left + " vs " + right + "); ";
                            shown++;
                        }
                        a = endA + 1;
                        b = endB + 1;
                    }
                }
                if ((part.mName == "order" || part.mName == "draw_order" || part.mName == "alive_order" || part.mName == "anim_order" || part.mName == "platform_order") && it != other.mParts.end())
                {
                    // Where the two lists of classes first differ
                    std::size_t a = 0;
                    std::size_t b = 0;
                    s32 index = 0;
                    while (a < part.mText.size() && b < it->mText.size())
                    {
                        const std::size_t endA = part.mText.find('\n', a);
                        const std::size_t endB = it->mText.find('\n', b);
                        const std::string left = part.mText.substr(a, endA - a);
                        const std::string right = it->mText.substr(b, endB - b);
                        if (left != right)
                        {
                            auto flat = [](std::string text)
                            {
                                std::replace(text.begin(), text.end(), '\n', ',');
                                return text;
                            };
                            result += "(first at " + std::to_string(index) + ": " + left + " vs " + right + "; was " + flat(part.mText) + " now " + flat(it->mText) + "); ";
                            break;
                        }
                        a = endA + 1;
                        b = endB + 1;
                        index++;
                    }
                }
            }
        }
        else if (!ignored(cls))
        {
            counts[{cls, part.mHash}]++;
        }
    }
    for (const Part& part : other.mParts)
    {
        const std::string cls = classOf(part.mName);
        if (!cls.empty() && !ignored(cls))
        {
            counts[{cls, part.mHash}]--;
        }
    }
    // Where the saves of an object that differ first differ: the byte, in the record
    std::map<std::string, std::vector<std::string>> savesOnlyHere;
    std::map<std::string, std::vector<std::string>> savesOnlyThere;
    for (const Part& part : mParts)
    {
        const std::string cls = classOf(part.mName);
        if (!cls.empty() && cls.size() > 5 && cls.compare(cls.size() - 5, 5, " save") == 0 && !part.mText.empty() && counts[{cls, part.mHash}] > 0)
        {
            savesOnlyHere[cls].push_back(part.mText);
        }
    }
    for (const Part& part : other.mParts)
    {
        const std::string cls = classOf(part.mName);
        if (!cls.empty() && cls.size() > 5 && cls.compare(cls.size() - 5, 5, " save") == 0 && !part.mText.empty() && counts[{cls, part.mHash}] < 0)
        {
            savesOnlyThere[cls].push_back(part.mText);
        }
    }
    for (const auto& entry : savesOnlyHere)
    {
        const auto it = savesOnlyThere.find(entry.first);
        if (it == savesOnlyThere.end() || it->second.empty())
        {
            continue;
        }
        const std::string& was = entry.second.front();
        const std::string& now = it->second.front();
        std::size_t k = 0;
        while (k < was.size() && k < now.size() && was[k] == now[k])
        {
            k++;
        }
        const std::size_t from = k >= 8 ? (k - k % 2) - 8 : 0;
        result += entry.first + " first differs at byte " + std::to_string(k / 2) + " (was ..." + was.substr(from, 24) + " now ..." + now.substr(from, 24) + "); ";
    }
    // The words of an object's memory that differ, as byte offsets in the object
    std::map<std::string, std::vector<std::string>> rawOnlyHere;
    std::map<std::string, std::vector<std::string>> rawOnlyThere;
    for (const Part& part : mParts)
    {
        const std::string cls = classOf(part.mName);
        if (cls.size() > 4 && cls.compare(cls.size() - 4, 4, " raw") == 0 && !part.mText.empty() && counts[{cls, part.mHash}] > 0)
        {
            rawOnlyHere[cls].push_back(part.mText);
        }
    }
    for (const Part& part : other.mParts)
    {
        const std::string cls = classOf(part.mName);
        if (cls.size() > 4 && cls.compare(cls.size() - 4, 4, " raw") == 0 && !part.mText.empty() && counts[{cls, part.mHash}] < 0)
        {
            rawOnlyThere[cls].push_back(part.mText);
        }
    }
    for (const auto& entry : rawOnlyHere)
    {
        const auto it = rawOnlyThere.find(entry.first);
        if (it == rawOnlyThere.end() || it->second.empty())
        {
            continue;
        }
        std::istringstream was(entry.second.front());
        std::istringstream now(it->second.front());
        std::string wasWord;
        std::string nowWord;
        std::string differing;
        std::size_t index = 0;
        std::size_t shown = 0;
        while (was >> wasWord && now >> nowWord)
        {
            // Two addresses are two objects, not a difference
            const bool bothAddresses = wasWord[0] == '?' && nowWord[0] == '?';
            if (wasWord != nowWord && !bothAddresses && shown < 8)
            {
                differing += "+" + std::to_string(index * sizeof(u64)) + " " + wasWord + "->" + nowWord + ", ";
                shown++;
            }
            index++;
        }
        if (!differing.empty())
        {
            result += entry.first + " words: " + differing + "; ";
        }
    }
    // Which of the fields an object is hashed by differ, for the ones that don't end in save or raw
    const auto isPlain = [](const std::string& cls)
    {
        const auto ends = [&cls](const char* suffix)
        {
            const std::size_t n = std::strlen(suffix);
            return cls.size() > n && cls.compare(cls.size() - n, n, suffix) == 0;
        };
        return !ends(" save") && !ends(" raw");
    };
    std::map<std::string, std::vector<std::string>> fieldsOnlyHere;
    std::map<std::string, std::vector<std::string>> fieldsOnlyThere;
    for (const Part& part : mParts)
    {
        const std::string cls = classOf(part.mName);
        if (!cls.empty() && isPlain(cls) && !part.mText.empty() && counts[{cls, part.mHash}] > 0)
        {
            fieldsOnlyHere[cls].push_back(part.mText);
        }
    }
    for (const Part& part : other.mParts)
    {
        const std::string cls = classOf(part.mName);
        if (!cls.empty() && isPlain(cls) && !part.mText.empty() && counts[{cls, part.mHash}] < 0)
        {
            fieldsOnlyThere[cls].push_back(part.mText);
        }
    }
    for (const auto& entry : fieldsOnlyHere)
    {
        const auto it = fieldsOnlyThere.find(entry.first);
        if (it == fieldsOnlyThere.end() || it->second.empty())
        {
            continue;
        }
        // Several objects of a class can differ, so list each pair rather than only the first
        const std::size_t pairs = std::min<std::size_t>(std::min(entry.second.size(), it->second.size()), 4);
        for (std::size_t i = 0; i < pairs; i++)
        {
            std::istringstream was(entry.second[i]);
            std::istringstream now(it->second[i]);
            std::string wasField;
            std::string nowField;
            std::string differing;
            while (was >> wasField && now >> nowField)
            {
                if (wasField != nowField)
                {
                    differing += wasField + " -> " + nowField + ", ";
                }
            }
            if (!differing.empty())
            {
                result += entry.first + " fields: " + differing + "[was " + entry.second[i].substr(0, 600) + "] [now " + it->second[i].substr(0, 600) + "]; ";
            }
        }
    }
    for (const auto& entry : counts)
    {
        if (entry.second != 0)
        {
            result += (entry.second > 0 ? "now has " : "now lacks ") + std::to_string(std::abs(entry.second)) + " " + entry.first.first + " (hash " + std::to_string(entry.first.second % 100000) + "); ";
        }
    }
    return result;
}

std::string StateHash::FirstDifference(const StateHash& other) const
{
    const std::size_t count = std::min(mParts.size(), other.mParts.size());
    for (std::size_t i = 0; i < count; i++)
    {
        if (mParts[i].mHash != other.mParts[i].mHash || mParts[i].mName != other.mParts[i].mName)
        {
            return mParts[i].mName + " vs " + other.mParts[i].mName;
        }
    }
    if (mParts.size() != other.mParts.size())
    {
        return "object count " + std::to_string(mParts.size()) + " vs " + std::to_string(other.mParts.size());
    }
    return {};
}

#pragma once

#include "Types.hpp"
#include "SaveStateBase.hpp"

#include <array>
#include <string>
#include <type_traits>
#include <vector>

class SerializedObjectData;

// Every WorldStatic, in a quicksave. A variable is known by a hash of its name.
struct WorldStaticsSaveState final : public SaveStateBase
{
    WorldStaticsSaveState()
        : SaveStateBase(ReliveTypes::eWorldStatics, sizeof(*this))
    { }
    static constexpr u32 kMaxStatics = 192;
    static constexpr u32 kMaxSize = 48;
    struct Static final
    {
        u32 mNameHash;
        u32 mSize;
        u8 mData[kMaxSize];
    };
    u32 mCount;
    Static mStatics[kMaxStatics];
};

// The variables of the game that live outside any object but are part of the world, e.g. the index a class takes
// its random numbers from. They are made with WorldStatic, which registers each by name, so a quicksave keeps
// them all and two worlds can be compared by them (StateHash).
class WorldStatics final
{
public:
    static void Register(const char* name, void* pData, std::size_t size);

    // Every one
    static WorldStaticsSaveState GetSaveState();
    // Sets the ones the save has; one it doesn't have is left as it is
    static void RestoreFromSaveState(const WorldStaticsSaveState& state);

    // All of them as they are now, to put back later in the same run: what making the objects of a restore does to
    // them is thrown away
    static std::vector<u8> Snapshot();
    static void RestoreSnapshot(const std::vector<u8>& snapshot);

    // Name and value in hex of each, one per line, for comparing worlds
    static std::string Describe();

    // Logs each that is not as the text (from Describe) had it: what putting back the saved ones changed, which is what
    // the constructors of the objects a restore made did to them
    static void LogChangedSince(const std::string& before, const char* what);
};

// A variable that is part of the world. Used where a plain variable of type T would be.
template <typename T>
class WorldStatic final
{
public:
    static_assert(std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>, "a pointer is an object's id, not state to copy");

    WorldStatic(const char* name, T initial)
        : mValue(initial)
    {
        WorldStatics::Register(name, &mValue, sizeof(T));
    }

    WorldStatic(const WorldStatic&) = delete;
    WorldStatic& operator=(const WorldStatic&) = delete;

    operator T&()
    {
        return mValue;
    }

    operator const T&() const
    {
        return mValue;
    }

    // For a class type, whose members the conversions don't reach
    const T& Get() const
    {
        return mValue;
    }

    // For an array
    auto& operator[](std::size_t index)
    {
        return mValue[index];
    }

    T& operator=(const T& value)
    {
        mValue = value;
        return mValue;
    }

private:
    T mValue;
};

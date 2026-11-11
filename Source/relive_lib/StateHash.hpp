#pragma once

#include "Types.hpp"
#include <string>
#include <vector>

class BaseMap;
class BaseGameObject;

// A hash of everything in the running world that a frame can change, so two runs can be compared
// frame by frame, and a quicksave restore checked against the run it was taken from. The bot
// (a replay it makes has to come out the same every time) and the determinism tests use it.
//
// What it covers: the frame number, the RNG seed, the camera, the switch states and each game
// object in object list order (type, flags, position, velocity, animation frame, and for the
// characters their health and motions, plus whatever the object writes for a quicksave).
// What it doesn't: rendering, sound, and objects' private state that a quicksave doesn't write
// either. Anything a restore gets wrong that shows up there is a gap in the save, which is the point.
struct StateHash final
{
    struct Part final
    {
        std::string mName;
        u64 mHash = 0;
        // What was hashed, as text (name=value), when asked for with perPart
        std::string mText;
    };

    u64 mTotal = 0;
    // Only filled when asked for, one per thing hashed: "frame", "rng", "camera", "switches", then
    // "obj[<index>] <type>"
    std::vector<Part> mParts;

    // perPart: fill mParts, for finding which thing differs
    static StateHash Compute(BaseMap& map, bool perPart);

    // Every object's memory as hex words (see StateHash.cpp), to see which bytes a "raw" part differs in
    static std::string RawMemoryDump();

    // The object's C++ class name (Slig, Dove...), for logs: many objects have no ReliveTypes
    static std::string ClassName(const BaseGameObject& obj);

    // What differs from other as text, or empty if they match. The objects are compared in any order,
    // leaving out those whose class is in ignoreClasses. Both have to be Compute'd with perPart.
    std::string Differences(const StateHash& other, const std::vector<std::string>& ignoreClasses) const;

    // The first part whose hash differs from other's, as "<name>: <this> vs <other>", or empty
    std::string FirstDifference(const StateHash& other) const;
};

#pragma once

#include "../Types.hpp"
#include <memory>

class BaseMap;

namespace Bot {

// The whole world at one moment, kept in memory: a quicksave the bot owns. Loading it puts the world back exactly
// (see StateHash and the restore_check automation command), which is how the bot tries something and takes it back.
// The game's own quicksave, the one dying goes back to, is left as it was.
class Checkpoint final
{
public:
    Checkpoint();
    ~Checkpoint();
    Checkpoint(Checkpoint&&) noexcept;
    Checkpoint& operator=(Checkpoint&&) noexcept;

    // Whether the world can be saved right now: between frames, Abe alive, no camera change under way
    static bool CanSave(BaseMap& map);
    void Save(BaseMap& map);
    // Starts putting the world back: it takes a few main loop iterations, until Loading() is false
    void Load(BaseMap& map);
    // True while a Load is under way. Once it is over the game's own quicksave is put back.
    bool Loading(BaseMap& map, bool betweenFrames);

    u32 mFrame = 0;

private:
    struct Data;
    std::unique_ptr<Data> mData;
    bool mLoading = false;
};

} // namespace Bot

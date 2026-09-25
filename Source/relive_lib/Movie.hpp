#pragma once

#include "GameObjects/BaseGameObject.hpp"
#include <memory>
#include <string>

bool AreMovieSkippingInputsHeld();

class MoviePlayback;

// Plays a movie once it's updated, taking over the main loop (see BaseGameObject::StartModal)
// until it has finished. Anything waiting on it watches gMovieRefCount go back to 0.
class Movie final : public BaseGameObject
{
public:
    virtual void VUpdate() override;
    virtual ModalState VModalUpdate() override;
    virtual void VScreenChanged() override;

    explicit Movie(const char_type* pName, ResourceManagerWrapper& resMan, BaseMap& map);
    ~Movie();

    static s32 gMovieRefCount;
private:
    void Init();
    void Finish();

    std::string mName;
    std::unique_ptr<MoviePlayback> mPlayback;
};

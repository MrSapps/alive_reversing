#pragma once

#include "BaseGameObject.hpp"

class BackgroundMusic final : public BaseGameObject
{
public:
    BackgroundMusic(s32 musicId, ResourceManagerWrapper& resMan, BaseMap& map);

    virtual void VUpdate() override;

    static void Stop();
    static void Play();
    // The SEQ the current path's background music plays, -1 for none
    static s16 CurrentSeqId();

    s16 mMusicId = 0;
};

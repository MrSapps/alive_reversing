#include "stdafx.h"
#include "BackgroundMusic.hpp"
#include "../AliveLibAE/stdlib.hpp"
#include "../MapWrapper.hpp"
#include "../Sound/Midi.hpp"
#include "BaseMap.hpp"

static s16 sBackgroundMusicSeqId = -1;

BackgroundMusic::BackgroundMusic(s32 musicId, ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    mMusicId = static_cast<s16>(musicId);
    sBackgroundMusicSeqId = -1;
}

void BackgroundMusic::VUpdate()
{
    if (mMusicId >= 0)
    {
        if (mMap.mCurrentLevel != EReliveLevelIds::eNone
            || mMap.mCurrentPath != 1
            || mMap.mCurrentCamera != 10)
        {
            SND_SEQ_PlaySeq(mMusicId, 0, 0);
        }
        sBackgroundMusicSeqId = mMusicId;
    }
    SetDead(true);
}

s16 BackgroundMusic::CurrentSeqId()
{
    return sBackgroundMusicSeqId;
}

// In order of functions in the orignal game these are not part of this object
void BackgroundMusic::Stop()
{
    if (sBackgroundMusicSeqId >= 0)
    {
        SND_SEQ_Stop(sBackgroundMusicSeqId);
    }
}

void BackgroundMusic::Play()
{
    if (sBackgroundMusicSeqId >= 0)
    {
        SND_SEQ_PlaySeq(sBackgroundMusicSeqId, 0, 0);
    }
}

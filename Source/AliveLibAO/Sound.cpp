#include "stdafx_ao.h"
#include "Sound.hpp"
#include "MusicController.hpp"
#include "Midi.hpp"
#include "../relive_lib/GameObjects/BackgroundMusic.hpp"
#include "../relive_lib/BaseMap.hpp"

#include "../relive_lib/Sound/Sound.hpp"

namespace AO {
void SND_SsQuit()
{
    SND_SsQuit_4EFD50();
}

void SND_Restart(BaseMap& map)
{
    MusicController::EnableMusic(1);
    if (gBackgroundMusicSeqId >= 0)
    {
        SND_SEQ_PlaySeq(static_cast<SeqId>(gBackgroundMusicSeqId), 0, 0);
    }
    map.Start_Sounds_For_Objects_In_Near_Cameras();
}

} // namespace AO

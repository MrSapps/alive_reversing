#include "stdafx_ao.h"
#include "../relive_lib/ObjectIds.hpp"
#include "../relive_lib/SerializedObjectData.hpp"
#include "BellSong.hpp"
#include "Abe.hpp"
#include "GameSpeak.hpp"
#include "Sfx.hpp"
#include "../AliveLibAE/stdlib.hpp"

namespace AO {

void BellSong::VUpdate()
{
    if (mCodeIdx >= mCodeLength)
    {
        mDone = true;
    }

    if (!mDone)
    {
        const GameSpeakEvents code_looked_up = Code_LookUp(mCode, static_cast<u16>(mCodeIdx), mCodeLength);
        switch (mType)
        {
            case relive::Path_BellsongStone::BellsongTypes::eChimes:
            {
                switch (code_looked_up)
                {
                    case GameSpeakEvents::eAbe_WhistleHigh:
                        SfxPlayMono(relive::SoundEffects::BellChime_LowPitch, 0);
                        break;
                    case GameSpeakEvents::eAbe_WhistleLow:
                        SfxPlayMono(relive::SoundEffects::BellChime_MediumPitch, 0);
                        break;
                    default:
                        SfxPlayMono(relive::SoundEffects::BellChime_HighPitch, 0);
                }
                SetUpdateDelay(15);
                break;
            }
            case relive::Path_BellsongStone::BellsongTypes::eWhistle:
            {
                switch (code_looked_up)
                {
                    case GameSpeakEvents::eAbe_WhistleHigh:
                        Mudokon_SFX(MudSounds::eWhistleHigh_1, 0, 0, 0, mMap);
                        break;
                    case GameSpeakEvents::eAbe_WhistleLow:
                        Mudokon_SFX(MudSounds::eWhistleLow_2, 0, 0, 0, mMap);
                        break;
                    default:
                        Mudokon_SFX(MudSounds::eFart_7, 0, 0, 0, mMap);
                }
                SetUpdateDelay(30);
                break;
            }
        }

        mCodeIdx++;

        if (mCodeIdx >= mCodeLength)
        {
            SetUpdateDelay(30);
        }
    }
}

BellSong::BellSong(relive::Path_BellsongStone::BellsongTypes type, u32 code, ResourceManager& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    mType = type;
    mCode = code;
    mCodeLength = Code_Length(code);
    mCodeIdx = 0;
    mDone = false;
    SetUpdateDelay(15);
}

void BellSong::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    BellSongSaveState data = {};
    data.mType = static_cast<s32>(mType);
    data.mCode = mCode;
    data.mCodeLength = mCodeLength;
    data.mCodeIdx = mCodeIdx;
    data.mDone = mDone;
    pSaveBuffer.Write(data);
}

void BellSong::CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map)
{
    const BellSongSaveState& state = *pData.ReadTmpPtr<BellSongSaveState>();
    BellSong* p = relive_new BellSong(static_cast<relive::Path_BellsongStone::BellsongTypes>(state.mType), static_cast<u32>(state.mCode), resMan, map);
    if (!p)
    {
        return;
    }
    p->mType = static_cast<relive::Path_BellsongStone::BellsongTypes>(state.mType);
    p->mCode = state.mCode;
    p->mCodeLength = state.mCodeLength;
    p->mCodeIdx = state.mCodeIdx;
    p->mDone = state.mDone;
}

} // namespace AO

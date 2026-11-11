#include "stdafx.h"
#include "../WorldStatic.hpp"
#include "GasEmitter.hpp"
#include "Math.hpp"
#include "../Sound/Midi.hpp"
#include "../SwitchStates.hpp"
#include "Particle.hpp"
#include "Sfx.hpp"
#include "../relive_lib/GameObjects/GasCountDown.hpp"
#include "../GameType.hpp"
#include "BaseMap.hpp"
#include "SerializedObjectData.hpp"

// The one that plays the sound
static WorldStatic<Guid> sMainGasEmitterId("GasEmitter.sMainGasEmitterId", Guid{});
// The channels its sound plays on are the sound engine's, not part of the state: a restore starts the sound again
static u32 sGasEmitterAudioMask = 0;

GasEmitter::GasEmitter(relive::Path_GasEmitter* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    SetType(ReliveTypes::eNone);

    if (GetGameType() == GameType::eAo)
    {
        mSmokeScale = FP_FromInteger(1);
        mParticleCount = 3;
        mGasColour = relive::Path_GasEmitter::GasColour::eGreen;
    }
    else
    {
        mSmokeScale = FP_FromDouble(0.5);
        mParticleCount = 1;
        mGasColour = pTlv->mColour;
        mSwitchId = pTlv->mSwitchId;
    }

    mTlvId = tlvId;

    mEmitterXPos = FP_FromInteger(pTlv->mTopLeftX);
    mEmitterYPos = FP_FromInteger(pTlv->mTopLeftY);

    mEmitPower = Math_NextRandom() % 4;
}

GasEmitter::~GasEmitter()
{
    mMap.TLV_Reset(mTlvId);

    if (sMainGasEmitterId.Get() == mBaseGameObjectId)
    {
        sMainGasEmitterId = Guid{};
        SND_Stop_Channels_Mask(sGasEmitterAudioMask);
        sGasEmitterAudioMask = 0;
    }
}

void GasEmitter::VStopAudio()
{
    if (sMainGasEmitterId.Get() == mBaseGameObjectId)
    {
        sMainGasEmitterId = Guid{};
        SND_Stop_Channels_Mask(sGasEmitterAudioMask);
        sGasEmitterAudioMask = 0;
    }
}

void GasEmitter::VScreenChanged()
{
    SetDead(true);
}

void GasEmitter::VUpdate()
{
    if ((gDeathGasOn && !((sGnFrame + mEmitPower) % 4)) || (SwitchStates_Get(mSwitchId) && mDrawFlipper && Math_RandomRange(0, 1)))
    {
        switch (mGasColour)
        {
            case relive::Path_GasEmitter::GasColour::eYellow:
                New_Smoke_Particles(mEmitterXPos, mEmitterYPos, mSmokeScale, mParticleCount, RGB16{128, 128, 32}, mResMan, mMap);
                break;

            case relive::Path_GasEmitter::GasColour::eRed:
                New_Smoke_Particles(mEmitterXPos, mEmitterYPos, mSmokeScale, mParticleCount, RGB16{128, 32, 32}, mResMan, mMap);
                break;

            case relive::Path_GasEmitter::GasColour::eGreen:
                New_Smoke_Particles(mEmitterXPos, mEmitterYPos, mSmokeScale, mParticleCount, RGB16{32, 128, 32}, mResMan, mMap);
                break;

            case relive::Path_GasEmitter::GasColour::eBlue:
                New_Smoke_Particles(mEmitterXPos, mEmitterYPos, mSmokeScale, mParticleCount, RGB16{32, 32, 128}, mResMan, mMap);
                break;

            case relive::Path_GasEmitter::GasColour::eWhite:
                New_Smoke_Particles(mEmitterXPos, mEmitterYPos, mSmokeScale, mParticleCount, RGB16{128, 128, 128}, mResMan, mMap);
                break;

            default:
                break;
        }

        if (!sMainGasEmitterId.Get().IsValid())
        {
            SfxPlayMono(relive::SoundEffects::Gas1, 127);
            sMainGasEmitterId = mBaseGameObjectId;
            sGasEmitterAudioMask = SfxPlayMono(relive::SoundEffects::Gas2, 127);
        }
        else if (sMainGasEmitterId.Get() == mBaseGameObjectId && !sGasEmitterAudioMask)
        {
            sGasEmitterAudioMask = SfxPlayMono(relive::SoundEffects::Gas2, 127);
        }
    }

    // Flip the value
    mDrawFlipper = !mDrawFlipper;
}

void GasEmitter::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    GasEmitterSaveState data = {};
    data.mTlvId = mTlvId;
    data.mEmitPower = mEmitPower;
    data.mDrawFlipper = mDrawFlipper;
    pSaveBuffer.Write(data);
}

void GasEmitter::CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map)
{
    const auto pState = pData.ReadTmpPtr<GasEmitterSaveState>();
    auto pTlv = static_cast<relive::Path_GasEmitter*>(map.TLV_From_Offset_Lvl_Cam(pState->mTlvId).GetTlv());
    // What played before the restore is not the sound of the restored emitter
    SND_Stop_Channels_Mask(sGasEmitterAudioMask);
    sGasEmitterAudioMask = 0;
    auto pEmitter = relive_new GasEmitter(pTlv, pState->mTlvId, resMan, map);
    pEmitter->mEmitPower = pState->mEmitPower;
    pEmitter->mDrawFlipper = pState->mDrawFlipper;
}

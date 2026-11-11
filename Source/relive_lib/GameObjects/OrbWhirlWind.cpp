#include "stdafx.h"
#include "OrbWhirlWind.hpp"
#include "../OrbWhirlWindParticle.hpp"
#include "../Function.hpp"
#include "BaseAnimatedWithPhysicsGameObject.hpp"
#include "../SerializedObjectData.hpp"

OrbWhirlWind::OrbWhirlWind(FP xpos, FP ypos, FP scale, bool bIsMudokonSpirit, ResourceManager& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    SetType(ReliveTypes::eNone);

    mXPos = xpos;
    mYPos = ypos;
    mScale = scale;

    mState = ParticlesState::eCreating;
    mParticleSpawnCounter = 0;

    mIsMudokonSpirit = bIsMudokonSpirit;
    gObjListDrawables->Push_Back(this);
    SetDrawable(true);

    mParticleIdx = 0;

    memset(mOrbParticles, 0, sizeof(mOrbParticles));
}

OrbWhirlWind::OrbWhirlWind(RestoreTag, ResourceManager& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    SetType(ReliveTypes::eNone);
    gObjListDrawables->Push_Back(this);
    SetDrawable(true);
    memset(mOrbParticles, 0, sizeof(mOrbParticles));
}

void OrbWhirlWind::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    OrbWhirlWindSaveState data = {};
    data.mXPos = mXPos;
    data.mYPos = mYPos;
    data.mScale = mScale;
    data.mParticleSpawnCounter = mParticleSpawnCounter;
    data.mState = static_cast<s32>(mState);
    data.mParticleIdx = mParticleIdx;
    data.mIsMudokonSpirit = mIsMudokonSpirit;
    for (s32 i = 0; i < mParticleIdx && i < ALIVE_COUNTOF(mOrbParticles); i++)
    {
        if (mOrbParticles[i])
        {
            data.mParticles[i] = mOrbParticles[i]->GetState();
        }
    }
    pSaveBuffer.Write(data);
}

void OrbWhirlWind::CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map)
{
    const OrbWhirlWindSaveState& state = *pData.ReadTmpPtr<OrbWhirlWindSaveState>();
    OrbWhirlWind* p = relive_new OrbWhirlWind(RestoreTag{}, resMan, map);
    if (!p)
    {
        return;
    }
    p->mXPos = state.mXPos;
    p->mYPos = state.mYPos;
    p->mScale = state.mScale;
    p->mParticleSpawnCounter = state.mParticleSpawnCounter;
    p->mState = static_cast<ParticlesState>(state.mState);
    p->mParticleIdx = state.mParticleIdx;
    p->mIsMudokonSpirit = state.mIsMudokonSpirit;
    for (s32 i = 0; i < p->mParticleIdx && i < ALIVE_COUNTOF(p->mOrbParticles); i++)
    {
        p->mOrbParticles[i] = relive_new OrbWhirlWindParticle(OrbWhirlWindParticle::RestoreTag{}, resMan, p);
        p->mOrbParticles[i]->SetState(state.mParticles[i]);
    }
}

void OrbWhirlWind::VPostRestore()
{
    for (s32 i = 0; i < mParticleIdx && i < ALIVE_COUNTOF(mOrbParticles); i++)
    {
        if (mOrbParticles[i])
        {
            mOrbParticles[i]->ResolveTarget();
        }
    }
}

void OrbWhirlWind::ToSpin(FP xpos, FP ypos, FP scale, BaseGameObject* pObj)
{
    for (s32 i = 0; i < mParticleIdx; i++)
    {
        if (mOrbParticles[i])
        {
            if (!mOrbParticles[i]->IsActive())
            {
                mOrbParticles[i]->Spin(xpos, ypos, scale, pObj);
            }
        }
    }
    mState = ParticlesState::eActive;
}

void OrbWhirlWind::ToStop()
{
    for (s32 i = 0; i < mParticleIdx; i++)
    {
        if (mOrbParticles[i])
        {
            if (!mOrbParticles[i]->IsActive())
            {
                mOrbParticles[i]->ToStop();
            }
        }
    }
    mState = ParticlesState::eActive;
}

void OrbWhirlWind::VRender(OrderingTable& ot)
{
    for (s32 i = 0; i < mParticleIdx; i++)
    {
        if (mOrbParticles[i])
        {
            if (!mOrbParticles[i]->IsActive())
            {
                mOrbParticles[i]->Render(ot);
            }
        }
    }
}

void OrbWhirlWind::VUpdate()
{
    if (mState == ParticlesState::eCreating)
    {
        if (!(mParticleSpawnCounter % 4))
        {
            auto pParticle = relive_new OrbWhirlWindParticle(
                mXPos,
                mYPos,
                mScale,
                mIsMudokonSpirit,
                mResMan, mMap, this);
            mOrbParticles[mParticleIdx++] = pParticle;

            if (mParticleIdx >= ALIVE_COUNTOF(mOrbParticles))
            {
                mState = ParticlesState::eCreated;
            }
        }
        mParticleSpawnCounter++;
    }
    else if (mState == ParticlesState::eActive)
    {
        bool hasInactiveParticles = false;
        for (s32 i = 0; i < mParticleIdx; i++)
        {
            if (mOrbParticles[i])
            {
                if (!mOrbParticles[i]->IsActive())
                {
                    hasInactiveParticles = true;
                    break;
                }
            }
        }

        if (!hasInactiveParticles)
        {
            SetDead(true);
        }
    }

    for (s32 i = 0; i < mParticleIdx; i++)
    {
        if (mOrbParticles[i])
        {
            if (!mOrbParticles[i]->IsActive())
            {
                mOrbParticles[i]->Update();
            }
        }
    }
}

OrbWhirlWind::~OrbWhirlWind()
{
    gObjListDrawables->Remove_Item(this);

    for (auto& obj : mOrbParticles)
    {
        delete obj;
    }
}


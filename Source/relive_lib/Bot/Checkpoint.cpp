#include "Checkpoint.hpp"
#include "../BaseMap.hpp"
#include "../Engine.hpp"
#include "../GameType.hpp"
#include "../GameObjects/BaseAliveGameObject.hpp"
#include "../../AliveLibAE/QuikSave.hpp"
#include "../../AliveLibAO/QuikSave.hpp"

extern s16 gNumCamSwappers; // Engine.cpp

namespace Bot {

struct Checkpoint::Data final
{
    // The world, and the game's own quicksave at the time
    ::Quicksave mAe;
    ::Quicksave mAeActive;
    AO::Quicksave mAo;
    AO::Quicksave mAoActive;
};

Checkpoint::Checkpoint() = default;
Checkpoint::~Checkpoint() = default;
Checkpoint::Checkpoint(Checkpoint&&) noexcept = default;
Checkpoint& Checkpoint::operator=(Checkpoint&&) noexcept = default;

bool Checkpoint::CanSave(BaseMap& map)
{
    // An electrocution is saved with the world and kills a few frames in: a save taken during one is no use
    for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
    {
        const BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
        if (pObj && pObj->Type() == ReliveTypes::eElectrocute && !pObj->GetDead())
        {
            return false;
        }
        // (nor with the ghost of a slap lock on its way to its door: put back from a save taken then, the door
        // of the Mudomo vault's path 11 never opened, with every lock slapped)
        if (pObj && pObj->Type() == ReliveTypes::eSlapLock_OrbWhirlWind && !pObj->GetDead())
        {
            return false;
        }
    }
    return sControlledCharacter && sControlledCharacter->mHealth > FP_FromInteger(0) && gNumCamSwappers == 0 && map.mCamState == BaseMap::CamChangeStates::eInactive_0 && !map.mPendingSaveRestore && !map.GetActiveModal();
}

void Checkpoint::Save(BaseMap& map)
{
    if (!mData)
    {
        mData = std::make_unique<Data>();
    }
    // The save is of the world with what died already gone, as a restored world is
    BaseGameObject::DestroyDeadObjects();
    mFrame = sGnFrame;
    if (GetGameType() == GameType::eAe)
    {
        mData->mAeActive = QuikSave::gActiveQuicksaveData;
        QuikSave::DoQuicksave(map);
        mData->mAe = QuikSave::gActiveQuicksaveData;
        QuikSave::gActiveQuicksaveData = mData->mAeActive;
    }
    else
    {
        mData->mAoActive = AO::QuikSave::gActiveQuicksaveData;
        AO::QuikSave::DoQuicksave(map);
        mData->mAo = AO::QuikSave::gActiveQuicksaveData;
        AO::QuikSave::gActiveQuicksaveData = mData->mAoActive;
    }
}

void Checkpoint::Load(BaseMap& map)
{
    if (!mData)
    {
        return;
    }
    // An electrocution that began after the save isn't ended by the restore, and knows Abe by his id
    for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
    {
        BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
        if (pObj && pObj->Type() == ReliveTypes::eElectrocute)
        {
            pObj->SetDead(true);
        }
    }
    map.mRestoreKeepsInput = true;
    if (GetGameType() == GameType::eAe)
    {
        QuikSave::gActiveQuicksaveData = mData->mAe;
        QuikSave::LoadActive(map);
    }
    else
    {
        AO::QuikSave::gActiveQuicksaveData = mData->mAo;
        AO::QuikSave::LoadActive(map);
    }
    mLoading = true;
}

bool Checkpoint::Loading(BaseMap& map, bool betweenFrames)
{
    if (!mLoading)
    {
        return false;
    }
    if (!betweenFrames || map.mPendingSaveRestore || map.mCamState != BaseMap::CamChangeStates::eInactive_0 || gNumCamSwappers != 0)
    {
        return true;
    }
    mLoading = false;
    if (GetGameType() == GameType::eAe)
    {
        QuikSave::gActiveQuicksaveData = mData->mAeActive;
    }
    else
    {
        AO::QuikSave::gActiveQuicksaveData = mData->mAoActive;
    }
    return false;
}

} // namespace Bot

#pragma once

#include "../relive_lib/QuikSaveTypes.hpp"
#include "../relive_lib/SwitchStates.hpp"
#include "Abe.hpp"

class BaseMap;
class ResourceManagerWrapper;

namespace AO {

// AO's quicksave format, sharing PendingObjectRestoreData (TLV bly flags,
// via relive::QuiksaveAttribute) and QuicksaveWorldInfoBase (player position
// and progress counters) with AE - see relive_lib/QuikSaveTypes.hpp. AO has
// no zulags/meter bars/visited-level tracking, so unlike AE's own
// Quicksave_WorldInfo there's nothing AO-specific to add on top of the
// shared base.
//
// Per-object state is generic (see QuikSave::RestoreBlyData in QuikSave.cpp):
// every live object's VGetSaveState() gets called, and the base class
// default is a no-op, so only types that actually override it end up in
// the stream - currently Abe, plus the relive_lib GameObjects that already
// implement VGetSaveState/CreateFromSaveState identically for both engines
// (Grenade, TrapDoor, TimerTrigger, AbilityRing, ThrowableArray). AO's own
// enemy/NPC types don't implement it yet, so they keep whatever state
// their own VUpdate() naturally gives them on reload (freshly spawned from
// their TLV) - same as any AE object type that doesn't implement it either.
struct Quicksave final : PendingObjectRestoreData
{
    // The manual/F5 quicksave: world info + switches + (via PendingObjectRestoreData)
    // TLV bly flags and per-object state.
    QuicksaveWorldInfoBase mWorldInfo;
    SwitchStates mSwitchStates;

    // The lighter "continue at last checkpoint" snapshot Abe takes when he
    // passes a continue point TLV - just enough to bring Abe and the world
    // counters back, not a full quickload. Mirrors AE's
    // mRestartPathWorldInfo/mRestartPathAbeState/mRestartPathSwitchStates.
    QuicksaveWorldInfoBase mRestartPathWorldInfo;
    AbeSaveState mRestartPathAbeState;
    SwitchStates mRestartPathSwitchStates;
};

class QuikSave final
{
public:
    static void DoQuicksave(BaseMap& map);
    static void LoadActive(BaseMap& map);
    static void SaveWorldInfo(QuicksaveWorldInfoBase& pInfo, BaseMap& map);
    static void RestoreWorldInfo(const QuicksaveWorldInfoBase& pInfo);

    // Deferred restore consumed by GoTo_Camera() via BaseMap::mPendingSaveRestore.
    // Restores per-object state (currently just Abe - see the Quicksave
    // comment above) then TLV bly flags. Named to match AE's own
    // QuikSave::RestoreBlyData, which does the same two-part job.
    static void RestoreBlyData(PendingObjectRestoreData& pSaveData, ResourceManagerWrapper& resMan, BaseMap& map);

    // Checkpoint snapshot/respawn - see mRestartPath* fields above.
    static void SaveCheckpoint(BaseMap& map);
    static void RestoreCheckpoint(ResourceManagerWrapper& resMan, BaseMap& map, bool killObjects);

public:
    static Quicksave gActiveQuicksaveData;
};

} // namespace AO

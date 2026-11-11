#pragma once

#include "BaseAnimatedWithPhysicsGameObject.hpp"
#include "../BinaryPath.hpp"

class PathLine;

enum class eMotionType
{
    ePreviousMotion,
    eCurrentMotion,
    eNextMotion
};

// What every character keeps in common that the save of its own class might not carry. The character is the one with
// SaveStateBase::mObjectId.
struct AliveObjectStateSaveState final : public SaveStateBase
{
    AliveObjectStateSaveState()
        : SaveStateBase(ReliveTypes::eAliveObjectState, sizeof(*this))
    { }
    bool mbGotShot;
    FP mCollectionRectX;
    FP mCollectionRectY;
    FP mCollectionRectW;
    FP mCollectionRectH;
    s32 mLastAnimFrame;
    bool mCanBePossessed;
    bool mPossessed;
    bool mZappedByShrykull;
    bool mCanSetOffExplosives;
    bool mElectrocuted;
    bool mInvisible;
    bool mTeleporting;
    bool mElectrocuting;
    bool mCanBeesChase;
    // What it stands on, if it is made by an object that may be made after it
    Guid mPlatformId;
    // The line it stands on, by its place in the lines (plus 1, 0 for none): a raycast from where it is can miss it.
    // False for a save from the original game's, which has no line to give
    bool mCollisionLineSaved;
    s32 mCollisionLineIdx;
    // The line is one that a platform made while it lives (they come in the order the platforms were made, so the place
    // in the lines isn't the same in a restored world): it is the line of the platform it stands on
    bool mCollisionLineIsPlatforms;
    // Not in the save of a character that doesn't keep its health (a bomb is made again from its TLV)
    FP mHealth;
    // Where the line is and what it is, to find it among the ones the platforms make
    s16 mCollisionLineX;
    s16 mCollisionLineY;
    s16 mCollisionLineW;
    s16 mCollisionLineH;
    s16 mCollisionLineType;
};

class BaseAliveGameObject : public BaseAnimatedWithPhysicsGameObject
{
public:
    using BaseAnimatedWithPhysicsGameObject::BaseAnimatedWithPhysicsGameObject;

    explicit BaseAliveGameObject(s16 resourceArraySize, ResourceManager& resMan, BaseMap& map);
    ~BaseAliveGameObject();


    virtual void VUnPosses();
    virtual void VPossessed();
    virtual bool VTakeDamage(BaseGameObject* pFrom);
    virtual void VOnTlvCollision(TlvIterator tlvIterator);
    virtual void VOnTrapDoorOpen();

    // Note: this is only used for the GameAutoPlayer
    virtual s16 VGetMotion(eMotionType /*motionType*/)
    {
        return 0;
    }

    void CheckPlatformCollision(PSX_Point xy, PSX_Point wh, DynamicArrayT<BaseGameObject>& pObjList);

    void SetActiveCameraDelayedFromDir();
    Scale PerGameScale() const;
    bool WallHit(FP offY, FP offX);
    bool Check_IsOnEndOfLine(s16 direction, s16 distance);
    virtual class IBirdPortal* VIntoBirdPortal(s16 numGridBlocks);

    template <class BaseAliveGameObjectClass, class FnArray, class BrainState>
    static auto InvokeMemberFunction(BaseAliveGameObjectClass caller, const FnArray& fnArray, BrainState brainState)
    {
        auto function = fnArray[static_cast<u32>(brainState)];
        return (caller->*function)();
    }

    static bool IsInInvisibleZone(BaseAliveGameObject* pObj);

    bool GetCanBePossessed() const { return mCanBePossessed; }
    void SetCanBePossessed(bool val) { mCanBePossessed = val; }
    bool GetPossessed() const { return mPossessed; }
    void SetPossessed(bool val) { mPossessed = val; }
    bool GetZappedByShrykull() const { return mZappedByShrykull; }
    void SetZappedByShrykull(bool val) { mZappedByShrykull = val; }
    bool GetCanSetOffExplosives() const { return mCanSetOffExplosives; }
    void SetCanSetOffExplosives(bool val) { mCanSetOffExplosives = val; }
    bool GetElectrocuted() const { return mElectrocuted; }
    void SetElectrocuted(bool val) { mElectrocuted = val; }
    bool GetInvisible() const { return mInvisible; }
    void SetInvisible(bool val) { mInvisible = val; }
    bool GetTeleporting() const { return mTeleporting; }
    void SetTeleporting(bool val) { mTeleporting = val; }
    bool GetElectrocuting() const { return mElectrocuting; }
    void SetElectrocuting(bool val) { mElectrocuting = val; }
    bool GetCanBeesChase() const { return mCanBeesChase; }
    void SetCanBeesChase(bool val) { mCanBeesChase = val; }

    FP mHealth = FP_FromInteger(1);
    PathLine* BaseAliveGameObjectCollisionLine = nullptr;
    FP BaseAliveGameObjectLastLineYPos = {};
    bool mbGotShot = false;
    FP_RECT mCollectionRect = {};
    s32 mBaseAliveGameObjectLastAnimFrame = 0;
    TlvIterator BaseAliveGameObjectPathTLV = TlvIterator::Invalid();

    AliveObjectStateSaveState GetAliveObjectState() const;
    void SetAliveObjectState(const AliveObjectStateSaveState& state);

    // The id of the TLV BaseAliveGameObjectPathTLV is at, or none. For saves.
    Guid GetPathTlvId();
    void SetPathTlvFromId(const Guid& tlvId);
    Guid BaseAliveGameObject_PlatformId = Guid{};

protected:
    s16 SetBaseAnimPaletteTint(const TintEntry* pTintArray, EReliveLevelIds level_id, PalId resourceID);

private:
    bool mCanBePossessed = false;
    bool mPossessed = false;
    bool mZappedByShrykull = false;
    bool mCanSetOffExplosives = false;
    bool mElectrocuted = false;
    bool mInvisible = false;
    bool mTeleporting = false;
    bool mElectrocuting = false;
    bool mCanBeesChase = false;      // AO only: can the bees attack - can be above the value 1 but bee swarm only checks for non zero


public:
    virtual void VOnPathTransition(s32 camWorldX, s32 camWorldY, CameraPos direction);
    virtual void VCheckCollisionLineStillValid(s32 distance);

    // AO only currently
    virtual void VSetXSpawn(s16 camWorldX, s32 screenXPos); // AO
    virtual void VSetYSpawn(s32 camWorldY, s16 bLeft);      // AO

    bool MapFollowMe(bool snapToGrid);

    virtual bool VOnPlatformIntersection(BaseAnimatedWithPhysicsGameObject* pPlatform);

protected:
    bool MapFollowMeAO(bool snapToGrid);
    bool MapFollowMeAE(bool snapToGrid);

    void OnPathTransitionAO(s32 camWorldX, s32 camWorldY, CameraPos direction);
    void OnPathTransitionAE(s32 camWorldX, s32 camWorldY, CameraPos direction);

    bool InAirCollision(PathLine** ppLine, FP* hitX, FP* hitY, FP velY);
    BaseGameObject* FindObjectOfType(ReliveTypes typeToFind, FP xpos, FP ypos);

    void UsePathTransScale();                                                                                  // AO
    BaseAliveGameObject* GetStackedSlapTarget(const Guid& idToFind, ReliveTypes typeToFind, FP xpos, FP ypos); // AE
public:
    s16 BaseAliveGameObjectCollisionLineType = 0; // AE only, quick save data
    // Only between a save and its restore: the index in gCollisions of the line plus 1, or 0 for the line the type finds by raycast
    s16 BaseAliveGameObjectCollisionLineIdx = 0;
};

extern DynamicArrayT<BaseAliveGameObject>* gBaseAliveGameObjects;
extern BaseAliveGameObject* sControlledCharacter;

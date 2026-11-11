#pragma once

#include <cstddef>
#include "../Types.hpp"
#include "../DynamicArray.hpp"
#include "../data_conversion/guid.hpp"
#include "../ResourceManager.hpp"
#include "../ObjectTypes.hpp"

extern u32 sGnFrame;

struct BasePrimitive;

struct SaveStateBase;
struct GameSpeakListenerSaveState;
struct AliveObjectStateSaveState;
class GameSpeakListenerBase;
struct AnimationStateSaveState;
class SerializedObjectData;
class OrderingTable;
class BaseMap;

// What BaseGameObject::VModalUpdate() returns
enum class ModalState
{
    eRunning,
    eFinished, // ends the modal
    eQuitGame, // stops the main loop, see QuitGame
};

class [[nodiscard]] BaseGameObject
{
public:
    // Objects are made zeroed, so the padding between their members is the same in every world: the memory
    // of two objects in the same state is then the same (StateHash looks at it)
    static void* operator new(std::size_t size);
    static void operator delete(void* ptr);

    BaseGameObject(s16 bAddToObjectList, s16 resourceArraySize, ResourceManager& resMan, BaseMap& map);


    virtual ~BaseGameObject();

    virtual void VUpdate();

    virtual void VRender(OrderingTable& ot);

    virtual void VScreenChanged();

    virtual void VStopAudio();

    virtual void VGetSaveState(SerializedObjectData& /*pSaveBuffer*/);

    // Saves the object with VGetSaveState, and puts its id in what it wrote (SaveStateBase::mObjectId)
    void SaveState(SerializedObjectData& pSaveBuffer);

    // Second pass of a quicksave restore, once every saved object has been made: link up to the other
    // restored objects, e.g. with RefreshId. Objects are made in the order of the save, so the one an
    // object refers to may not exist yet while it is being made.
    virtual void VPostRestore();

    // For an object that the camera made again after a restore (not saved, so VPostRestore doesn't reach it): the
    // state its first update would have put it in, which the object saved was already in
    virtual void VPostRestoreFromTlv();

    // The listener of an object that listens to the game speak, which is saved with it
    virtual GameSpeakListenerBase* VListener();

    // Memory inside the object that isn't game state, for comparing worlds (StateHash): what it drew with, handles
    // to sound or the renderer, the copy of the TLV it was made from (which holds the flags the TLV had then)
    struct NotStateMemory final
    {
        const void* mStart;
        std::size_t mSize;
    };
    virtual std::vector<NotStateMemory> VNotStateMemory();

    // Puts what every saved object has in its header (SaveStateBase) on the object it made
    static void ApplySavedHeader(const SaveStateBase& header);

    // Puts what every character has in common back on it
    static void RestoreAliveObjectState(const AliveObjectStateSaveState& state);

    // Puts where a character was in the game speak events back on it
    static void RestoreListener(const GameSpeakListenerSaveState& state);

    // Puts the animation a save kept for an object that has no save of its own back on the object that was made again
    static void RestoreAnimationState(const AnimationStateSaveState& state);

    // Puts the objects in the order the ids are in: that is the order they update in, which decides who uses the
    // random numbers first. Objects that aren't in the list keep their places.
    static void ReorderObjects(const std::vector<Guid>& idsInOrder);

    // Sets what the last frame's drawing left on every object that is drawn (see Engine::RunFrame)
    static void MarkDrawnObjects();

    // Calls VPostRestoreFromTlv on every object
    static void PostRestoreAllFromTlv();

    // Calls VPostRestore on every object
    static void PostRestoreAll();

    // Writes the order of the lists that objects are in besides the one they update in, see ListOrderSaveState
    static void SaveListOrders(SerializedObjectData& pSaveBuffer);
    // Puts those lists in the order of a save
    static void ReorderLists(const std::vector<Guid>& drawables, const std::vector<Guid>& aliveObjects, const std::vector<std::pair<Guid, s32>>& animations, const std::vector<Guid>& platforms);

    // Destroys the dead objects now, between frames, as the next frame would (those that were drawn as they died
    // are kept until then). A save leaves the dead out, and the list takes the last object into the place of a
    // destroyed one: the world that is saved has to have them gone, or it is not in the order the saved one is.
    static void DestroyDeadObjects();

    // Sets the id counter to what a quickload has, but past the ids that objects living through the load hold (the
    // fade of a death, say): the next object made would share one
    static void RestoreGuidCounter(u32 counter);

    // Runs one main loop iteration while this object is the active modal, in place of the
    // whole game frame. See StartModal.
    virtual ModalState VModalUpdate();

    static ReliveTypes FromAO(AOTypes aoType);
    static AOTypes ToAO(ReliveTypes reliveType);

    static ReliveTypes FromAE(AETypes aeType);

    static AETypes ToAE(ReliveTypes reliveType);

    void SetType(ReliveTypes type);

    ReliveTypes Type() const;

protected:
    static Guid RefreshId(const Guid& objectId);

public:
    void SetUpdateDelay(s32 updateDelay);

    s32 UpdateDelay() const;

    AnimResource& GetAnimRes(AnimId animId);

    PalResource& GetPalRes(PalId palId);

    ResourceManager& GetResMan() const { return mResMan; }

    BaseMap& GetMap() const { return mMap; }

    bool GetUpdatable() const { return mUpdatable; }
    void SetUpdatable(bool val) { mUpdatable = val; }
    bool GetDead() const { return mDead; }
    void SetDead(bool val) { mDead = val; }
    bool GetDrawable() const { return mDrawable; }
    void SetDrawable(bool val) { mDrawable = val; }
    bool GetIsBaseAnimatedWithPhysicsObj() const { return mIsBaseAnimatedWithPhysicsObj; }
    void SetIsBaseAnimatedWithPhysicsObj(bool val) { mIsBaseAnimatedWithPhysicsObj = val; }
    bool GetIsBaseAliveGameObject() const { return mIsBaseAliveGameObject; }
    void SetIsBaseAliveGameObject(bool val) { mIsBaseAliveGameObject = val; }
    bool GetCanExplode() const { return mCanExplode; }
    void SetCanExplode(bool val) { mCanExplode = val; }
    bool GetInteractive() const { return mInteractive; }
    void SetInteractive(bool val) { mInteractive = val; }
    bool GetSurviveDeathReset() const { return mSurviveDeathReset; }
    void SetSurviveDeathReset(bool val) { mSurviveDeathReset = val; }
    bool GetUpdateDuringCamSwap() const { return mUpdateDuringCamSwap; }
    void SetUpdateDuringCamSwap(bool val) { mUpdateDuringCamSwap = val; }
    bool GetCantKill() const { return mCantKill; }
    void SetCantKill(bool val) { mCantKill = val; }
    bool GetModal() const { return mModal; }

    // Takes over the main loop, in place of the original game's nested loops: from the next
    // iteration Engine::Game_Loop runs only this object's VModalUpdate(). The frame that was
    // running is suspended and carries on where it stopped once VModalUpdate() returns
    // eFinished (or the object is deleted). A modal started while another is active runs on top of it
    // until it ends. Whatever started the modal checks on it from its own state when it
    // next runs, e.g. Movie::gMovieRefCount going back to 0.
    void StartModal();
    // Called by the main loop when VModalUpdate() returns eFinished
    void EndModal();

    // Helper to check if a timer has expired
    template <class T>
    static inline bool Expired(const T& value)
    {
        return static_cast<s32>(sGnFrame) > value;
    }

    template <class T>
    static s32 MakeTimer(const T value)
    {
        return static_cast<s32>(sGnFrame) + value;
    }
public:
    Guid mBaseGameObjectId;                  // AE
    Guid mBaseGameObjectTlvInfo;                 // AE
    DynamicArrayT<u8*> mBaseGameObjectResArray; // AE
    std::vector<AnimResource> mLoadedAnims;
    std::vector<PalResource> mLoadedPals;
    u32 mChaseCounter = 0;

private:
    ReliveTypes mBaseGameObjectTypeId = ReliveTypes::eNone;
    s32 mBaseGameObjectUpdateDelay = 0;

    bool mUpdatable = false;
    bool mDead = false;
    bool mDrawable = false;
    bool mIsBaseAnimatedWithPhysicsObj = false;
    bool mIsBaseAliveGameObject = false;
    bool mCanExplode = false;
    bool mInteractive = false;
    bool mSurviveDeathReset = false;
    bool mUpdateDuringCamSwap = false;
    bool mCantKill = false;
    bool mModal = false;

protected:
    ResourceManager& mResMan;
    BaseMap& mMap;
};

extern DynamicArrayT<BaseGameObject>* gBaseGameObjects;

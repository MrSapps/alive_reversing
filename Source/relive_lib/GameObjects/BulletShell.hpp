#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../SaveStateBase.hpp"

class PathLine;

struct BulletShellSaveState final : public SaveStateBase
{
    BulletShellSaveState()
        : SaveStateBase(ReliveTypes::eBulletShell, sizeof(*this))
    { }
    s16 mFloorBounceCount;
    FP mSpeed;
};

class BulletShell final : public BaseAnimatedWithPhysicsGameObject
{
public:
    // For a save: made with nothing counted, the count is put back with the rest
    struct RestoreTag final
    { };
    BulletShell(RestoreTag, ResourceManager& resMan, BaseMap& map);

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    BulletShell(FP xpos, FP ypos, bool flipX, FP scale, ResourceManager& resMan, BaseMap& map);
    ~BulletShell();

    virtual void VUpdate() override;
    virtual std::vector<NotStateMemory> VNotStateMemory() override;

private:
    PathLine* mLine = nullptr;
    s16 mFloorBounceCount = 0;
    FP mSpeed = FP_FromInteger(1);
};

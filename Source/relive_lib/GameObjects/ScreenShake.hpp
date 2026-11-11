#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../SaveStateBase.hpp"
#include "../relive_lib/Primitives.hpp"

struct ScreenShakeSaveState final : public SaveStateBase
{
    ScreenShakeSaveState()
        : SaveStateBase(ReliveTypes::eScreenShake, sizeof(*this))
    { }
    bool mEnableShakeEvent;
    bool mSofterShakes;
    s16 mShakeNumber;
};

class ScreenShake final : public BaseGameObject
{
public:
    ScreenShake(bool enableShakeEvent, bool softerShakes, ResourceManager& resMan, BaseMap& map);
    ~ScreenShake();

    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    virtual std::vector<NotStateMemory> VNotStateMemory() override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

private:
    Prim_ScreenOffset mScreenOffset = {};
    s16 mShakeNumber = 0;
    bool mEnableShakeEvent = false;
    bool mSofterShakes = false;
};

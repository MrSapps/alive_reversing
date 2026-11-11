#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../SaveStateBase.hpp"
#include "../WorldStatic.hpp"
#include "../relive_lib/Layer.hpp"

extern WorldStatic<s32> gDeathGasCount;

struct DeathGasSaveState final : public SaveStateBase
{
    DeathGasSaveState()
        : SaveStateBase(ReliveTypes::eDeathGas, sizeof(*this))
    { }
    s32 mLayer;
    s32 mAmount;
    s32 mTotal;
    bool mDone;
    u8 mByte1[50];
    u8 mByte2[50];
};

class DeathGas final : public BaseGameObject
{
public:
    // For a save: the gas count is put back with the rest of the variables
    struct RestoreTag final
    { };
    DeathGas(RestoreTag, ResourceManager& resMan, BaseMap& map);

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    DeathGas(Layer layer, s32 amount, ResourceManager& resMan, BaseMap& map);
    ~DeathGas();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;

private:
    Layer mLayer = Layer::eLayer_0;
    s32 mAmount = 0;
    s32 mTotal = 0;
    bool mDone = false;
};

#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"
#include "../relive_lib/FixedPoint.hpp"
#include "../relive_lib/SaveStateBase.hpp"

struct GasEmitterSaveState final : public SaveStateBase
{
    GasEmitterSaveState()
        : SaveStateBase(ReliveTypes::eGasEmitter, sizeof(*this))
    { }
    Guid mTlvId;
    s32 mEmitPower;
    bool mDrawFlipper;
};

class GasEmitter final : public BaseGameObject
{
public:
    GasEmitter(relive::Path_GasEmitter* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~GasEmitter();

    virtual void VStopAudio() override;
    virtual void VScreenChanged() override;
    virtual void VUpdate() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

private:
    Guid mTlvId;
    s32 mEmitPower = 0;
    FP mEmitterXPos = {};
    FP mEmitterYPos = {};
    FP mSmokeScale = {};
    s16 mParticleCount = 0;
    s16 mSwitchId = 0;
    bool mDrawFlipper = true;
    relive::Path_GasEmitter::GasColour mGasColour = relive::Path_GasEmitter::GasColour::eYellow;
};

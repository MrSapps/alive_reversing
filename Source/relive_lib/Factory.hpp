#pragma once

class Guid;
class ResourceManagerWrapper;
class BaseMap;

namespace relive
{
    class Path_TLV;

    class Factory final
    {
        public:
        explicit Factory(ResourceManagerWrapper& resMan, BaseMap& map)
            : mResourceManager(resMan)
            , mMap(map)
        {
        }

        enum class LoadMode : s16
        {
            ConstructObject_0 = 0,
            LoadResourceFromList_1 = 1,
            LoadResource_2 = 2
        };

        void HoistAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void EdgeAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void ShadowZoneAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void LiftPointAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void WellAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void DoveAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void RockSackAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void ZBallAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void FallingItemAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void PullRingRopeAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void BackgroundAnimationAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void HoneyAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void TimedMineAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SligAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SlogAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void LeverAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void BellHammerAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SecurityOrbAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void LiftMudAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void BeeSwarmHoleAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void HoneySackAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void AbeStartAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void ElumWallAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SlingMudAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void BeeNestAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void MineAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void UXBAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void ParamiteAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void BatAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void RingMudAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void BirdPortalAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void BellSongStoneAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void TrapDoorAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void RollingBallAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SligBoundLeftAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void RollingBallStopperAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void FootSwitchAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SecurityClawAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void MotionDetectorAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SligSpawnerAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void ElectricWallAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void LiftMoverAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void ChimeLockAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void ElumAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void MeatSackAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void ScrabAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void FlintLockFireAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SligBoundRightAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void InvisibleSwitchAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void WorkerMudokonAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void DoorFlameAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void MovingBombAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void MeatSawAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void MainMenuControllerAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void HintFlyAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void TimerTriggerAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SecurityDoorAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void BoomMachineAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void LCDScreenAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void HandStoneAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void CreditsControllerAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void LCDStatusBoardAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SwitchStateBooleanLogicAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void MusicTriggerAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void LightEffectAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SlogSpawnerAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void GasCountDownAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void RingCancelAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void GasEmitterAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void ZzzSpawnerAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void BackgroundGlukkonAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void KillUnsavedMudsAO(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);

        void MainMenuControllerAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void HoistAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void EdgeAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void DoorAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void ShadowZoneAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void LiftPointAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void WellAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void DoveAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void RockSackAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void FallingItemAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void PullRingRopeAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void TimedMineAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void SligAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SlogAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void LeverAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void SecurityOrbAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void AbeStartAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void MineAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void UXBAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void ParamiteAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void MovieHandStoneAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void BirdPortalAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void TrapDoorAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void SligBoundLeftAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void FootSwitchAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void ChantSuppressor_WithArmsAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void MotionDetectorAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SligSpawnerAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void ElectricWallAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void LiftMoverAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void MeatSackAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void ScrabAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SligBoundRightAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void InvisibleSwitchAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void MudokonAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void DoorFlameAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void MovingBombAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void TimerTriggerAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void SecurityDoorAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void BoomMachineAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void BackgroundAnimationAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void LCDScreenAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void HandStoneAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void CreditsControllerAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void LCDStatusBoardAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void WheelSyncerAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void MusicTriggerAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void SlogSpawnerAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void GasCountDownAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void GasEmitterAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void ZzzSpawnerAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void GlukkonAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void WaterAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void WorkWheelAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void LaughingGasAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void FlyingSligAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void FleechAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SlurgAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SlamDoorAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void LevelLoaderAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void DemoSpawnPointAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void TeleporterAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void SlurgSpawnerAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void DrillAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void ColourfulMeterAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void FlyingSligSpawnerAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void MineCarAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void BoneBagAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void ExplosionSetAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void MultiSwitchControllerAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void StatusLightAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void SlapLockAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void ParamiteWebLineAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void AlarmAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void BrewMachineAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadmode);
        void ScrabSpawnerAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void CrawlingSligAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SligGetPantsAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void SligGetWingsAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void GreeterAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void CrawlingSligButtonAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void GlukkonSwitchAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void DoorBlockerAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void RollingBallStopperAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void TorturedMudokonAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void TrainDoorAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);
        void RollingBallAE(relive::Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode);

        void ConstructTLVObject(Path_TLV* pTlv, const Guid& tlvInfo, LoadMode loadMode);

        ResourceManagerWrapper& mResourceManager;
        BaseMap& mMap;
    };
}

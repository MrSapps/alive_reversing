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
        enum class LoadMode : s16
        {
            ConstructObject_0 = 0,
            LoadResourceFromList_1 = 1,
            LoadResource_2 = 2
        };

        void HoistAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void EdgeAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ShadowZoneAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void LiftPointAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void WellAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void DoveAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void RockSackAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ZBallAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void FallingItemAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void PullRingRopeAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void BackgroundAnimationAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void HoneyAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void TimedMineAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SligAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SlogAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void LeverAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void BellHammerAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SecurityOrbAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void LiftMudAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void BeeSwarmHoleAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void HoneySackAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void AbeStartAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ElumWallAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SlingMudAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void BeeNestAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void MineAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void UXBAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ParamiteAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void BatAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void RingMudAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void BirdPortalAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void BellSongStoneAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void TrapDoorAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void RollingBallAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SligBoundLeftAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void RollingBallStopperAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void FootSwitchAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SecurityClawAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void MotionDetectorAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SligSpawnerAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ElectricWallAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void LiftMoverAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ChimeLockAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ElumAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void MeatSackAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ScrabAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void FlintLockFireAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SligBoundRightAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void InvisibleSwitchAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void WorkerMudokonAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void DoorFlameAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void MovingBombAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void MeatSawAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void MainMenuControllerAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void HintFlyAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void TimerTriggerAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SecurityDoorAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void BoomMachineAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void LCDScreenAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void HandStoneAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void CreditsControllerAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void LCDStatusBoardAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SwitchStateBooleanLogicAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void MusicTriggerAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void LightEffectAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SlogSpawnerAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void GasCountDownAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void RingCancelAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void GasEmitterAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ZzzSpawnerAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void BackgroundGlukkonAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void KillUnsavedMudsAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);

        void MainMenuControllerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void HoistAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void EdgeAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void DoorAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ShadowZoneAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void LiftPointAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void WellAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void DoveAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void RockSackAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void FallingItemAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void PullRingRopeAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void TimedMineAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SligAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SlogAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void LeverAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SecurityOrbAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void AbeStartAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void MineAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void UXBAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ParamiteAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void MovieHandStoneAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void BirdPortalAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void TrapDoorAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SligBoundLeftAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void FootSwitchAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ChantSuppressor_WithArmsAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void MotionDetectorAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SligSpawnerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ElectricWallAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void LiftMoverAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void MeatSackAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ScrabAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SligBoundRightAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void InvisibleSwitchAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void MudokonAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void DoorFlameAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void MovingBombAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void TimerTriggerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SecurityDoorAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void BoomMachineAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void BackgroundAnimationAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void LCDScreenAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void HandStoneAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void CreditsControllerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void LCDStatusBoardAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void WheelSyncerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void MusicTriggerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SlogSpawnerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void GasCountDownAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void GasEmitterAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ZzzSpawnerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void GlukkonAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void WaterAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void WorkWheelAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void LaughingGasAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void FlyingSligAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void FleechAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SlurgAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SlamDoorAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void LevelLoaderAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void DemoSpawnPointAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void TeleporterAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SlurgSpawnerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void DrillAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ColourfulMeterAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void FlyingSligSpawnerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void MineCarAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void BoneBagAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ExplosionSetAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void MultiSwitchControllerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void StatusLightAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SlapLockAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ParamiteWebLineAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void AlarmAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void BrewMachineAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void ScrabSpawnerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void CrawlingSligAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SligGetPantsAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void SligGetWingsAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void GreeterAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void CrawlingSligButtonAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void GlukkonSwitchAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void DoorBlockerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void RollingBallStopperAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void TorturedMudokonAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void TrainDoorAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
        void RollingBallAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);

        void ConstructTLVObject(Path_TLV* pTlv, const Guid& tlvInfo, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map);
    };
}

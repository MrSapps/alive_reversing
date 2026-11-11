#include "stdafx.h"
#include "AutomationSpawn.hpp"
#include "GameObjects/BaseAliveGameObject.hpp"
#include "GameObjects/AirExplosion.hpp"
#include "GameObjects/Alarm.hpp"
#include "GameObjects/Blood.hpp"
#include "GameObjects/Fade.hpp"
#include "GameObjects/Flash.hpp"
#include "GameObjects/ScreenShake.hpp"
#include "GameObjects/Gibs.hpp"
#include "GameObjects/Particle.hpp"
#include "GameObjects/ParticleBurst.hpp"
#include "GameObjects/Spark.hpp"
#include "GameObjects/BulletShell.hpp"
#include "GameObjects/DeathGas.hpp"
#include "GameObjects/Electrocute.hpp"
#include "GameObjects/GroundExplosion.hpp"
#include "GameObjects/OrbWhirlWind.hpp"
#include "GameObjects/PossessionFlicker.hpp"
#include "GameObjects/ThrowableTotalIndicator.hpp"
#include "FixedPoint.hpp"
#include "Layer.hpp"


struct SpawnEntry final
{
    const char* mName;
    void (*mSpawn)(FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map);
};

static const SpawnEntry kEntries[] = {
    {"spark", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new Spark(x, y, scale, 10, 0, 255, SparkType::eSmallChantParticle_0, resMan, map); }},
    {"spark_big", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new Spark(x, y, scale, 12, 0, 255, SparkType::eBigChantParticle_1, resMan, map); }},
    {"particle_smoke", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { New_Smoke_Particles(x, y, scale, 6, RGB16{128, 128, 128}, resMan, map); }},
    {"particle_orb", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { New_Orb_Particle(x, y, FP_FromInteger(1), FP_FromInteger(-1), scale, Layer::eLayer_0, RGB16{128, 100, 90}, resMan, map); }},
    {"particle_destroy", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { New_DestroyOrCreateObject_Particle(x, y, scale, resMan, map); }},
    {"particle_fire", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { New_ShootingFire_Particle(x, y, 1, scale, resMan, map); }},
    {"gibs_abe", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new Gibs(GibType::eAbe, x, y, FP_FromInteger(0), FP_FromInteger(-3), scale, false, resMan, map); }},
    {"gibs_slig", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new Gibs(GibType::eSlig, x, y, FP_FromInteger(2), FP_FromInteger(-5), scale, true, resMan, map); }},
    {"gibs_metal", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new Gibs(GibType::eMetal, x, y, FP_FromInteger(0), FP_FromInteger(5), scale, false, resMan, map); }},
    {"burst_rocks", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new ParticleBurst(x, y, 12, scale, BurstType::eRocks, resMan, map); }},
    {"burst_meat", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new ParticleBurst(x, y, 10, scale, BurstType::eMeat, resMan, map); }},
    {"burst_sparks", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new ParticleBurst(x, y, 20, scale, BurstType::eBigRedSparks, resMan, map, 13, true); }},
    {"burst_purple", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new ParticleBurst(x, y, 10, scale, BurstType::eBigPurpleSparks, resMan, map); }},
    {"flash", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new Flash(Layer::eLayer_Above_FG1_39, 255, 255, 255, resMan, map, relive::TBlendModes::eBlend_3, 40); }},
    {"fade", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new Fade(Layer::eLayer_FadeFlash_40, FadeOptions::eFadeOut, false, 2, relive::TBlendModes::eBlend_2, resMan, map); }},
    {"alarm", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new Alarm(300, 0, 5, Layer::eLayer_Above_FG1_39, resMan, map); }},
    {"screen_shake", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new ScreenShake(false, false, resMan, map); }},
    {"air_explosion", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new AirExplosion(x + FP_FromInteger(300), y, scale, false, resMan, map); }},
    {"air_explosion_small", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new AirExplosion(x + FP_FromInteger(300), y, scale, true, resMan, map); }},
    {"ground_explosion", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new GroundExplosion(x + FP_FromInteger(300), y, scale, resMan, map); }},
    {"possession_flicker", [](FP, FP, FP, ResourceManager& resMan, BaseMap& map) { relive_new PossessionFlicker(sControlledCharacter, 90, 100, 100, 100, resMan, map); }},
    {"orb_whirl_wind", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new OrbWhirlWind(x, y, scale, false, resMan, map); }},
    {"orb_whirl_wind_spirit", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new OrbWhirlWind(x, y, scale, true, resMan, map); }},
    {"death_gas", [](FP, FP, FP, ResourceManager& resMan, BaseMap& map) { relive_new DeathGas(Layer::eLayer_Above_FG1_39, 10, resMan, map); }},
    {"throwable_indicator", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new ThrowableTotalIndicator(x, y, Layer::eLayer_Above_FG1_39, scale, 5, true, resMan, map); }},
    {"bullet_shell", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new BulletShell(x, y, false, scale, resMan, map); }},
    {"electrocute", [](FP, FP, FP, ResourceManager& resMan, BaseMap& map) { relive_new Electrocute(sControlledCharacter, true, false, resMan, map); }},
    {"blood", [](FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map) { relive_new Blood(x, y, FP_FromInteger(4), FP_FromInteger(-6), scale, 24, resMan, map); }},
};

bool AutomationSpawn::IsKnown(const std::string& name)
{
    for (const SpawnEntry& entry : kEntries)
    {
        if (name == entry.mName)
        {
            return true;
        }
    }
    return false;
}

std::vector<std::string> AutomationSpawn::Names()
{
    std::vector<std::string> names;
    for (const SpawnEntry& entry : kEntries)
    {
        names.push_back(entry.mName);
    }
    return names;
}

bool AutomationSpawn::Spawn(const std::string& name, ResourceManager& resMan, BaseMap& map)
{
    if (!sControlledCharacter)
    {
        return false;
    }
    for (const SpawnEntry& entry : kEntries)
    {
        if (name == entry.mName)
        {
            entry.mSpawn(sControlledCharacter->mXPos, sControlledCharacter->mYPos - FP_FromInteger(40), sControlledCharacter->GetSpriteScale(), resMan, map);
            return true;
        }
    }
    return false;
}

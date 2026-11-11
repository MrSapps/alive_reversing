#pragma once

#include <nlohmann/json.hpp>
#include "AESaveConverter.hpp"
#include "CommonTypesSerialization.hpp"
#include "../../AliveLibAE/GameSpeak.hpp"
#include "../GameObjects/BaseAliveGameObject.hpp"
#include "../Events.hpp"
#include "../ListOrderSaveState.hpp"
#include "../GameObjects/BoomMachinePipe.hpp"
#include "../GameObjects/RollingBallShaker.hpp"
#include "../GameObjects/ParamiteWeb.hpp"
#include "../../AliveLibAE/InvisibleEffect.hpp"
#include "../GameObjects/CircularFade.hpp"
#include "../GameObjects/ZapLine.hpp"
#include "../../AliveLibAE/Shrykull.hpp"
#include "../GameObjects/ScreenClipper.hpp"
#include "../GameObjects/BirdPortalTerminator.hpp"
#include "../GameObjects/OrbWhirlWind.hpp"
#include "../GameObjects/ThrowableTotalIndicator.hpp"
#include "../GameObjects/DeathGas.hpp"
#include "../GameObjects/PossessionFlicker.hpp"
#include "../../AliveLibAE/ExplosionSet.hpp"
#include "../GameObjects/GroundExplosion.hpp"
#include "../GameObjects/Electrocute.hpp"
#include "../GameObjects/PaletteOverwriter.hpp"
#include "../../AliveLibAE/ColourfulMeter.hpp"
#include "../../AliveLibAE/Bullet.hpp"
#include "../GameObjects/BulletShell.hpp"
#include "../GameObjects/Door.hpp"
#include "../GameObjects/ElectricWall.hpp"
#include "../../AliveLibAE/ParamiteWebLine.hpp"
#include "../../AliveLibAE/PullRingRope.hpp"
#include "../../AliveLibAE/SligGetPantsAndWings.hpp"
#include "../GameObjects/GasCountDown.hpp"
#include "../../AliveLibAE/CrawlingSligButton.hpp"
#include "../../AliveLibAE/BrewMachine.hpp"
#include "../../AliveLibAE/LevelLoader.hpp"
#include "../GameObjects/BackgroundAnimation.hpp"
#include "../../AliveLibAE/GlukkonSwitch.hpp"
#include "../../AliveLibAE/TorturedMudokon.hpp"
#include "../../AliveLibAE/DoorBlocker.hpp"
#include "../../AliveLibAE/DoorFlame.hpp"
#include "../../AliveLibAE/StatusLight.hpp"
#include "../GameObjects/TimedMine.hpp"
#include "../../AliveLibAE/MovingBomb.hpp"
#include "../GameObjects/Mine.hpp"
#include "../../AliveLibAE/BoneBag.hpp"
#include "../../AliveLibAE/RockSack.hpp"
#include "../../AliveLibAE/MeatSack.hpp"
#include "../GameObjects/BoomMachine.hpp"
#include "../GameObjects/ZzzSpawner.hpp"
#include "../../AliveLibAE/SlurgSpawner.hpp"
#include "../../AliveLibAE/SlogSpawner.hpp"
#include "../GameObjects/RollingBallStopper.hpp"
#include "../../AliveLibAE/RollingBall.hpp"
#include "../../AliveLibAE/FootSwitch.hpp"
#include "../../AliveLibAE/Lever.hpp"
#include "../../AliveLibAE/MotionDetector.hpp"
#include "../../AliveLibAE/SecurityDoor.hpp"
#include "../../AliveLibAE/MultiSwitchController.hpp"
#include "../../AliveLibAE/Teleporter.hpp"
#include "../../AliveLibAE/FallingItem.hpp"
#include "../../AliveLibAE/LaughingGas.hpp"
#include "../GameObjects/AirExplosion.hpp"
#include "../GameObjects/Alarm.hpp"
#include "../GameObjects/Blood.hpp"
#include "../GameObjects/DeathBirdParticle.hpp"
#include "../GameObjects/Fade.hpp"
#include "../GameObjects/Flash.hpp"
#include "../GameObjects/Gibs.hpp"
#include "../GameObjects/Particle.hpp"
#include "../GameObjects/ParticleBurst.hpp"
#include "../GameObjects/ScreenShake.hpp"
#include "../GameObjects/Spark.hpp"
#include "../GameObjects/ZapSpark.hpp"
#include "../WorldStatic.hpp"
#include "EnumSerialization.hpp" // needed for some common enum and struct serialization (should probably move those to a different file)


// clang-format off

NLOHMANN_JSON_SERIALIZE_ENUM(LevelIds, {
    {LevelIds::eNone, "none"},
    {LevelIds::eMenu_0, "menu"},
    {LevelIds::eMines_1, "mines"},
    {LevelIds::eNecrum_2, "necrum"},
    {LevelIds::eMudomoVault_3, "mudomo_vault"},
    {LevelIds::eMudancheeVault_4, "mudanchee_vault"},
    {LevelIds::eFeeCoDepot_5, "feeco_depot"},
    {LevelIds::eBarracks_6, "barracks"},
    {LevelIds::eMudancheeVault_Ender_7, "mudanchee_vault_ender"},
    {LevelIds::eBonewerkz_8, "bonewerkz"},
    {LevelIds::eBrewery_9, "brewery"},
    {LevelIds::eBrewery_Ender_10, "brewery_ender"},
    {LevelIds::eMudomoVault_Ender_11, "mudomo_vault_ender"},
    {LevelIds::eFeeCoDepot_Ender_12, "feeco_depot_ender"},
    {LevelIds::eBarracks_Ender_13, "barracks_ender"},
    {LevelIds::eBonewerkz_Ender_14, "bonewerkz_ender"},
    {LevelIds::eTestLevel_15, "test_level"},
    {LevelIds::eCredits_16, "credits"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(Scale, {
    {Scale::Bg, "bg"},
    {Scale::Fg, "fg"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(SligSpeak, {
    {SligSpeak::eNone, "none"},
    {SligSpeak::eHi_0, "hi"},
    {SligSpeak::eHereBoy_1, "here_boy"},
    {SligSpeak::eGetHim_2, "get_him"},
    {SligSpeak::eLaugh_3, "laugh"},
    {SligSpeak::eStay_4, "stay"},
    {SligSpeak::eBullshit_5, "bullshit"},
    {SligSpeak::eLookOut_6, "look_out"},
    {SligSpeak::eBullshit2_7, "bullshit2"},
    {SligSpeak::eFreeze_8, "freeze"},
    {SligSpeak::eWhat_9, "what"},
    {SligSpeak::eHelp_10, "help"},
    {SligSpeak::eGotYa_12, "got_ya"},
    {SligSpeak::eBlurgh_11, "blurgh"},
    {SligSpeak::eOuch1_13, "ouch_1"},
    {SligSpeak::eOuch2_14, "ouch_2"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(SpawnerStates, {
    {SpawnerStates::eInactive_0, "inactive"},
    {SpawnerStates::eSligSpawned_1, "slig_spawned"},
})

inline void to_json(nlohmann::json& j, const AnimationSaveState& p)
{
    j = nlohmann::json{
        {"valid", p.mValid},
        {"frame", p.mFrame},
        {"frame_change_counter", p.mFrameChangeCounter},
        {"frame_delay", p.mFrameDelay},
        {"animate", p.mAnimate},
        {"loop", p.mLoop},
        {"flip_x", p.mFlipX},
        {"render", p.mRender},
        {"is_last_frame", p.mIsLastFrame},
        {"forward_loop_completed", p.mForwardLoopCompleted},
        {"flip_y", p.mFlipY},
        {"swap_xy", p.mSwapXY},
        {"semi_trans", p.mSemiTrans},
        {"blending", p.mBlending},
        {"loop_backwards", p.mLoopBackwards},
        {"ignore_pos_offset", p.mIgnorePosOffset},
        {"blend_mode", p.mBlendMode},
        {"render_layer", p.mRenderLayer},
        {"red", p.mRed},
        {"green", p.mGreen},
        {"blue", p.mBlue},
        {"sprite_scale", p.mSpriteScale},
        {"anim_id", p.mAnimId},
    };
}

inline void from_json(const nlohmann::json& j, AnimationSaveState& p)
{
    j.at("valid").get_to(p.mValid);
    j.at("frame").get_to(p.mFrame);
    j.at("frame_change_counter").get_to(p.mFrameChangeCounter);
    j.at("frame_delay").get_to(p.mFrameDelay);
    j.at("animate").get_to(p.mAnimate);
    j.at("loop").get_to(p.mLoop);
    j.at("flip_x").get_to(p.mFlipX);
    j.at("render").get_to(p.mRender);
    j.at("is_last_frame").get_to(p.mIsLastFrame);
    j.at("forward_loop_completed").get_to(p.mForwardLoopCompleted);
    // A save from before these were kept has none, and its animation is as the object makes it
    if (!j.contains("blend_mode"))
    {
        p.mValid = false;
        return;
    }
    j.at("flip_y").get_to(p.mFlipY);
    j.at("swap_xy").get_to(p.mSwapXY);
    j.at("semi_trans").get_to(p.mSemiTrans);
    j.at("blending").get_to(p.mBlending);
    j.at("loop_backwards").get_to(p.mLoopBackwards);
    j.at("ignore_pos_offset").get_to(p.mIgnorePosOffset);
    j.at("blend_mode").get_to(p.mBlendMode);
    j.at("render_layer").get_to(p.mRenderLayer);
    j.at("red").get_to(p.mRed);
    j.at("green").get_to(p.mGreen);
    j.at("blue").get_to(p.mBlue);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("anim_id").get_to(p.mAnimId);
}

inline void to_json(nlohmann::json& j, const HoistRocksEffectSaveState& p)
{
    nlohmann::json rocks = nlohmann::json::array();
    for (const auto& rock : p.mRocks)
    {
        rocks.push_back(nlohmann::json{
            {"state", rock.mState},
            {"xpos", rock.mXPos},
            {"ypos", rock.mYPos},
            {"vely", rock.mVelY},
            {"anim_id", static_cast<s32>(rock.mAnimId)},
            {"anim", rock.mAnim},
        });
    }
    j = nlohmann::json{
        {"type", p.mType},
        {"timer", p.mTimer},
        {"rocks", rocks},
    };
}

inline void from_json(const nlohmann::json& j, HoistRocksEffectSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("timer").get_to(p.mTimer);
    for (size_t i = 0; i < 4; i++)
    {
        const nlohmann::json& rock = j.at("rocks").at(i);
        rock.at("state").get_to(p.mRocks[i].mState);
        rock.at("xpos").get_to(p.mRocks[i].mXPos);
        rock.at("ypos").get_to(p.mRocks[i].mYPos);
        rock.at("vely").get_to(p.mRocks[i].mVelY);
        p.mRocks[i].mAnimId = static_cast<AnimId>(rock.at("anim_id").get<s32>());
        rock.at("anim").get_to(p.mRocks[i].mAnim);
    }
}

inline void to_json(nlohmann::json& j, const InvisibleSwitchSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"tlv_id", p.mTlvId},
        {"delay_timer", p.mDelayTimer},
        {"state", p.mState},
    };
}

inline void from_json(const nlohmann::json& j, InvisibleSwitchSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("tlv_id").get_to(p.mTlvId);
    j.at("delay_timer").get_to(p.mDelayTimer);
    j.at("state").get_to(p.mState);
}

inline void to_json(nlohmann::json& j, const MusicTriggerSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"tlv_id", p.mTlvId},
        {"triggered_by_touching", p.mTriggeredByTouching},
        {"triggered_by_timer", p.mTriggeredByTimer},
        {"counter", p.mCounter},
        {"music_type", p.mMusicType},
        {"set_music_to_none_on_dtor", p.mSetMusicToNoneOnDtor},
        {"update_delay", p.mUpdateDelay},
    };
}

inline void from_json(const nlohmann::json& j, MusicTriggerSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("tlv_id").get_to(p.mTlvId);
    j.at("triggered_by_touching").get_to(p.mTriggeredByTouching);
    j.at("triggered_by_timer").get_to(p.mTriggeredByTimer);
    j.at("counter").get_to(p.mCounter);
    p.mMusicType = j.value("music_type", 0);
    p.mSetMusicToNoneOnDtor = j.value("set_music_to_none_on_dtor", false);
    p.mUpdateDelay = j.value("update_delay", 0);
}

inline void to_json(nlohmann::json& j, const SecurityOrbSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"tlv_id", p.mTlvId},
        {"state", p.mState},
        {"timer", p.mTimer},
        {"health", p.mHealth},
    };
}

inline void from_json(const nlohmann::json& j, SecurityOrbSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("tlv_id").get_to(p.mTlvId);
    j.at("state").get_to(p.mState);
    j.at("timer").get_to(p.mTimer);
    j.at("health").get_to(p.mHealth);
}

inline void to_json(nlohmann::json& j, const GasEmitterSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"tlv_id", p.mTlvId},
        {"emit_power", p.mEmitPower},
        {"draw_flipper", p.mDrawFlipper},
    };
}

inline void from_json(const nlohmann::json& j, GasEmitterSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("tlv_id").get_to(p.mTlvId);
    j.at("emit_power").get_to(p.mEmitPower);
    j.at("draw_flipper").get_to(p.mDrawFlipper);
}

inline void to_json(nlohmann::json& j, const LeafSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"velx", p.mVelX},
        {"vely", p.mVelY},
        {"sprite_scale", p.mSpriteScale},
        {"hit_something", p.mHitSomething},
    };
}

inline void from_json(const nlohmann::json& j, LeafSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("velx").get_to(p.mVelX);
    j.at("vely").get_to(p.mVelY);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("hit_something").get_to(p.mHitSomething);
}

inline void to_json(nlohmann::json& j, const AnimatedObjectSaveState& p)
{
    j = nlohmann::json{
        {"anim", p.mAnim},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"path", p.mCurrentPath},
        {"level", p.mCurrentLevel},
        {"vel_x", p.mVelX},
        {"vel_y", p.mVelY},
        {"red", p.mRed},
        {"green", p.mGreen},
        {"blue", p.mBlue},
        {"y_offset", p.mYOffset},
        {"x_offset", p.mXOffset},
        {"scale", p.mScale},
        {"sprite_scale", p.mSpriteScale},
        {"apply_shadow_zone_colour", p.mApplyShadowZoneColour},
        {"do_purple_light_effect", p.mDoPurpleLightEffect},
    };
}

inline void from_json(const nlohmann::json& j, AnimatedObjectSaveState& p)
{
    j.at("anim").get_to(p.mAnim);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("path").get_to(p.mCurrentPath);
    j.at("level").get_to(p.mCurrentLevel);
    j.at("vel_x").get_to(p.mVelX);
    j.at("vel_y").get_to(p.mVelY);
    j.at("red").get_to(p.mRed);
    j.at("green").get_to(p.mGreen);
    j.at("blue").get_to(p.mBlue);
    j.at("y_offset").get_to(p.mYOffset);
    j.at("x_offset").get_to(p.mXOffset);
    j.at("scale").get_to(p.mScale);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("apply_shadow_zone_colour").get_to(p.mApplyShadowZoneColour);
    j.at("do_purple_light_effect").get_to(p.mDoPurpleLightEffect);
}

inline void to_json(nlohmann::json& j, const EffectBaseSaveState& p)
{
    j = nlohmann::json{
        {"blend_mode", p.mBlendMode},
        {"layer", p.mLayer},
        {"path", p.mPathId},
        {"level", p.mLevelId},
        {"red", p.mRed},
        {"green", p.mGreen},
        {"blue", p.mBlue},
        {"semi_trans", p.mSemiTrans},
    };
}

inline void from_json(const nlohmann::json& j, EffectBaseSaveState& p)
{
    j.at("blend_mode").get_to(p.mBlendMode);
    j.at("layer").get_to(p.mLayer);
    j.at("path").get_to(p.mPathId);
    j.at("level").get_to(p.mLevelId);
    j.at("red").get_to(p.mRed);
    j.at("green").get_to(p.mGreen);
    j.at("blue").get_to(p.mBlue);
    j.at("semi_trans").get_to(p.mSemiTrans);
}

inline void to_json(nlohmann::json& j, const SparkSaveState& p)
{
    nlohmann::json sparks = nlohmann::json::array();
    for (s32 i = 0; i < p.mSparkAllocCount && i < 16; i++)
    {
        const auto& spark = p.mSparks[i];
        sparks.push_back(nlohmann::json{{"x0", spark.mX0}, {"y0", spark.mY0}, {"x1", spark.mX1}, {"y1", spark.mY1}, {"ang", spark.mAng}, {"radius", spark.mRadius}, {"len", spark.mLen}});
    }
    j = nlohmann::json{
        {"type", p.mType},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"sprite_scale", p.mSpriteScale},
        {"red", p.mRed},
        {"green", p.mGreen},
        {"blue", p.mBlue},
        {"layer", p.mLayer},
        {"spark_count", p.mSparkCount},
        {"spark_alloc_count", p.mSparkAllocCount},
        {"timer", p.mTimer},
        {"spark_type", p.mSparkType},
        {"sparks", sparks},
    };
}

inline void from_json(const nlohmann::json& j, SparkSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("red").get_to(p.mRed);
    j.at("green").get_to(p.mGreen);
    j.at("blue").get_to(p.mBlue);
    j.at("layer").get_to(p.mLayer);
    j.at("spark_count").get_to(p.mSparkCount);
    j.at("spark_alloc_count").get_to(p.mSparkAllocCount);
    j.at("timer").get_to(p.mTimer);
    j.at("spark_type").get_to(p.mSparkType);
    const nlohmann::json& sparks = j.at("sparks");
    for (size_t i = 0; i < sparks.size() && i < 16; i++)
    {
        const nlohmann::json& spark = sparks.at(i);
        spark.at("x0").get_to(p.mSparks[i].mX0);
        spark.at("y0").get_to(p.mSparks[i].mY0);
        spark.at("x1").get_to(p.mSparks[i].mX1);
        spark.at("y1").get_to(p.mSparks[i].mY1);
        spark.at("ang").get_to(p.mSparks[i].mAng);
        spark.at("radius").get_to(p.mSparks[i].mRadius);
        spark.at("len").get_to(p.mSparks[i].mLen);
    }
}

inline void to_json(nlohmann::json& j, const ParticleSaveState& p)
{
    j = nlohmann::json{{"type", p.mType}, {"object_type", p.mObjectType}, {"object", p.mObject}, {"scale_amount", p.mScaleAmount}};
}

inline void from_json(const nlohmann::json& j, ParticleSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("object_type").get_to(p.mObjectType);
    j.at("object").get_to(p.mObject);
    j.at("scale_amount").get_to(p.mScaleAmount);
}

inline void to_json(nlohmann::json& j, const GibsSaveState& p)
{
    nlohmann::json parts = nlohmann::json::array();
    for (s32 i = 0; i < p.mPartsUsedCount && i < 7; i++)
    {
        const auto& part = p.mParts[i];
        parts.push_back(nlohmann::json{{"x", part.mX}, {"y", part.mY}, {"z", part.mZ}, {"dx", part.mDx}, {"dy", part.mDy}, {"dz", part.mDz}, {"anim", part.mAnim}});
    }
    j = nlohmann::json{
        {"type", p.mType},
        {"gib_type", p.mGibType},
        {"base_scale", p.mBaseScale},
        {"make_smaller", p.mMakeSmaller},
        {"object", p.mObject},
        {"z", p.mZ},
        {"dz", p.mDz},
        {"alive_timer", p.mAliveTimer},
        {"parts_used_count", p.mPartsUsedCount},
        {"parts", parts},
    };
}

inline void from_json(const nlohmann::json& j, GibsSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("gib_type").get_to(p.mGibType);
    j.at("base_scale").get_to(p.mBaseScale);
    j.at("make_smaller").get_to(p.mMakeSmaller);
    j.at("object").get_to(p.mObject);
    j.at("z").get_to(p.mZ);
    j.at("dz").get_to(p.mDz);
    j.at("alive_timer").get_to(p.mAliveTimer);
    j.at("parts_used_count").get_to(p.mPartsUsedCount);
    const nlohmann::json& parts = j.at("parts");
    for (size_t i = 0; i < parts.size() && i < 7; i++)
    {
        const nlohmann::json& part = parts.at(i);
        part.at("x").get_to(p.mParts[i].mX);
        part.at("y").get_to(p.mParts[i].mY);
        part.at("z").get_to(p.mParts[i].mZ);
        part.at("dx").get_to(p.mParts[i].mDx);
        part.at("dy").get_to(p.mParts[i].mDy);
        part.at("dz").get_to(p.mParts[i].mDz);
        part.at("anim").get_to(p.mParts[i].mAnim);
    }
}

inline void to_json(nlohmann::json& j, const ParticleBurstSaveState& p)
{
    nlohmann::json particles = nlohmann::json::array();
    for (s32 i = 0; i < p.mParticleCount && i < 40; i++)
    {
        const auto& particle = p.mParticles[i];
        particles.push_back(nlohmann::json{{"x", particle.mX}, {"y", particle.mY}, {"z", particle.mZ}, {"x_speed", particle.mXSpeed}, {"y_speed", particle.mYSpeed}, {"z_speed", particle.mZSpeed}, {"anim", particle.mAnim}});
    }
    j = nlohmann::json{
        {"type", p.mType},
        {"requested_count", p.mRequestedCount},
        {"burst_type", p.mBurstType},
        {"fine_scale", p.mFineScale},
        {"fadeout", p.mFadeout},
        {"scale", p.mScale},
        {"object", p.mObject},
        {"particle_count", p.mParticleCount},
        {"alive_timer", p.mAliveTimer},
        {"particles", particles},
    };
}

inline void from_json(const nlohmann::json& j, ParticleBurstSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("requested_count").get_to(p.mRequestedCount);
    j.at("burst_type").get_to(p.mBurstType);
    j.at("fine_scale").get_to(p.mFineScale);
    j.at("fadeout").get_to(p.mFadeout);
    j.at("scale").get_to(p.mScale);
    j.at("object").get_to(p.mObject);
    j.at("particle_count").get_to(p.mParticleCount);
    j.at("alive_timer").get_to(p.mAliveTimer);
    const nlohmann::json& particles = j.at("particles");
    for (size_t i = 0; i < particles.size() && i < 40; i++)
    {
        const nlohmann::json& particle = particles.at(i);
        particle.at("x").get_to(p.mParticles[i].mX);
        particle.at("y").get_to(p.mParticles[i].mY);
        particle.at("z").get_to(p.mParticles[i].mZ);
        particle.at("x_speed").get_to(p.mParticles[i].mXSpeed);
        particle.at("y_speed").get_to(p.mParticles[i].mYSpeed);
        particle.at("z_speed").get_to(p.mParticles[i].mZSpeed);
        particle.at("anim").get_to(p.mParticles[i].mAnim);
    }
}

inline void to_json(nlohmann::json& j, const BloodSaveState& p)
{
    nlohmann::json drops = nlohmann::json::array();
    for (s32 i = 0; i < p.mCurrentBloodCount && i < 64; i++)
    {
        const auto& drop = p.mDrops[i];
        drops.push_back(nlohmann::json{{"x", drop.mX}, {"y", drop.mY}, {"off_x", drop.mOffX}, {"off_y", drop.mOffY}});
    }
    j = nlohmann::json{
        {"type", p.mType},
        {"total_blood_count", p.mTotalBloodCount},
        {"scale", p.mScale},
        {"object", p.mObject},
        {"blood_xpos", p.mBloodXPos},
        {"blood_ypos", p.mBloodYPos},
        {"rand_seed", p.mRandSeed},
        {"current_blood_count", p.mCurrentBloodCount},
        {"update_calls", p.mUpdateCalls},
        {"ot_layer", p.mOtLayer},
        {"drops", drops},
    };
}

inline void from_json(const nlohmann::json& j, BloodSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("total_blood_count").get_to(p.mTotalBloodCount);
    j.at("scale").get_to(p.mScale);
    j.at("object").get_to(p.mObject);
    j.at("blood_xpos").get_to(p.mBloodXPos);
    j.at("blood_ypos").get_to(p.mBloodYPos);
    j.at("rand_seed").get_to(p.mRandSeed);
    j.at("current_blood_count").get_to(p.mCurrentBloodCount);
    j.at("update_calls").get_to(p.mUpdateCalls);
    j.at("ot_layer").get_to(p.mOtLayer);
    const nlohmann::json& drops = j.at("drops");
    for (size_t i = 0; i < drops.size() && i < 64; i++)
    {
        const nlohmann::json& drop = drops.at(i);
        drop.at("x").get_to(p.mDrops[i].mX);
        drop.at("y").get_to(p.mDrops[i].mY);
        drop.at("off_x").get_to(p.mDrops[i].mOffX);
        drop.at("off_y").get_to(p.mDrops[i].mOffY);
    }
}

inline void to_json(nlohmann::json& j, const DeathBirdParticleSaveState& p)
{
    j = nlohmann::json{{"type", p.mType}, {"start_timer", p.mStartTimer}, {"play_sound", p.mPlaySound}, {"scale", p.mScale}, {"object", p.mObject}, {"random", p.mRandom}, {"state", p.mState}};
}

inline void from_json(const nlohmann::json& j, DeathBirdParticleSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("start_timer").get_to(p.mStartTimer);
    j.at("play_sound").get_to(p.mPlaySound);
    j.at("scale").get_to(p.mScale);
    j.at("object").get_to(p.mObject);
    j.at("random").get_to(p.mRandom);
    j.at("state").get_to(p.mState);
}

inline void to_json(nlohmann::json& j, const ZapSparkSaveState& p)
{
    j = nlohmann::json{{"type", p.mType}, {"object", p.mObject}, {"spark_timer", p.mSparkTimer}};
}

inline void from_json(const nlohmann::json& j, ZapSparkSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("object").get_to(p.mObject);
    j.at("spark_timer").get_to(p.mSparkTimer);
}

inline void to_json(nlohmann::json& j, const FlashSaveState& p)
{
    j = nlohmann::json{{"type", p.mType}, {"effect", p.mEffect}, {"flash_time", p.mFlashTime}};
}

inline void from_json(const nlohmann::json& j, FlashSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("effect").get_to(p.mEffect);
    j.at("flash_time").get_to(p.mFlashTime);
}

inline void to_json(nlohmann::json& j, const FadeSaveState& p)
{
    j = nlohmann::json{{"type", p.mType}, {"effect", p.mEffect}, {"fade_option", p.mFadeOption}, {"destroy_on_done", p.mDestroyOnDone}, {"speed", p.mSpeed}, {"fade_colour", p.mFadeColour}, {"done", p.mDone}};
}

inline void from_json(const nlohmann::json& j, FadeSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("effect").get_to(p.mEffect);
    j.at("fade_option").get_to(p.mFadeOption);
    j.at("destroy_on_done").get_to(p.mDestroyOnDone);
    j.at("speed").get_to(p.mSpeed);
    j.at("fade_colour").get_to(p.mFadeColour);
    j.at("done").get_to(p.mDone);
}

inline void to_json(nlohmann::json& j, const AlarmSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"effect", p.mEffect},
        {"is_tlv_alarm", p.mIsTlvAlarm},
        {"tlv_info", p.mTlvInfo},
        {"alarm_red", p.mAlarmRed},
        {"alarm_pause_timer", p.mAlarmPauseTimer},
        {"alarm_duration_timer", p.mAlarmDurationTimer},
        {"alarm_state", p.mAlarmState},
        {"alarm_switch_id", p.mAlarmSwitchId},
        {"alarm_duration", p.mAlarmDuration},
        {"drawable", p.mDrawable},
    };
}

inline void from_json(const nlohmann::json& j, AlarmSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("effect").get_to(p.mEffect);
    j.at("is_tlv_alarm").get_to(p.mIsTlvAlarm);
    j.at("tlv_info").get_to(p.mTlvInfo);
    j.at("alarm_red").get_to(p.mAlarmRed);
    j.at("alarm_pause_timer").get_to(p.mAlarmPauseTimer);
    j.at("alarm_duration_timer").get_to(p.mAlarmDurationTimer);
    j.at("alarm_state").get_to(p.mAlarmState);
    j.at("alarm_switch_id").get_to(p.mAlarmSwitchId);
    j.at("alarm_duration").get_to(p.mAlarmDuration);
    j.at("drawable").get_to(p.mDrawable);
}

inline void to_json(nlohmann::json& j, const ScreenShakeSaveState& p)
{
    j = nlohmann::json{{"type", p.mType}, {"enable_shake_event", p.mEnableShakeEvent}, {"softer_shakes", p.mSofterShakes}, {"shake_number", p.mShakeNumber}};
}

inline void from_json(const nlohmann::json& j, ScreenShakeSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("enable_shake_event").get_to(p.mEnableShakeEvent);
    j.at("softer_shakes").get_to(p.mSofterShakes);
    j.at("shake_number").get_to(p.mShakeNumber);
}

inline void to_json(nlohmann::json& j, const AirExplosionSaveState& p)
{
    j = nlohmann::json{{"type", p.mType}, {"small_explosion", p.mSmallExplosion}, {"object", p.mObject}, {"explosion_scale", p.mExplosionScale}, {"particle_burst_scale", p.mParticleBurstScale}};
}

inline void from_json(const nlohmann::json& j, AirExplosionSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("small_explosion").get_to(p.mSmallExplosion);
    j.at("object").get_to(p.mObject);
    j.at("explosion_scale").get_to(p.mExplosionScale);
    j.at("particle_burst_scale").get_to(p.mParticleBurstScale);
}

inline void to_json(nlohmann::json& j, const LaughingGasSaveState& p)
{
    nlohmann::json gasX = nlohmann::json::array();
    nlohmann::json gasY = nlohmann::json::array();
    for (s32 i = 0; i < 6; i++)
    {
        for (s32 k = 0; k < 6; k++)
        {
            gasX.push_back(p.mGasX[i][k]);
            gasY.push_back(p.mGasY[i][k]);
        }
    }
    j = nlohmann::json{{"type", p.mType}, {"amount_on", p.mAmountOn}, {"gas_x", gasX}, {"gas_y", gasY}, {"random_seed", p.mRandomSeed}};
}

inline void from_json(const nlohmann::json& j, LaughingGasSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("amount_on").get_to(p.mAmountOn);
    const nlohmann::json& gasX = j.at("gas_x");
    const nlohmann::json& gasY = j.at("gas_y");
    for (s32 i = 0; i < 36 && i < static_cast<s32>(gasX.size()); i++)
    {
        p.mGasX[i / 6][i % 6] = gasX.at(i).get<f32>();
        p.mGasY[i / 6][i % 6] = gasY.at(i).get<f32>();
    }
    j.at("random_seed").get_to(p.mRandomSeed);
}

inline void to_json(nlohmann::json& j, const WorldStaticsSaveState& p)
{
    nlohmann::json statics = nlohmann::json::array();
    for (u32 i = 0; i < p.mCount && i < WorldStaticsSaveState::kMaxStatics; i++)
    {
        nlohmann::json data = nlohmann::json::array();
        for (u32 b = 0; b < p.mStatics[i].mSize && b < WorldStaticsSaveState::kMaxSize; b++)
        {
            data.push_back(p.mStatics[i].mData[b]);
        }
        statics.push_back(nlohmann::json{{"name_hash", p.mStatics[i].mNameHash}, {"data", data}});
    }
    j = nlohmann::json{{"type", p.mType}, {"statics", statics}};
}

inline void from_json(const nlohmann::json& j, WorldStaticsSaveState& p)
{
    j.at("type").get_to(p.mType);
    const nlohmann::json& statics = j.at("statics");
    p.mCount = 0;
    for (size_t i = 0; i < statics.size() && i < WorldStaticsSaveState::kMaxStatics; i++)
    {
        const nlohmann::json& entry = statics.at(i);
        entry.at("name_hash").get_to(p.mStatics[i].mNameHash);
        const nlohmann::json& data = entry.at("data");
        p.mStatics[i].mSize = static_cast<u32>(data.size());
        for (size_t b = 0; b < data.size() && b < WorldStaticsSaveState::kMaxSize; b++)
        {
            p.mStatics[i].mData[b] = data.at(b).get<u8>();
        }
        p.mCount++;
    }
}

inline void to_json(nlohmann::json& j, const FallingItemSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"object", p.mObject},
        {"health", p.mHealth},
        {"state", p.mState},
        {"switch_id", p.mSwitchId},
        {"fall_interval", p.mFallInterval},
        {"max_falling_items", p.mMaxFallingItems},
        {"remaining_falling_items", p.mRemainingFallingItems},
        {"reset_switch_id_after_use", p.mResetSwitchIdAfterUse},
        {"tlv_x_pos", p.mTlvXPos},
        {"tlv_y_pos", p.mTlvYPos},
        {"start_y_pos", p.mStartYPos},
        {"fall_interval_timer", p.mFallIntervalTimer},
        {"do_air_stream_sound", p.mDoAirStreamSound},
        {"hit_drill_or_mine_car", p.mHitDrillOrMineCar},
        {"air_stream_snd_channels", p.mAirStreamSndChannels},
        {"created_gn_frame", p.mCreatedGnFrame},
    };
}

inline void from_json(const nlohmann::json& j, FallingItemSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("object").get_to(p.mObject);
    j.at("health").get_to(p.mHealth);
    j.at("state").get_to(p.mState);
    j.at("switch_id").get_to(p.mSwitchId);
    j.at("fall_interval").get_to(p.mFallInterval);
    j.at("max_falling_items").get_to(p.mMaxFallingItems);
    j.at("remaining_falling_items").get_to(p.mRemainingFallingItems);
    j.at("reset_switch_id_after_use").get_to(p.mResetSwitchIdAfterUse);
    j.at("tlv_x_pos").get_to(p.mTlvXPos);
    j.at("tlv_y_pos").get_to(p.mTlvYPos);
    j.at("start_y_pos").get_to(p.mStartYPos);
    j.at("fall_interval_timer").get_to(p.mFallIntervalTimer);
    j.at("do_air_stream_sound").get_to(p.mDoAirStreamSound);
    j.at("hit_drill_or_mine_car").get_to(p.mHitDrillOrMineCar);
    j.at("air_stream_snd_channels").get_to(p.mAirStreamSndChannels);
    j.at("created_gn_frame").get_to(p.mCreatedGnFrame);
}

inline void to_json(nlohmann::json& j, const TeleporterSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"switch_state", p.mSwitchState},
        {"state", p.mState},
        {"destroy_self", p.mDestroySelf},
        {"electrocute_id", p.mElectrocuteId},
        {"effects_created", p.mEffectsCreated},
    };
}

inline void from_json(const nlohmann::json& j, TeleporterSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("switch_state").get_to(p.mSwitchState);
    j.at("state").get_to(p.mState);
    j.at("destroy_self").get_to(p.mDestroySelf);
    j.at("electrocute_id").get_to(p.mElectrocuteId);
    j.at("effects_created").get_to(p.mEffectsCreated);
}

inline void to_json(nlohmann::json& j, const MultiSwitchControllerSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"last_switch_on_time", p.mLastSwitchOnTime},
        {"all_switches_on_or_off_time", p.mAllSwitchesOnOrOffTime},
        {"is_on", p.mIsOn},
    };
}

inline void from_json(const nlohmann::json& j, MultiSwitchControllerSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("last_switch_on_time").get_to(p.mLastSwitchOnTime);
    j.at("all_switches_on_or_off_time").get_to(p.mAllSwitchesOnOrOffTime);
    j.at("is_on").get_to(p.mIsOn);
}

inline void to_json(nlohmann::json& j, const SecurityDoorSaveState& p)
{
    nlohmann::json password_buffer_array = nlohmann::json::array();
    for (s32 i = 0; i < 16; i++)
    {
        password_buffer_array.push_back(p.mPasswordBuffer[i]);
    }
    j = nlohmann::json{
        {"type", p.mType},
        {"state", p.mState},
        {"code_length", p.mCodeLength},
        {"max_idx", p.mMaxIdx},
        {"buffer_start_idx", p.mBufferStartIdx},
        {"timer", p.mTimer},
        {"max_idx2", p.mMaxIdx2},
        {"password_buffer", password_buffer_array},
    };
}

inline void from_json(const nlohmann::json& j, SecurityDoorSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("state").get_to(p.mState);
    j.at("code_length").get_to(p.mCodeLength);
    j.at("max_idx").get_to(p.mMaxIdx);
    j.at("buffer_start_idx").get_to(p.mBufferStartIdx);
    j.at("timer").get_to(p.mTimer);
    j.at("max_idx2").get_to(p.mMaxIdx2);
    const nlohmann::json& password_buffer_array = j.at("password_buffer");
    for (s32 i = 0; i < 16 && i < static_cast<s32>(password_buffer_array.size()); i++)
    {
        password_buffer_array.at(i).get_to(p.mPasswordBuffer[i]);
    }
}

inline void to_json(nlohmann::json& j, const MotionDetectorSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"laser_id", p.mLaserId},
        {"owner_id", p.mOwnerId},
        {"state", p.mState},
        {"pause_timer", p.mPauseTimer},
        {"has_owner", p.mHasOwner},
        {"dont_come_back", p.mDontComeBack},
        {"top_left_x", p.mTopLeftX},
        {"top_left_y", p.mTopLeftY},
        {"bottom_right_x", p.mBottomRightX},
        {"bottom_right_y", p.mBottomRightY},
        {"speed", p.mSpeed},
        {"object_in_laser", p.mObjectInLaser},
    };
}

inline void from_json(const nlohmann::json& j, MotionDetectorSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("laser_id").get_to(p.mLaserId);
    j.at("owner_id").get_to(p.mOwnerId);
    j.at("state").get_to(p.mState);
    j.at("pause_timer").get_to(p.mPauseTimer);
    j.at("has_owner").get_to(p.mHasOwner);
    j.at("dont_come_back").get_to(p.mDontComeBack);
    j.at("top_left_x").get_to(p.mTopLeftX);
    j.at("top_left_y").get_to(p.mTopLeftY);
    j.at("bottom_right_x").get_to(p.mBottomRightX);
    j.at("bottom_right_y").get_to(p.mBottomRightY);
    j.at("speed").get_to(p.mSpeed);
    j.at("object_in_laser").get_to(p.mObjectInLaser);
}

inline void to_json(nlohmann::json& j, const LeverSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"state", p.mState},
        {"pulled_from_left", p.mPulledFromLeft},
    };
}

inline void from_json(const nlohmann::json& j, LeverSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("state").get_to(p.mState);
    j.at("pulled_from_left").get_to(p.mPulledFromLeft);
}

inline void to_json(nlohmann::json& j, const FootSwitchSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"state", p.mState},
        {"stood_on_me_id", p.mStoodOnMeId},
        {"create_sparks", p.mCreateSparks},
        {"find_stander", p.mFindStander},
    };
}

inline void from_json(const nlohmann::json& j, FootSwitchSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("state").get_to(p.mState);
    j.at("stood_on_me_id").get_to(p.mStoodOnMeId);
    j.at("create_sparks").get_to(p.mCreateSparks);
    j.at("find_stander").get_to(p.mFindStander);
}

inline void to_json(nlohmann::json& j, const RollingBallSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"state", p.mState},
        {"rolling_ball_shaker_id", p.mRollingBallShakerId},
        {"max_speed", p.mMaxSpeed},
        {"acceleration", p.mAcceleration},
    };
}

inline void from_json(const nlohmann::json& j, RollingBallSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("state").get_to(p.mState);
    j.at("rolling_ball_shaker_id").get_to(p.mRollingBallShakerId);
    j.at("max_speed").get_to(p.mMaxSpeed);
    j.at("acceleration").get_to(p.mAcceleration);
}

inline void to_json(nlohmann::json& j, const RollingBallStopperSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"state", p.mState},
    };
}

inline void from_json(const nlohmann::json& j, RollingBallStopperSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("state").get_to(p.mState);
}

inline void to_json(nlohmann::json& j, const SlogSpawnerSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"spawned_slogs_count", p.mSpawnedSlogsCount},
        {"spawn_timer", p.mSpawnTimer},
    };
}

inline void from_json(const nlohmann::json& j, SlogSpawnerSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("spawned_slogs_count").get_to(p.mSpawnedSlogsCount);
    j.at("spawn_timer").get_to(p.mSpawnTimer);
}

inline void to_json(nlohmann::json& j, const SlurgSpawnerSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"spawn_counter", p.mSpawnCounter},
        {"spawned_count", p.mSpawnedCount},
    };
}

inline void from_json(const nlohmann::json& j, SlurgSpawnerSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("spawn_counter").get_to(p.mSpawnCounter);
    j.at("spawned_count").get_to(p.mSpawnedCount);
}

inline void to_json(nlohmann::json& j, const StatusLightSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"untweaked_x_pos", p.mUntweakedXPos},
        {"untweaked_y_pos", p.mUntweakedYPos},
    };
}

inline void from_json(const nlohmann::json& j, StatusLightSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("untweaked_x_pos").get_to(p.mUntweakedXPos);
    j.at("untweaked_y_pos").get_to(p.mUntweakedYPos);
}

inline void to_json(nlohmann::json& j, const ZzzSpawnerSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"timer", p.mTimer},
    };
}

inline void from_json(const nlohmann::json& j, ZzzSpawnerSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("timer").get_to(p.mTimer);
}

inline void to_json(nlohmann::json& j, const BoomMachineSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"pipe_id", p.mPipeId},
        {"is_button_on", p.mIsButtonOn},
    };
}

inline void from_json(const nlohmann::json& j, BoomMachineSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("pipe_id").get_to(p.mPipeId);
    j.at("is_button_on").get_to(p.mIsButtonOn);
}

inline void to_json(nlohmann::json& j, const MeatSackSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"has_been_hit", p.mHasBeenHit},
        {"meat_amount", p.mMeatAmount},
        {"play_wobble_sound", p.mPlayWobbleSound},
    };
}

inline void from_json(const nlohmann::json& j, MeatSackSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("has_been_hit").get_to(p.mHasBeenHit);
    j.at("meat_amount").get_to(p.mMeatAmount);
    j.at("play_wobble_sound").get_to(p.mPlayWobbleSound);
}

inline void to_json(nlohmann::json& j, const RockSackSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"has_been_hit", p.mHasBeenHit},
        {"rock_amount", p.mRockAmount},
        {"play_wobble_sound", p.mPlayWobbleSound},
        {"force_wobble_sound", p.mForceWobbleSound},
    };
}

inline void from_json(const nlohmann::json& j, RockSackSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("has_been_hit").get_to(p.mHasBeenHit);
    j.at("rock_amount").get_to(p.mRockAmount);
    j.at("play_wobble_sound").get_to(p.mPlayWobbleSound);
    j.at("force_wobble_sound").get_to(p.mForceWobbleSound);
}

inline void to_json(nlohmann::json& j, const BoneBagSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"has_been_hit", p.mHasBeenHit},
        {"play_wobble_sound", p.mPlayWobbleSound},
        {"force_play_wobble_sound", p.mForcePlayWobbleSound},
        {"bone_amount", p.mBoneAmount},
    };
}

inline void from_json(const nlohmann::json& j, BoneBagSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("has_been_hit").get_to(p.mHasBeenHit);
    j.at("play_wobble_sound").get_to(p.mPlayWobbleSound);
    j.at("force_play_wobble_sound").get_to(p.mForcePlayWobbleSound);
    j.at("bone_amount").get_to(p.mBoneAmount);
}

inline void to_json(nlohmann::json& j, const MineSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"detonating", p.mDetonating},
        {"explosion_timer", p.mExplosionTimer},
        {"persist_offscreen", p.mPersistOffscreen},
        {"flash_anim", p.mFlashAnim},
    };
}

inline void from_json(const nlohmann::json& j, MineSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("detonating").get_to(p.mDetonating);
    j.at("explosion_timer").get_to(p.mExplosionTimer);
    j.at("persist_offscreen").get_to(p.mPersistOffscreen);
    j.at("flash_anim").get_to(p.mFlashAnim);
}

inline void to_json(nlohmann::json& j, const MovingBombSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"state", p.mState},
        {"timer", p.mTimer},
        {"speed", p.mSpeed},
        {"channel_mask", p.mChannelMask},
        {"persist_offscreen", p.mPersistOffscreen},
    };
}

inline void from_json(const nlohmann::json& j, MovingBombSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("state").get_to(p.mState);
    j.at("timer").get_to(p.mTimer);
    j.at("speed").get_to(p.mSpeed);
    j.at("channel_mask").get_to(p.mChannelMask);
    j.at("persist_offscreen").get_to(p.mPersistOffscreen);
}

inline void to_json(nlohmann::json& j, const TimedMineSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"slapped_mine", p.mSlappedMine},
        {"ticks_until_explosion", p.mTicksUntilExplosion},
        {"explosion_timer", p.mExplosionTimer},
        {"tick_anim", p.mTickAnim},
        {"old_gn_frame", p.mOldGnFrame},
        {"single_tick_timer", p.mSingleTickTimer},
        {"stick_to_lift_point", p.mStickToLiftPoint},
    };
}

inline void from_json(const nlohmann::json& j, TimedMineSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("slapped_mine").get_to(p.mSlappedMine);
    j.at("ticks_until_explosion").get_to(p.mTicksUntilExplosion);
    j.at("explosion_timer").get_to(p.mExplosionTimer);
    j.at("tick_anim").get_to(p.mTickAnim);
    j.at("old_gn_frame").get_to(p.mOldGnFrame);
    j.at("single_tick_timer").get_to(p.mSingleTickTimer);
    j.at("stick_to_lift_point").get_to(p.mStickToLiftPoint);
}

inline void to_json(nlohmann::json& j, const DoorFlameSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"frame_count", p.mFrameCount},
        {"state", p.mState},
        {"random", p.mRandom},
        {"sounds_mask", p.mSoundsMask},
        {"fire_background_glow_id", p.mFireBackgroundGlowId},
        {"flame_sparks_id", p.mFlameSparksId},
    };
    j["sparks_saved"] = p.mSparksSaved;
    if (p.mSparksSaved)
    {
        j["sparks_render"] = p.mSparksRender;
        j["sparks_start_x"] = p.mSparksStartXPos;
        j["sparks_start_y"] = p.mSparksStartYPos;
        nlohmann::json sparks = nlohmann::json::array();
        for (const auto& spark : p.mSparks)
        {
            sparks.push_back(nlohmann::json{
                {"x", spark.mX},
                {"y", spark.mY},
                {"off_x", spark.mOffX},
                {"off_y", spark.mOffY},
                {"random64", spark.mRandom64},
                {"visible", spark.mVisible},
                {"sprite_scale", spark.mSpriteScale},
                {"anim", spark.mAnim},
            });
        }
        j["sparks"] = sparks;
    }
}

inline void from_json(const nlohmann::json& j, DoorFlameSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("frame_count").get_to(p.mFrameCount);
    j.at("state").get_to(p.mState);
    j.at("random").get_to(p.mRandom);
    j.at("sounds_mask").get_to(p.mSoundsMask);
    j.at("fire_background_glow_id").get_to(p.mFireBackgroundGlowId);
    j.at("flame_sparks_id").get_to(p.mFlameSparksId);
    p.mSparksSaved = j.contains("sparks_saved") && j.at("sparks_saved").get<bool>();
    if (p.mSparksSaved)
    {
        j.at("sparks_render").get_to(p.mSparksRender);
        j.at("sparks_start_x").get_to(p.mSparksStartXPos);
        j.at("sparks_start_y").get_to(p.mSparksStartYPos);
        const auto& sparks = j.at("sparks");
        for (size_t i = 0; i < sparks.size() && i < ALIVE_COUNTOF(p.mSparks); i++)
        {
            sparks.at(i).at("x").get_to(p.mSparks[i].mX);
            sparks.at(i).at("y").get_to(p.mSparks[i].mY);
            sparks.at(i).at("off_x").get_to(p.mSparks[i].mOffX);
            sparks.at(i).at("off_y").get_to(p.mSparks[i].mOffY);
            sparks.at(i).at("random64").get_to(p.mSparks[i].mRandom64);
            sparks.at(i).at("visible").get_to(p.mSparks[i].mVisible);
            sparks.at(i).at("sprite_scale").get_to(p.mSparks[i].mSpriteScale);
            sparks.at(i).at("anim").get_to(p.mSparks[i].mAnim);
        }
    }
}

inline void to_json(nlohmann::json& j, const DoorBlockerSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"done", p.mDone},
    };
}

inline void from_json(const nlohmann::json& j, DoorBlockerSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("done").get_to(p.mDone);
}

inline void to_json(nlohmann::json& j, const TorturedMudokonSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"tears_anim", p.mTearsAnim},
        {"zap_anim", p.mZapAnim},
        {"flash_colour_timer", p.mFlashColourTimer},
        {"flash_colour_counter", p.mFlashColourCounter},
        {"state", p.mState},
        {"pain_sound_pitch", p.mPainSoundPitch},
    };
}

inline void from_json(const nlohmann::json& j, TorturedMudokonSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("tears_anim").get_to(p.mTearsAnim);
    j.at("zap_anim").get_to(p.mZapAnim);
    j.at("flash_colour_timer").get_to(p.mFlashColourTimer);
    j.at("flash_colour_counter").get_to(p.mFlashColourCounter);
    j.at("state").get_to(p.mState);
    j.at("pain_sound_pitch").get_to(p.mPainSoundPitch);
}

inline void to_json(nlohmann::json& j, const GlukkonSwitchSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"state", p.mState},
        {"timer", p.mTimer},
    };
}

inline void from_json(const nlohmann::json& j, GlukkonSwitchSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("state").get_to(p.mState);
    j.at("timer").get_to(p.mTimer);
}

inline void to_json(nlohmann::json& j, const BackgroundAnimationSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"sound_channels_mask", p.mSoundChannelsMask},
    };
}

inline void from_json(const nlohmann::json& j, BackgroundAnimationSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("sound_channels_mask").get_to(p.mSoundChannelsMask);
}

inline void to_json(nlohmann::json& j, const LevelLoaderSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"state", p.mState},
        {"fade_id", p.mFadeId},
    };
}

inline void from_json(const nlohmann::json& j, LevelLoaderSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("state").get_to(p.mState);
    j.at("fade_id").get_to(p.mFadeId);
}

inline void to_json(nlohmann::json& j, const BrewMachineSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"remaining_brew_count", p.mRemainingBrewCount},
        {"brew_machine_camera", p.mBrewMachineCamera},
        {"text_x", p.mTextX},
        {"text_y", p.mTextY},
    };
}

inline void from_json(const nlohmann::json& j, BrewMachineSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("remaining_brew_count").get_to(p.mRemainingBrewCount);
    if (j.contains("brew_machine_camera"))
    {
        j.at("brew_machine_camera").get_to(p.mBrewMachineCamera);
        j.at("text_x").get_to(p.mTextX);
        j.at("text_y").get_to(p.mTextY);
    }
}

inline void to_json(nlohmann::json& j, const CrawlingSligButtonSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"in_use", p.mInUse},
    };
}

inline void from_json(const nlohmann::json& j, CrawlingSligButtonSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("in_use").get_to(p.mInUse);
}

inline void to_json(nlohmann::json& j, const GasCountDownSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"gas_time_left_secs", p.mGasTimeLeftSecs},
        {"gas_countdown_timer", p.mGasCountdownTimer},
        {"gas_x_pos", p.mGasXPos},
        {"gas_y_pos", p.mGasYPos},
    };
}

inline void from_json(const nlohmann::json& j, GasCountDownSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("gas_time_left_secs").get_to(p.mGasTimeLeftSecs);
    j.at("gas_countdown_timer").get_to(p.mGasCountdownTimer);
    j.at("gas_x_pos").get_to(p.mGasXPos);
    j.at("gas_y_pos").get_to(p.mGasYPos);
}

inline void to_json(nlohmann::json& j, const SligGetPantsAndWingsSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"state", p.mState},
        {"transform_timer", p.mTransformTimer},
    };
}

inline void from_json(const nlohmann::json& j, SligGetPantsAndWingsSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("state").get_to(p.mState);
    j.at("transform_timer").get_to(p.mTransformTimer);
}

inline void to_json(nlohmann::json& j, const PullRingRopeSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"stay_in_state_ticks", p.mStayInStateTicks},
        {"rope_id", p.mRopeId},
        {"ring_puller_id", p.mRingPullerId},
        {"state", p.mState},
        {"is_pulled", p.mIsPulled},
    };
}

inline void from_json(const nlohmann::json& j, PullRingRopeSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("stay_in_state_ticks").get_to(p.mStayInStateTicks);
    j.at("rope_id").get_to(p.mRopeId);
    j.at("ring_puller_id").get_to(p.mRingPullerId);
    j.at("state").get_to(p.mState);
    j.at("is_pulled").get_to(p.mIsPulled);
}

inline void to_json(nlohmann::json& j, const ParamiteWebLineSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"wobble_idx", p.field_104_wobble_idx},
        {"wobble_pos", p.field_106_wobble_pos},
        {"anim_flare", p.field_108_anim_flare},
        {"pulse_position", p.field_1A0_pulse_position},
        {"pulse_position_speed", p.field_1A2_pulse_position_speed},
        {"delay_counter", p.field_1A4_delay_counter},
    };
}

inline void from_json(const nlohmann::json& j, ParamiteWebLineSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("wobble_idx").get_to(p.field_104_wobble_idx);
    j.at("wobble_pos").get_to(p.field_106_wobble_pos);
    j.at("anim_flare").get_to(p.field_108_anim_flare);
    j.at("pulse_position").get_to(p.field_1A0_pulse_position);
    j.at("pulse_position_speed").get_to(p.field_1A2_pulse_position_speed);
    j.at("delay_counter").get_to(p.field_1A4_delay_counter);
}

inline void to_json(nlohmann::json& j, const ElectricWallSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"sound_timer", p.mSoundTimer},
    };
}

inline void from_json(const nlohmann::json& j, ElectricWallSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("sound_timer").get_to(p.mSoundTimer);
}

inline void to_json(nlohmann::json& j, const DoorSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"current_state", p.mCurrentState},
        {"door_id", p.mDoorId},
    };
}

inline void from_json(const nlohmann::json& j, DoorSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("current_state").get_to(p.mCurrentState);
    j.at("door_id").get_to(p.mDoorId);
}

inline void to_json(nlohmann::json& j, const BulletShellSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"floor_bounce_count", p.mFloorBounceCount},
        {"speed", p.mSpeed},
    };
}

inline void from_json(const nlohmann::json& j, BulletShellSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("floor_bounce_count").get_to(p.mFloorBounceCount);
    j.at("speed").get_to(p.mSpeed);
}

inline void to_json(nlohmann::json& j, const BulletSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"bullet_type", p.mBulletType},
        {"bullet_parent_id", p.mBulletParentId},
        {"x_pos", p.mXPos},
        {"y_pos", p.mYPos},
        {"sprite_scale", p.mSpriteScale},
        {"number_of_bullets", p.mNumberOfBullets},
        {"x_distance", p.mXDistance},
        {"bullet_level", p.mBulletLevel},
        {"bullet_path", p.mBulletPath},
    };
}

inline void from_json(const nlohmann::json& j, BulletSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("bullet_type").get_to(p.mBulletType);
    j.at("bullet_parent_id").get_to(p.mBulletParentId);
    j.at("x_pos").get_to(p.mXPos);
    j.at("y_pos").get_to(p.mYPos);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("number_of_bullets").get_to(p.mNumberOfBullets);
    j.at("x_distance").get_to(p.mXDistance);
    j.at("bullet_level").get_to(p.mBulletLevel);
    j.at("bullet_path").get_to(p.mBulletPath);
}

inline void to_json(nlohmann::json& j, const ColourfulMeterSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"polys_to_render_count", p.mPolysToRenderCount},
        {"count", p.mCount},
        {"mines_alarm_countdown", p.mMinesAlarmCountdown},
        {"starting_switch_state", p.mStartingSwitchState},
        {"bar_count", p.mBarCount},
        {"start_filled", p.mStartFilled},
        {"text_x", p.mTextX},
        {"text_y", p.mTextY},
    };
}

inline void from_json(const nlohmann::json& j, ColourfulMeterSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("polys_to_render_count").get_to(p.mPolysToRenderCount);
    j.at("count").get_to(p.mCount);
    j.at("mines_alarm_countdown").get_to(p.mMinesAlarmCountdown);
    j.at("starting_switch_state").get_to(p.mStartingSwitchState);
    j.at("bar_count").get_to(p.mBarCount);
    j.at("start_filled").get_to(p.mStartFilled);
    j.at("text_x").get_to(p.mTextX);
    j.at("text_y").get_to(p.mTextY);
}

inline void to_json(nlohmann::json& j, const PalOverwriterData& p)
{
    j = nlohmann::json{
        {"target_id", p.mTargetId},
        {"sweep_index", p.mSweepIndex},
        {"kind", p.mKind},
        {"cur_pal_index", p.mCurPalIndex},
        {"num_entries_to_overwrite", p.mNumEntriesToOverwrite},
        {"pal_depth", p.mPalDepth},
        {"update_delay", p.mUpdateDelay},
        {"r", p.mR},
        {"g", p.mG},
        {"b", p.mB},
        {"first_update", p.mFirstUpdate},
        {"done", p.mDone},
    };
}

inline void from_json(const nlohmann::json& j, PalOverwriterData& p)
{
    j.at("target_id").get_to(p.mTargetId);
    j.at("sweep_index").get_to(p.mSweepIndex);
    j.at("kind").get_to(p.mKind);
    j.at("cur_pal_index").get_to(p.mCurPalIndex);
    j.at("num_entries_to_overwrite").get_to(p.mNumEntriesToOverwrite);
    j.at("pal_depth").get_to(p.mPalDepth);
    j.at("update_delay").get_to(p.mUpdateDelay);
    j.at("r").get_to(p.mR);
    j.at("g").get_to(p.mG);
    j.at("b").get_to(p.mB);
    j.at("first_update").get_to(p.mFirstUpdate);
    j.at("done").get_to(p.mDone);
}

inline void to_json(nlohmann::json& j, const PalleteOverwriterSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"data", p.mData},
    };
}

inline void from_json(const nlohmann::json& j, PalleteOverwriterSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("data").get_to(p.mData);
}

inline void to_json(nlohmann::json& j, const ElectrocuteSaveState& p)
{
    nlohmann::json overwriters = nlohmann::json::array();
    for (s32 i = 0; i < 3; i++)
    {
        overwriters.push_back(nlohmann::json{{"present", p.mHasOverwriter[i]}, {"id", p.mOverwriterIds[i]}, {"data", p.mOverwriters[i]}});
    }
    j = nlohmann::json{
        {"type", p.mType},
        {"target_obj_id", p.mTargetObjId},
        {"target_red", p.mTargetRed},
        {"target_green", p.mTargetGreen},
        {"target_blue", p.mTargetBlue},
        {"pal_overwriters_count", p.mPalOverwritersCount},
        {"state", p.mState},
        {"extra_overwriter", p.mExtraOverwriter},
        {"kill_target", p.mKillTarget},
        {"restore_sprite", p.mRestoreSprite},
        {"overwriters", overwriters},
    };
}

inline void from_json(const nlohmann::json& j, ElectrocuteSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("target_obj_id").get_to(p.mTargetObjId);
    j.at("target_red").get_to(p.mTargetRed);
    j.at("target_green").get_to(p.mTargetGreen);
    j.at("target_blue").get_to(p.mTargetBlue);
    j.at("pal_overwriters_count").get_to(p.mPalOverwritersCount);
    j.at("state").get_to(p.mState);
    j.at("extra_overwriter").get_to(p.mExtraOverwriter);
    j.at("kill_target").get_to(p.mKillTarget);
    j.at("restore_sprite").get_to(p.mRestoreSprite);
    const nlohmann::json& overwriters = j.at("overwriters");
    for (s32 i = 0; i < 3 && i < static_cast<s32>(overwriters.size()); i++)
    {
        overwriters.at(i).at("present").get_to(p.mHasOverwriter[i]);
        overwriters.at(i).at("id").get_to(p.mOverwriterIds[i]);
        overwriters.at(i).at("data").get_to(p.mOverwriters[i]);
    }
}

inline void to_json(nlohmann::json& j, const GroundExplosionSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"bomb_sprite_scale", p.mBombSpriteScale},
    };
}

inline void from_json(const nlohmann::json& j, GroundExplosionSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("bomb_sprite_scale").get_to(p.mBombSpriteScale);
}

inline void to_json(nlohmann::json& j, const ExplosionSetSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"screen_shake_idx", p.mScreenShakeIdx},
        {"screen_shake_idx_modifier", p.mScreenShakeIdxModifier},
        {"start_delay", p.mStartDelay},
        {"spacing_multiplicator", p.mSpacingMultiplicator},
        {"increasing_grid_spacing", p.mIncreasingGridSpacing},
        {"spawn_assets", p.mSpawnAssets},
        {"flip_x", p.mFlipX},
        {"active", p.mActive},
        {"tlv_data_saved", p.mTlvDataSaved},
        {"tlv_rect_x", p.mTlvRectX},
        {"tlv_rect_y", p.mTlvRectY},
        {"tlv_rect_w", p.mTlvRectW},
        {"tlv_rect_h", p.mTlvRectH},
        {"sprite_scale", p.mSpriteScale},
        {"switch_id", p.mSwitchId},
        {"asset_interval", p.mAssetInterval},
        {"grid_spacing", p.mGridSpacing},
    };
}

inline void from_json(const nlohmann::json& j, ExplosionSetSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("screen_shake_idx").get_to(p.mScreenShakeIdx);
    j.at("screen_shake_idx_modifier").get_to(p.mScreenShakeIdxModifier);
    j.at("start_delay").get_to(p.mStartDelay);
    j.at("spacing_multiplicator").get_to(p.mSpacingMultiplicator);
    j.at("increasing_grid_spacing").get_to(p.mIncreasingGridSpacing);
    j.at("spawn_assets").get_to(p.mSpawnAssets);
    j.at("flip_x").get_to(p.mFlipX);
    j.at("active").get_to(p.mActive);
    p.mTlvDataSaved = j.value("tlv_data_saved", false);
    if (p.mTlvDataSaved)
    {
        j.at("tlv_rect_x").get_to(p.mTlvRectX);
        j.at("tlv_rect_y").get_to(p.mTlvRectY);
        j.at("tlv_rect_w").get_to(p.mTlvRectW);
        j.at("tlv_rect_h").get_to(p.mTlvRectH);
        j.at("sprite_scale").get_to(p.mSpriteScale);
        j.at("switch_id").get_to(p.mSwitchId);
        j.at("asset_interval").get_to(p.mAssetInterval);
        j.at("grid_spacing").get_to(p.mGridSpacing);
    }
}

inline void to_json(nlohmann::json& j, const PossessionFlickerSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"target_obj_id", p.mTargetObjId},
        {"time_to_flicker", p.mTimeToFlicker},
        {"new_red", p.mNewRed},
        {"new_green", p.mNewGreen},
        {"new_blue", p.mNewBlue},
        {"old_red", p.mOldRed},
        {"old_green", p.mOldGreen},
        {"old_blue", p.mOldBlue},
    };
}

inline void from_json(const nlohmann::json& j, PossessionFlickerSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("target_obj_id").get_to(p.mTargetObjId);
    j.at("time_to_flicker").get_to(p.mTimeToFlicker);
    j.at("new_red").get_to(p.mNewRed);
    j.at("new_green").get_to(p.mNewGreen);
    j.at("new_blue").get_to(p.mNewBlue);
    j.at("old_red").get_to(p.mOldRed);
    j.at("old_green").get_to(p.mOldGreen);
    j.at("old_blue").get_to(p.mOldBlue);
}

inline void to_json(nlohmann::json& j, const DeathGasSaveState& p)
{
    nlohmann::json byte1_array = nlohmann::json::array();
    for (s32 i = 0; i < 50; i++)
    {
        byte1_array.push_back(p.mByte1[i]);
    }
    nlohmann::json byte2_array = nlohmann::json::array();
    for (s32 i = 0; i < 50; i++)
    {
        byte2_array.push_back(p.mByte2[i]);
    }
    j = nlohmann::json{
        {"type", p.mType},
        {"layer", p.mLayer},
        {"amount", p.mAmount},
        {"total", p.mTotal},
        {"done", p.mDone},
        {"byte1", byte1_array},
        {"byte2", byte2_array},
    };
}

inline void from_json(const nlohmann::json& j, DeathGasSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("layer").get_to(p.mLayer);
    j.at("amount").get_to(p.mAmount);
    j.at("total").get_to(p.mTotal);
    j.at("done").get_to(p.mDone);
    const nlohmann::json& byte1_array = j.at("byte1");
    for (s32 i = 0; i < 50 && i < static_cast<s32>(byte1_array.size()); i++)
    {
        byte1_array.at(i).get_to(p.mByte1[i]);
    }
    const nlohmann::json& byte2_array = j.at("byte2");
    for (s32 i = 0; i < 50 && i < static_cast<s32>(byte2_array.size()); i++)
    {
        byte2_array.at(i).get_to(p.mByte2[i]);
    }
}

inline void to_json(nlohmann::json& j, const ThrowableTotalIndicatorSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"start_x_pos", p.mStartXPos},
        {"start_y_pos", p.mStartYPos},
        {"x_pos", p.mXPos},
        {"y_pos", p.mYPos},
        {"speed_x", p.mSpeedX},
        {"speed_y", p.mSpeedY},
        {"sprite_scale", p.mSpriteScale},
        {"ot_layer", p.mOtLayer},
        {"red", p.mRed},
        {"green", p.mGreen},
        {"blue", p.mBlue},
        {"num_to_show", p.mNumToShow},
        {"state", p.mState},
        {"fade", p.mFade},
    };
}

inline void from_json(const nlohmann::json& j, ThrowableTotalIndicatorSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("start_x_pos").get_to(p.mStartXPos);
    j.at("start_y_pos").get_to(p.mStartYPos);
    j.at("x_pos").get_to(p.mXPos);
    j.at("y_pos").get_to(p.mYPos);
    j.at("speed_x").get_to(p.mSpeedX);
    j.at("speed_y").get_to(p.mSpeedY);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("ot_layer").get_to(p.mOtLayer);
    j.at("red").get_to(p.mRed);
    j.at("green").get_to(p.mGreen);
    j.at("blue").get_to(p.mBlue);
    j.at("num_to_show").get_to(p.mNumToShow);
    j.at("state").get_to(p.mState);
    j.at("fade").get_to(p.mFade);
}

inline void to_json(nlohmann::json& j, const OrbWhirlWindParticleState& p)
{
    j = nlohmann::json{
        {"anim", p.mAnim},
        {"ring_target_id", p.mRingTargetId},
        {"x_pos_render_offset", p.mXPosRenderOffset},
        {"y_pos_render_offset", p.mYPosRenderOffset},
        {"render_as_scale", p.mRenderAsScale},
        {"radius_offset_x", p.mRadiusOffsetX},
        {"y_pos_increment", p.mYPosIncrement},
        {"current_scale", p.mCurrentScale},
        {"random_scale", p.mRandomScale},
        {"scale_offset_fly_to_target", p.mScaleOffsetFlyToTarget},
        {"x_pos_mid", p.mXPosMid},
        {"y_pos_mid", p.mYPosMid},
        {"radius_x", p.mRadiusX},
        {"radius_y", p.mRadiusY},
        {"move_y", p.mMoveY},
        {"x_pos", p.mXPos},
        {"y_pos", p.mYPos},
        {"target_obj_scale", p.mTargetObjScale},
        {"x_pos_offset", p.mXPosOffset},
        {"y_pos_offset", p.mYPosOffset},
        {"x_pos_offset2", p.mXPosOffset2},
        {"y_pos_offset2", p.mYPosOffset2},
        {"scale_offset_spin_at_target", p.mScaleOffsetSpinAtTarget},
        {"state", p.mState},
        {"render_angle", p.mRenderAngle},
        {"position_timer", p.mPositionTimer},
        {"counter", p.mCounter},
        {"max_counter", p.mMaxCounter},
        {"is_active", p.mIsActive},
    };
}

inline void from_json(const nlohmann::json& j, OrbWhirlWindParticleState& p)
{
    j.at("anim").get_to(p.mAnim);
    j.at("ring_target_id").get_to(p.mRingTargetId);
    j.at("x_pos_render_offset").get_to(p.mXPosRenderOffset);
    j.at("y_pos_render_offset").get_to(p.mYPosRenderOffset);
    j.at("render_as_scale").get_to(p.mRenderAsScale);
    j.at("radius_offset_x").get_to(p.mRadiusOffsetX);
    j.at("y_pos_increment").get_to(p.mYPosIncrement);
    j.at("current_scale").get_to(p.mCurrentScale);
    j.at("random_scale").get_to(p.mRandomScale);
    j.at("scale_offset_fly_to_target").get_to(p.mScaleOffsetFlyToTarget);
    j.at("x_pos_mid").get_to(p.mXPosMid);
    j.at("y_pos_mid").get_to(p.mYPosMid);
    j.at("radius_x").get_to(p.mRadiusX);
    j.at("radius_y").get_to(p.mRadiusY);
    j.at("move_y").get_to(p.mMoveY);
    j.at("x_pos").get_to(p.mXPos);
    j.at("y_pos").get_to(p.mYPos);
    j.at("target_obj_scale").get_to(p.mTargetObjScale);
    j.at("x_pos_offset").get_to(p.mXPosOffset);
    j.at("y_pos_offset").get_to(p.mYPosOffset);
    j.at("x_pos_offset2").get_to(p.mXPosOffset2);
    j.at("y_pos_offset2").get_to(p.mYPosOffset2);
    j.at("scale_offset_spin_at_target").get_to(p.mScaleOffsetSpinAtTarget);
    j.at("state").get_to(p.mState);
    j.at("render_angle").get_to(p.mRenderAngle);
    j.at("position_timer").get_to(p.mPositionTimer);
    j.at("counter").get_to(p.mCounter);
    j.at("max_counter").get_to(p.mMaxCounter);
    j.at("is_active").get_to(p.mIsActive);
}

inline void to_json(nlohmann::json& j, const OrbWhirlWindSaveState& p)
{
    nlohmann::json particles = nlohmann::json::array();
    for (s32 i = 0; i < p.mParticleIdx && i < 16; i++)
    {
        particles.push_back(p.mParticles[i]);
    }
    j = nlohmann::json{
        {"type", p.mType},
        {"x_pos", p.mXPos},
        {"y_pos", p.mYPos},
        {"scale", p.mScale},
        {"particle_spawn_counter", p.mParticleSpawnCounter},
        {"state", p.mState},
        {"particle_idx", p.mParticleIdx},
        {"is_mudokon_spirit", p.mIsMudokonSpirit},
        {"particles", particles},
    };
}

inline void from_json(const nlohmann::json& j, OrbWhirlWindSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("x_pos").get_to(p.mXPos);
    j.at("y_pos").get_to(p.mYPos);
    j.at("scale").get_to(p.mScale);
    j.at("particle_spawn_counter").get_to(p.mParticleSpawnCounter);
    j.at("state").get_to(p.mState);
    j.at("particle_idx").get_to(p.mParticleIdx);
    j.at("is_mudokon_spirit").get_to(p.mIsMudokonSpirit);
    const nlohmann::json& particles = j.at("particles");
    for (s32 i = 0; i < 16 && i < static_cast<s32>(particles.size()); i++)
    {
        particles.at(i).get_to(p.mParticles[i]);
    }
}

inline void to_json(nlohmann::json& j, const BirdPortalTerminatorSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
    };
}

inline void from_json(const nlohmann::json& j, BirdPortalTerminatorSaveState& p)
{
    j.at("type").get_to(p.mType);
}

inline void to_json(nlohmann::json& j, const ScreenClipperSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"rect_x", p.mRectX},
        {"rect_y", p.mRectY},
        {"rect_w", p.mRectW},
        {"rect_h", p.mRectH},
        {"ot_layer", p.mOtLayer},
    };
}

inline void from_json(const nlohmann::json& j, ScreenClipperSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("rect_x").get_to(p.mRectX);
    j.at("rect_y").get_to(p.mRectY);
    j.at("rect_w").get_to(p.mRectW);
    j.at("rect_h").get_to(p.mRectH);
    j.at("ot_layer").get_to(p.mOtLayer);
}

inline void to_json(nlohmann::json& j, const ShrykullSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"state", p.mState},
        {"zap_interval_timer", p.mZapIntervalTimer},
        {"flash_timer", p.mFlashTimer},
        {"zap_line_id", p.mZapLineId},
        {"zap_target_id", p.mZapTargetId},
        {"can_electrocute", p.mCanElectrocute},
        {"reset_ring_timer", p.mResetRingTimer},
    };
}

inline void from_json(const nlohmann::json& j, ShrykullSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("state").get_to(p.mState);
    j.at("zap_interval_timer").get_to(p.mZapIntervalTimer);
    j.at("flash_timer").get_to(p.mFlashTimer);
    j.at("zap_line_id").get_to(p.mZapLineId);
    j.at("zap_target_id").get_to(p.mZapTargetId);
    j.at("can_electrocute").get_to(p.mCanElectrocute);
    j.at("reset_ring_timer").get_to(p.mResetRingTimer);
}

inline void to_json(nlohmann::json& j, const ZapLineSaveState& p)
{
    nlohmann::json sprite_x_array = nlohmann::json::array();
    for (s32 i = 0; i < 280; i++)
    {
        sprite_x_array.push_back(p.mSpriteX[i]);
    }
    nlohmann::json sprite_y_array = nlohmann::json::array();
    for (s32 i = 0; i < 280; i++)
    {
        sprite_y_array.push_back(p.mSpriteY[i]);
    }
    nlohmann::json part1_array = nlohmann::json::array();
    for (s32 i = 0; i < 20; i++)
    {
        part1_array.push_back(p.mPart1[i]);
    }
    nlohmann::json part2_array = nlohmann::json::array();
    for (s32 i = 0; i < 20; i++)
    {
        part2_array.push_back(p.mPart2[i]);
    }
    nlohmann::json part3_array = nlohmann::json::array();
    for (s32 i = 0; i < 20; i++)
    {
        part3_array.push_back(p.mPart3[i]);
    }
    nlohmann::json segment_x_array = nlohmann::json::array();
    for (s32 i = 0; i < 28; i++)
    {
        segment_x_array.push_back(p.mSegmentX[i]);
    }
    nlohmann::json segment_y_array = nlohmann::json::array();
    for (s32 i = 0; i < 28; i++)
    {
        segment_y_array.push_back(p.mSegmentY[i]);
    }
    j = nlohmann::json{
        {"type", p.mType},
        {"state", p.mState},
        {"x_pos_src", p.mXPosSrc},
        {"y_pos_src", p.mYPosSrc},
        {"x_pos_dst", p.mXPosDst},
        {"y_pos_dst", p.mYPosDst},
        {"alive_timer", p.mAliveTimer},
        {"max_alive_time", p.mMaxAliveTime},
        {"zap_line_type", p.mZapLineType},
        {"sprite_x", sprite_x_array},
        {"sprite_y", sprite_y_array},
        {"part1", part1_array},
        {"part2", part2_array},
        {"part3", part3_array},
        {"segment_x", segment_x_array},
        {"segment_y", segment_y_array},
    };
}

inline void from_json(const nlohmann::json& j, ZapLineSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("state").get_to(p.mState);
    j.at("x_pos_src").get_to(p.mXPosSrc);
    j.at("y_pos_src").get_to(p.mYPosSrc);
    j.at("x_pos_dst").get_to(p.mXPosDst);
    j.at("y_pos_dst").get_to(p.mYPosDst);
    j.at("alive_timer").get_to(p.mAliveTimer);
    j.at("max_alive_time").get_to(p.mMaxAliveTime);
    j.at("zap_line_type").get_to(p.mZapLineType);
    const nlohmann::json& sprite_x_array = j.at("sprite_x");
    for (s32 i = 0; i < 280 && i < static_cast<s32>(sprite_x_array.size()); i++)
    {
        sprite_x_array.at(i).get_to(p.mSpriteX[i]);
    }
    const nlohmann::json& sprite_y_array = j.at("sprite_y");
    for (s32 i = 0; i < 280 && i < static_cast<s32>(sprite_y_array.size()); i++)
    {
        sprite_y_array.at(i).get_to(p.mSpriteY[i]);
    }
    const nlohmann::json& part1_array = j.at("part1");
    for (s32 i = 0; i < 20 && i < static_cast<s32>(part1_array.size()); i++)
    {
        part1_array.at(i).get_to(p.mPart1[i]);
    }
    const nlohmann::json& part2_array = j.at("part2");
    for (s32 i = 0; i < 20 && i < static_cast<s32>(part2_array.size()); i++)
    {
        part2_array.at(i).get_to(p.mPart2[i]);
    }
    const nlohmann::json& part3_array = j.at("part3");
    for (s32 i = 0; i < 20 && i < static_cast<s32>(part3_array.size()); i++)
    {
        part3_array.at(i).get_to(p.mPart3[i]);
    }
    const nlohmann::json& segment_x_array = j.at("segment_x");
    for (s32 i = 0; i < 28 && i < static_cast<s32>(segment_x_array.size()); i++)
    {
        segment_x_array.at(i).get_to(p.mSegmentX[i]);
    }
    const nlohmann::json& segment_y_array = j.at("segment_y");
    for (s32 i = 0; i < 28 && i < static_cast<s32>(segment_y_array.size()); i++)
    {
        segment_y_array.at(i).get_to(p.mSegmentY[i]);
    }
}

inline void to_json(nlohmann::json& j, const CircularFadeSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"fade_in", p.mFadeIn},
        {"done", p.mDone},
        {"destroy_on_done", p.mDestroyOnDone},
        {"fade_colour", p.mFadeColour},
        {"speed", p.mSpeed},
    };
}

inline void from_json(const nlohmann::json& j, CircularFadeSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("fade_in").get_to(p.mFadeIn);
    j.at("done").get_to(p.mDone);
    j.at("destroy_on_done").get_to(p.mDestroyOnDone);
    j.at("fade_colour").get_to(p.mFadeColour);
    j.at("speed").get_to(p.mSpeed);
}

inline void to_json(nlohmann::json& j, const InvisibleEffectSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"state", p.mState},
        {"fade_steps", p.mFadeSteps},
        {"target_id", p.mTargetId},
        {"old_render_mode", p.mOldRenderMode},
        {"semi_trans", p.mSemiTrans},
        {"blending", p.mBlending},
        {"is_invisible", p.mIsInvisible},
    };
}

inline void from_json(const nlohmann::json& j, InvisibleEffectSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("state").get_to(p.mState);
    j.at("fade_steps").get_to(p.mFadeSteps);
    j.at("target_id").get_to(p.mTargetId);
    j.at("old_render_mode").get_to(p.mOldRenderMode);
    j.at("semi_trans").get_to(p.mSemiTrans);
    j.at("blending").get_to(p.mBlending);
    j.at("is_invisible").get_to(p.mIsInvisible);
}

inline void to_json(nlohmann::json& j, const ParamiteWebSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"segments_count", p.mSegmentsCount},
        {"segment_length", p.mSegmentLength},
        {"ttl", p.mTtl},
        {"ttl_remainder", p.mTtlRemainder},
        {"enabled", p.mEnabled},
    };
}

inline void from_json(const nlohmann::json& j, ParamiteWebSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("segments_count").get_to(p.mSegmentsCount);
    j.at("segment_length").get_to(p.mSegmentLength);
    j.at("ttl").get_to(p.mTtl);
    j.at("ttl_remainder").get_to(p.mTtlRemainder);
    j.at("enabled").get_to(p.mEnabled);
}

inline void to_json(nlohmann::json& j, const RollingBallShakerSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"shake_table_idx", p.mShakeTableIdx},
        {"stop_shaking", p.mStopShaking},
    };
}

inline void from_json(const nlohmann::json& j, RollingBallShakerSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("shake_table_idx").get_to(p.mShakeTableIdx);
    j.at("stop_shaking").get_to(p.mStopShaking);
}

inline void to_json(nlohmann::json& j, const BoomMachinePipeSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"state", p.mState},
        {"timer", p.mTimer},
        {"grenade_count", p.mGrenadeCount},
    };
}

inline void from_json(const nlohmann::json& j, BoomMachinePipeSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("state").get_to(p.mState);
    j.at("timer").get_to(p.mTimer);
    j.at("grenade_count").get_to(p.mGrenadeCount);
}

inline void to_json(nlohmann::json& j, const ListOrderSaveState& p)
{
    nlohmann::json ids = nlohmann::json::array();
    for (s32 i = 0; i < p.mCount; i++)
    {
        ids.push_back(p.mIds[i]);
    }
    nlohmann::json offsets = nlohmann::json::array();
    for (s32 i = 0; i < p.mCount; i++)
    {
        offsets.push_back(p.mOffsets[i]);
    }
    j = nlohmann::json{{"type", p.mType}, {"list", static_cast<s32>(p.mList)}, {"ids", ids}, {"offsets", offsets}};
}

inline void from_json(const nlohmann::json& j, ListOrderSaveState& p)
{
    j.at("type").get_to(p.mType);
    p.mList = static_cast<ListOrderSaveState::List>(j.at("list").get<s32>());
    const nlohmann::json& ids = j.at("ids");
    p.mCount = 0;
    for (s32 i = 0; i < static_cast<s32>(ids.size()) && i < ListOrderSaveState::kMaxIds; i++)
    {
        ids.at(i).get_to(p.mIds[i]);
        p.mOffsets[i] = j.contains("offsets") ? j.at("offsets").at(i).get<s32>() : -1;
        p.mCount++;
    }
}

inline void to_json(nlohmann::json& j, const EventsSaveState& p)
{
    nlohmann::json ids = nlohmann::json::array();
    for (s32 i = 0; i < 2; i++)
    {
        for (s32 k = 0; k < static_cast<s32>(Event::kEventMax); k++)
        {
            ids.push_back(p.mEventObjectIds[i][k]);
        }
    }
    j = nlohmann::json{{"type", p.mType}, {"events_to_use", p.mEventsToUse}, {"event_object_ids", ids}};
}

inline void from_json(const nlohmann::json& j, EventsSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("events_to_use").get_to(p.mEventsToUse);
    const nlohmann::json& ids = j.at("event_object_ids");
    const s32 perBuffer = static_cast<s32>(Event::kEventMax);
    for (s32 i = 0; i < 2 * perBuffer && i < static_cast<s32>(ids.size()); i++)
    {
        ids.at(i).get_to(p.mEventObjectIds[i / perBuffer][i % perBuffer]);
    }
}

inline void to_json(nlohmann::json& j, const AliveObjectStateSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"got_shot", p.mbGotShot},
        {"collection_rect_x", p.mCollectionRectX},
        {"collection_rect_y", p.mCollectionRectY},
        {"collection_rect_w", p.mCollectionRectW},
        {"collection_rect_h", p.mCollectionRectH},
        {"last_anim_frame", p.mLastAnimFrame},
        {"can_be_possessed", p.mCanBePossessed},
        {"possessed", p.mPossessed},
        {"zapped_by_shrykull", p.mZappedByShrykull},
        {"can_set_off_explosives", p.mCanSetOffExplosives},
        {"electrocuted", p.mElectrocuted},
        {"invisible", p.mInvisible},
        {"teleporting", p.mTeleporting},
        {"electrocuting", p.mElectrocuting},
        {"can_bees_chase", p.mCanBeesChase},
        {"platform_id", p.mPlatformId},
        {"collision_line_saved", p.mCollisionLineSaved},
        {"collision_line_idx", p.mCollisionLineIdx},
        {"collision_line_is_platforms", p.mCollisionLineIsPlatforms},
        {"health", p.mHealth},
        {"collision_line_x", p.mCollisionLineX},
        {"collision_line_y", p.mCollisionLineY},
        {"collision_line_w", p.mCollisionLineW},
        {"collision_line_h", p.mCollisionLineH},
        {"collision_line_type", p.mCollisionLineType},
    };
}

inline void from_json(const nlohmann::json& j, AliveObjectStateSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("got_shot").get_to(p.mbGotShot);
    j.at("collection_rect_x").get_to(p.mCollectionRectX);
    j.at("collection_rect_y").get_to(p.mCollectionRectY);
    j.at("collection_rect_w").get_to(p.mCollectionRectW);
    j.at("collection_rect_h").get_to(p.mCollectionRectH);
    j.at("last_anim_frame").get_to(p.mLastAnimFrame);
    j.at("can_be_possessed").get_to(p.mCanBePossessed);
    j.at("possessed").get_to(p.mPossessed);
    j.at("zapped_by_shrykull").get_to(p.mZappedByShrykull);
    j.at("can_set_off_explosives").get_to(p.mCanSetOffExplosives);
    j.at("electrocuted").get_to(p.mElectrocuted);
    j.at("invisible").get_to(p.mInvisible);
    j.at("teleporting").get_to(p.mTeleporting);
    j.at("electrocuting").get_to(p.mElectrocuting);
    j.at("can_bees_chase").get_to(p.mCanBeesChase);
    if (j.contains("platform_id"))
    {
        j.at("platform_id").get_to(p.mPlatformId);
    }
    p.mCollisionLineSaved = j.contains("collision_line_saved") && j.at("collision_line_saved").get<bool>();
    if (p.mCollisionLineSaved)
    {
        j.at("collision_line_idx").get_to(p.mCollisionLineIdx);
        j.at("collision_line_is_platforms").get_to(p.mCollisionLineIsPlatforms);
        j.at("health").get_to(p.mHealth);
        j.at("collision_line_x").get_to(p.mCollisionLineX);
        j.at("collision_line_y").get_to(p.mCollisionLineY);
        j.at("collision_line_w").get_to(p.mCollisionLineW);
        j.at("collision_line_h").get_to(p.mCollisionLineH);
        j.at("collision_line_type").get_to(p.mCollisionLineType);
    }
}

inline void to_json(nlohmann::json& j, const GameSpeakSaveState& p)
{
    nlohmann::json events = nlohmann::json::array();
    for (const GameSpeakEvents event : p.mEventBuffer)
    {
        events.push_back(static_cast<s32>(event));
    }
    j = nlohmann::json{
        {"type", p.mType},
        {"last_event_frame", p.mLastEventFrame},
        {"last_event", static_cast<s32>(p.mLastEvent)},
        {"last_event_index", p.mLastEventIndex},
        {"events", events},
    };
}

inline void from_json(const nlohmann::json& j, GameSpeakSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("last_event_frame").get_to(p.mLastEventFrame);
    p.mLastEvent = static_cast<GameSpeakEvents>(j.at("last_event").get<s32>());
    j.at("last_event_index").get_to(p.mLastEventIndex);
    const nlohmann::json& events = j.at("events");
    for (size_t i = 0; i < events.size() && i < 32; i++)
    {
        p.mEventBuffer[i] = static_cast<GameSpeakEvents>(events.at(i).get<s32>());
    }
}

inline void to_json(nlohmann::json& j, const GameSpeakListenerSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"last_event_idx", p.mLastEventIdx},
    };
}

inline void from_json(const nlohmann::json& j, GameSpeakListenerSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("last_event_idx").get_to(p.mLastEventIdx);
}

inline void to_json(nlohmann::json& j, const SnoozeParticleSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"start_y", p.mStartY},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"dest_x", p.mDestX},
        {"dest_y", p.mDestY},
        {"sprite_scale", p.mSpriteScale},
        {"scale_dx", p.mScaleDx},
        {"ot_layer", static_cast<s32>(p.mOtLayer)},
        {"r", p.mR},
        {"g", p.mG},
        {"b", p.mB},
        {"idx", p.mIdx},
        {"blow_up", p.mBlowUp},
        {"state", p.mState},
    };
}

inline void from_json(const nlohmann::json& j, SnoozeParticleSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("start_y").get_to(p.mStartY);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("dest_x").get_to(p.mDestX);
    j.at("dest_y").get_to(p.mDestY);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("scale_dx").get_to(p.mScaleDx);
    p.mOtLayer = static_cast<Layer>(j.at("ot_layer").get<s32>());
    j.at("r").get_to(p.mR);
    j.at("g").get_to(p.mG);
    j.at("b").get_to(p.mB);
    j.at("idx").get_to(p.mIdx);
    j.at("blow_up").get_to(p.mBlowUp);
    j.at("state").get_to(p.mState);
}

inline void to_json(nlohmann::json& j, const WaterSaveState& p)
{
    nlohmann::json drops = nlohmann::json::array();
    for (const auto& drop : p.mDrops)
    {
        drops.push_back(nlohmann::json{
            {"xpos", drop.mXPos},
            {"ypos", drop.mYPos},
            {"zpos", drop.mZPos},
            {"delta_x", drop.mDeltaX},
            {"delta_y", drop.mDeltaY},
            {"delta_z", drop.mDeltaZ},
            {"enabled", drop.mEnabled},
            {"splash_time", drop.mSplashTime},
            {"state", drop.mState},
        });
    }
    j = nlohmann::json{
        {"type", p.mType},
        {"state", p.mState},
        {"particle_count", p.mParticleCount},
        {"current_particle_idx", p.mCurrentParticleIdx},
        {"current_drops", p.mCurrentDrops},
        {"splash_time", p.mSplashTime},
        {"not_in_camera_count", p.mNotInCameraCount},
        {"water_duration", p.mWaterDuration},
        {"hit_timeout", p.mHitTimeout},
        {"screen_x", p.mScreenX},
        {"screen_y", p.mScreenY},
        {"top_left_x", p.mTopLeftX},
        {"top_left_y", p.mTopLeftY},
        {"bottom_right_x", p.mBottomRightX},
        {"bottom_right_y", p.mBottomRightY},
        {"radius", p.mRadius},
        {"centre", p.mCentre},
        {"drops", drops},
    };
}

inline void from_json(const nlohmann::json& j, WaterSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("state").get_to(p.mState);
    j.at("particle_count").get_to(p.mParticleCount);
    j.at("current_particle_idx").get_to(p.mCurrentParticleIdx);
    j.at("current_drops").get_to(p.mCurrentDrops);
    j.at("splash_time").get_to(p.mSplashTime);
    j.at("not_in_camera_count").get_to(p.mNotInCameraCount);
    j.at("water_duration").get_to(p.mWaterDuration);
    j.at("hit_timeout").get_to(p.mHitTimeout);
    j.at("screen_x").get_to(p.mScreenX);
    j.at("screen_y").get_to(p.mScreenY);
    j.at("top_left_x").get_to(p.mTopLeftX);
    j.at("top_left_y").get_to(p.mTopLeftY);
    j.at("bottom_right_x").get_to(p.mBottomRightX);
    j.at("bottom_right_y").get_to(p.mBottomRightY);
    j.at("radius").get_to(p.mRadius);
    j.at("centre").get_to(p.mCentre);
    const nlohmann::json& drops = j.at("drops");
    for (size_t i = 0; i < drops.size() && i < 128; i++)
    {
        const nlohmann::json& drop = drops.at(i);
        drop.at("xpos").get_to(p.mDrops[i].mXPos);
        drop.at("ypos").get_to(p.mDrops[i].mYPos);
        drop.at("zpos").get_to(p.mDrops[i].mZPos);
        drop.at("delta_x").get_to(p.mDrops[i].mDeltaX);
        drop.at("delta_y").get_to(p.mDrops[i].mDeltaY);
        drop.at("delta_z").get_to(p.mDrops[i].mDeltaZ);
        drop.at("enabled").get_to(p.mDrops[i].mEnabled);
        drop.at("splash_time").get_to(p.mDrops[i].mSplashTime);
        drop.at("state").get_to(p.mDrops[i].mState);
    }
}

inline void to_json(nlohmann::json& j, const AnimationStateSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"update_delay", p.mUpdateDelay},
        {"updatable", p.mUpdatable},
        {"drawable", p.mDrawable},
        {"can_explode", p.mCanExplode},
        {"interactive", p.mInteractive},
        {"survive_death_reset", p.mSurviveDeathReset},
        {"update_during_cam_swap", p.mUpdateDuringCamSwap},
        {"cant_kill", p.mCantKill},
        {"chase_counter", p.mChaseCounter},
        {"object", p.mObject},
    };
}

inline void from_json(const nlohmann::json& j, AnimationStateSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("update_delay").get_to(p.mUpdateDelay);
    j.at("updatable").get_to(p.mUpdatable);
    j.at("drawable").get_to(p.mDrawable);
    j.at("can_explode").get_to(p.mCanExplode);
    j.at("interactive").get_to(p.mInteractive);
    j.at("survive_death_reset").get_to(p.mSurviveDeathReset);
    j.at("update_during_cam_swap").get_to(p.mUpdateDuringCamSwap);
    j.at("cant_kill").get_to(p.mCantKill);
    p.mChaseCounter = j.contains("chase_counter") ? j.at("chase_counter").get<u32>() : 0;
    j.at("object").get_to(p.mObject);
}

inline void to_json(nlohmann::json& j, const SligSpawnerSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"tlv_id", p.mTlvId},
        {"state", p.mState},
        {"spawned_slig_id", p.mSpawnedSligId},
    };
}

inline void from_json(const nlohmann::json& j, SligSpawnerSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("tlv_id").get_to(p.mTlvId);
    j.at("state").get_to(p.mState);
    j.at("spawned_slig_id").get_to(p.mSpawnedSligId);
}

NLOHMANN_JSON_SERIALIZE_ENUM(LiftMoverStates, {
    {LiftMoverStates::eInactive_0, "inactive"},
    {LiftMoverStates::eStartMovingDown_1, "start_moving_down"},
    {LiftMoverStates::eMovingDown_2, "moving_down"},
    {LiftMoverStates::eStartMovingUp_3, "start_moving_up"},
    {LiftMoverStates::eMovingUp_4, "moving_up"},
    {LiftMoverStates::eMovingDone_5, "moving_done"},
})

inline void to_json(nlohmann::json& j, const LiftMoverSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"tlv_id", p.mTlvId},
        {"state", p.mState},
        {"target_saved", p.mTargetSaved},
        {"moving", p.mMoving},
        {"target_lift_id", p.mTargetLiftId},
    };
}

inline void from_json(const nlohmann::json& j, LiftMoverSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("tlv_id").get_to(p.mTlvId);
    j.at("state").get_to(p.mState);
    if (j.contains("target_saved") && j.at("target_saved").get<bool>())
    {
        p.mTargetSaved = true;
        j.at("moving").get_to(p.mMoving);
        j.at("target_lift_id").get_to(p.mTargetLiftId);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(BoneStates, {
    {BoneStates::eSpawned_0, "spawned"},
    {BoneStates::eAirborne_1, "airborne"},
    {BoneStates::eCollided_2, "collided"},
    {BoneStates::eOnGround_3, "on_ground"},
    {BoneStates::eEdible_4, "edible"},
    {BoneStates::eFalling_5, "falling"},
})

inline void to_json(nlohmann::json& j, const BoneSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"base_tlv_id", p.mBaseTlvId},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"velx", p.mVelX},
        {"vely", p.mVelY},
        {"sprite_scale", p.mSpriteScale},
        {"current_path", p.mCurrentPath},
        {"current_level", p.mCurrentLevel},
        {"render", p.mRender},
        {"drawable", p.mDrawable},
        {"loop", p.mLoop},
        {"interactive", p.mInteractive},
        {"hit_object", p.mHitObject},
        {"platform_id", p.mPlatformId},
        {"collision_line_type", p.mCollisionLineType},
        {"base_throwable_count", p.mThrowableCount},
        {"state", p.mState},
        {"volume_modifier", p.mBounceCount},
        {"initial_xpos", p.mPreviousXPos},
        {"initial_ypos", p.mPreviousYPos},
        {"time_to_live_timer", p.mTimeToLiveTimer},
        {"shimmer_timer_saved", p.mShimmerTimerSaved},
        {"shimmer_timer", p.mShimmerTimer},
    };
}

inline void from_json(const nlohmann::json& j, BoneSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("base_tlv_id").get_to(p.mBaseTlvId);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("velx").get_to(p.mVelX);
    j.at("vely").get_to(p.mVelY);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("current_path").get_to(p.mCurrentPath);
    j.at("current_level").get_to(p.mCurrentLevel);
    j.at("render").get_to(p.mRender);
    j.at("drawable").get_to(p.mDrawable);
    j.at("loop").get_to(p.mLoop);
    j.at("interactive").get_to(p.mInteractive);
    j.at("hit_object").get_to(p.mHitObject);
    j.at("platform_id").get_to(p.mPlatformId);
    j.at("collision_line_type").get_to(p.mCollisionLineType);
    j.at("base_throwable_count").get_to(p.mThrowableCount);
    j.at("state").get_to(p.mState);
    j.at("volume_modifier").get_to(p.mBounceCount);
    j.at("initial_xpos").get_to(p.mPreviousXPos);
    j.at("initial_ypos").get_to(p.mPreviousYPos);
    j.at("time_to_live_timer").get_to(p.mTimeToLiveTimer);
    if (j.contains("shimmer_timer_saved") && j.at("shimmer_timer_saved").get<bool>())
    {
        p.mShimmerTimerSaved = true;
        j.at("shimmer_timer").get_to(p.mShimmerTimer);
    }
}

inline void to_json(nlohmann::json& j, const LCDScreenSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"tlv_id", p.mTlvId},
        {"message_buffer", std::string(p.mMessageBuffer, strnlen(p.mMessageBuffer, sizeof(p.mMessageBuffer)))},
        {"message_id1", p.mMessageId1},
        {"offset_x", p.mOffsetX},
        {"character_width", p.mCharacterWidth},
        {"show_random_message", p.mShowRandomMessage},
        {"play_letter_sound", p.mPlayLetterSound},
        {"active_message_offset", p.mActiveMessageOffset},
        {"message_cutoff_offset", p.mMessageCutoffOffset},
    };
}

inline void from_json(const nlohmann::json& j, LCDScreenSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("tlv_id").get_to(p.mTlvId);
    const std::string message = j.at("message_buffer").get<std::string>();
    memset(p.mMessageBuffer, 0, sizeof(p.mMessageBuffer));
    memcpy(p.mMessageBuffer, message.data(), std::min(message.size(), sizeof(p.mMessageBuffer) - 1));
    j.at("message_id1").get_to(p.mMessageId1);
    j.at("offset_x").get_to(p.mOffsetX);
    j.at("character_width").get_to(p.mCharacterWidth);
    j.at("show_random_message").get_to(p.mShowRandomMessage);
    j.at("play_letter_sound").get_to(p.mPlayLetterSound);
    j.at("active_message_offset").get_to(p.mActiveMessageOffset);
    j.at("message_cutoff_offset").get_to(p.mMessageCutoffOffset);
}

inline void to_json(nlohmann::json& j, const DoveSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"base_tlv_id", p.mBaseTlvId},
        {"tlv_info", p.mTlvInfo},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"velx", p.mVelX},
        {"vely", p.mVelY},
        {"sprite_scale", p.mSpriteScale},
        {"join_x", p.mJoinX},
        {"join_y", p.mJoinY},
        {"current_path", p.mCurrentPath},
        {"current_level", p.mCurrentLevel},
        {"anim_id", p.mAnimId},
        {"anim", p.mAnim},
        {"keep_in_global_array", p.mKeepInGlobalArray},
        {"fly_away_counter", p.mFlyAwayCounter},
        {"dove_state", p.mDoveState},
        {"join_dead_timer", p.mJoinDeadTimer},
        {"angle", p.mAngle},
        {"abe_portal_timer", p.mAbePortalTimer},
        {"abe_portal_width", p.mAbePortalWidth},
        {"abe_portal_direction", p.mAbePortalDirection},
    };
}

inline void from_json(const nlohmann::json& j, DoveSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("base_tlv_id").get_to(p.mBaseTlvId);
    j.at("tlv_info").get_to(p.mTlvInfo);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("velx").get_to(p.mVelX);
    j.at("vely").get_to(p.mVelY);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("join_x").get_to(p.mJoinX);
    j.at("join_y").get_to(p.mJoinY);
    j.at("current_path").get_to(p.mCurrentPath);
    j.at("current_level").get_to(p.mCurrentLevel);
    j.at("anim_id").get_to(p.mAnimId);
    j.at("anim").get_to(p.mAnim);
    j.at("keep_in_global_array").get_to(p.mKeepInGlobalArray);
    j.at("fly_away_counter").get_to(p.mFlyAwayCounter);
    j.at("dove_state").get_to(p.mDoveState);
    j.at("join_dead_timer").get_to(p.mJoinDeadTimer);
    j.at("angle").get_to(p.mAngle);
    j.at("abe_portal_timer").get_to(p.mAbePortalTimer);
    j.at("abe_portal_width").get_to(p.mAbePortalWidth);
    j.at("abe_portal_direction").get_to(p.mAbePortalDirection);
}

inline void to_json(nlohmann::json& j, const MinesAlarmSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"explosion_timer", p.mExplosionTimer},
    };
}

inline void from_json(const nlohmann::json& j, MinesAlarmSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("explosion_timer").get_to(p.mExplosionTimer);
}

NLOHMANN_JSON_SERIALIZE_ENUM(CrawlingSligMotion, {
    {CrawlingSligMotion::None, "none"},
    {CrawlingSligMotion::Motion_0_Idle, "idle"},
    {CrawlingSligMotion::Motion_1_UsingButton, "using_button"},
    {CrawlingSligMotion::Motion_2_WakingUp, "waking_up"},
    {CrawlingSligMotion::Motion_3_Crawling, "crawling"},
    {CrawlingSligMotion::Motion_4_StartFalling, "start_falling"},
    {CrawlingSligMotion::Motion_5_Falling, "falling"},
    {CrawlingSligMotion::Motion_6_Landing, "landing"},
    {CrawlingSligMotion::Motion_7_ToShakingToIdle, "to_shaking_to_idle"},
    {CrawlingSligMotion::Motion_8_Speaking, "speaking"},
    {CrawlingSligMotion::Motion_9_Snoozing, "snoozing"},
    {CrawlingSligMotion::Motion_10_PushingWall, "pushing_wall"},
    {CrawlingSligMotion::Motion_11_TurnAround, "turn_around"},
    {CrawlingSligMotion::Motion_12_Shaking, "shaking"},
    {CrawlingSligMotion::Motion_13_Empty, "motion_13_empty"},
    {CrawlingSligMotion::Motion_14_ShakingToIdle, "shaking_to_idle"},
    {CrawlingSligMotion::Motion_15_EndCrawling, "end_crawling"},
    {CrawlingSligMotion::Motion_16_IdleToPushingWall, "idle_to_pushing_wall"},
    {CrawlingSligMotion::Motion_17_EndPushingWall, "end_pushing_wall"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(ICrawlingSligBrain::EBrainTypes, {
    {ICrawlingSligBrain::EBrainTypes::Sleeping, "sleeping"},
    {ICrawlingSligBrain::EBrainTypes::Idle, "idle"},
    {ICrawlingSligBrain::EBrainTypes::PanicGetALocker, "panic_get_a_locker"},
    {ICrawlingSligBrain::EBrainTypes::Possessed, "possessed"},
    {ICrawlingSligBrain::EBrainTypes::GetKilled, "get_killed"},
    {ICrawlingSligBrain::EBrainTypes::Transformed, "transformed"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(SleepingBrain::EState, {
    {SleepingBrain::EState::eSleeping, "sleeping"},
    {SleepingBrain::EState::eWakingUp, "waking_up"},
    {SleepingBrain::EState::eIsAwake, "is_awake"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(PanicGetALockerBrain::EState, {
    {PanicGetALockerBrain::EState::eDetermineCrawlDirection, "determine_crawl_direction"},
    {PanicGetALockerBrain::EState::eFalling, "falling"},
    {PanicGetALockerBrain::EState::eSearchLocker, "search_locker"},
    {PanicGetALockerBrain::EState::eTurnAroundForLocker, "turn_around_for_locker"},
    {PanicGetALockerBrain::EState::eSearchLockerOrTurnAround, "search_locker_or_turn_around"},
    {PanicGetALockerBrain::EState::eGetPantsOrWings, "get_pants_or_wings"},
    {PanicGetALockerBrain::EState::eUsingButton, "using_button"},
    {PanicGetALockerBrain::EState::eTurnAround, "turn_around"},
    {PanicGetALockerBrain::EState::eCrawling, "crawling"},
    {PanicGetALockerBrain::EState::eCheckIfWallHit, "check_if_wall_hit"},
    {PanicGetALockerBrain::EState::eBeatBySlig, "beat_by_slig"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(PossessedBrain::EState, {
    {PossessedBrain::EState::eStartPossession, "start_possession"},
    {PossessedBrain::EState::ePossessed, "possessed"},
    {PossessedBrain::EState::eUnpossessing, "unpossessing"},
    {PossessedBrain::EState::eBeatBySlig, "beat_by_slig"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(GetKilledBrain::EState, {
    {GetKilledBrain::EState::eUnknown, "unknown"},
    {GetKilledBrain::EState::eVaporize, "vaporize"},
    {GetKilledBrain::EState::eGibsDeath, "gibs_death"},
    {GetKilledBrain::EState::eSetDead, "set_dead"},
    {GetKilledBrain::EState::eDeathBySlog, "death_by_slog"},
    {GetKilledBrain::EState::eDeathDrop, "death_drop"},
})

inline void to_json(nlohmann::json& j, const CrawlingSligSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"base_tlv_id", p.mBaseTlvId},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"velx", p.mVelX},
        {"vely", p.mVelY},
        {"current_path", p.mCurrentPath},
        {"current_level", p.mCurrentLevel},
        {"sprite_scale", p.mSpriteScale},
        {"r", p.mR},
        {"g", p.mG},
        {"b", p.mB},
        {"flip_x", p.mFlipX},
        {"current_motion", p.mCurrentMotion},
        {"current_frame", p.mCurrentFrame},
        {"frame_change_counter", p.mFrameChangeCounter},
        {"render", p.mRender},
        {"drawable", p.mDrawable},
        {"health", p.mHealth},
        {"current_motion2", p.mCurrentMotion2},
        {"next_motion", p.mNextMotion},
        {"last_line_ypos", p.mLastLineYPos},
        {"collision_line_type", p.mCollisionLineType},
        {"controlled", p.mControlled},
        {"crawling_slig_tlv_id", p.mCrawlingSligTlvId},
        {"brain_state", p.mBrainType},
        {"sleeping_brain_state", p.mSleepingBrainState},
        {"panic_get_a_locker_brain_state", p.mPanicGetALockerBrainState},
        {"possessed_brain_state", p.mPossessedBrainState},
        {"get_killed_brain_state", p.mGetKilledBrainState},
        {"multi_use_timer", p.mMultiUseTimer},
        {"velx_scale_factor", p.mVelxScaleFactor},
        {"chanting", p.mChanting},
        {"abe_level", p.mAbeLevel},
        {"abe_path", p.mAbePath},
        {"abe_camera", p.mAbeCamera},
        {"slig_button_tlv_id", p.mSligButtonTlvId},
        {"70_obj_id", p.field_70_obj_id},
        {"transformed_slig_id", p.mTransformedSligId},
        {"speak", p.mSpeak},
        {"say_help_timer", p.mSayHelpTimer},
    };
}

inline void from_json(const nlohmann::json& j, CrawlingSligSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("base_tlv_id").get_to(p.mBaseTlvId);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("velx").get_to(p.mVelX);
    j.at("vely").get_to(p.mVelY);
    j.at("current_path").get_to(p.mCurrentPath);
    j.at("current_level").get_to(p.mCurrentLevel);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("r").get_to(p.mR);
    j.at("g").get_to(p.mG);
    j.at("b").get_to(p.mB);
    j.at("flip_x").get_to(p.mFlipX);
    j.at("current_motion").get_to(p.mCurrentMotion);
    j.at("current_frame").get_to(p.mCurrentFrame);
    j.at("frame_change_counter").get_to(p.mFrameChangeCounter);
    j.at("render").get_to(p.mRender);
    j.at("drawable").get_to(p.mDrawable);
    j.at("health").get_to(p.mHealth);
    j.at("current_motion2").get_to(p.mCurrentMotion2);
    j.at("next_motion").get_to(p.mNextMotion);
    j.at("last_line_ypos").get_to(p.mLastLineYPos);
    j.at("collision_line_type").get_to(p.mCollisionLineType);
    j.at("controlled").get_to(p.mControlled);
    j.at("crawling_slig_tlv_id").get_to(p.mCrawlingSligTlvId);
    j.at("brain_state").get_to(p.mBrainType);
    j.at("sleeping_brain_state").get_to(p.mSleepingBrainState);
    j.at("panic_get_a_locker_brain_state").get_to(p.mPanicGetALockerBrainState);
    j.at("possessed_brain_state").get_to(p.mPossessedBrainState);
    j.at("get_killed_brain_state").get_to(p.mGetKilledBrainState);
    j.at("multi_use_timer").get_to(p.mMultiUseTimer);
    j.at("velx_scale_factor").get_to(p.mVelxScaleFactor);
    j.at("chanting").get_to(p.mChanting);
    j.at("abe_level").get_to(p.mAbeLevel);
    j.at("abe_path").get_to(p.mAbePath);
    j.at("abe_camera").get_to(p.mAbeCamera);
    j.at("slig_button_tlv_id").get_to(p.mSligButtonTlvId);
    j.at("70_obj_id").get_to(p.field_70_obj_id);
    j.at("transformed_slig_id").get_to(p.mTransformedSligId);
    j.at("speak").get_to(p.mSpeak);
    j.at("say_help_timer").get_to(p.mSayHelpTimer);
}

NLOHMANN_JSON_SERIALIZE_ENUM(DrillStates, {
    {DrillStates::eRestartCycle_0, "restart_cycle"},
    {DrillStates::eGoingDown_1, "going_down"},
    {DrillStates::eGoingUp_2, "going_up"},
})

inline void to_json(nlohmann::json & j, const DrillSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"drill_tlv_id", p.mDrillTlvId},
        {"off_timer", p.mOffTimer},
        {"state", p.mState},
        {"xyoff", p.mXYOff},
        {"toggle_start_state_saved", p.mToggleStartStateSaved},
        {"toggle_start_state_start_on", p.mToggleStartState_StartOn},
    };
}

inline void from_json(const nlohmann::json& j, DrillSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("drill_tlv_id").get_to(p.mDrillTlvId);
    j.at("off_timer").get_to(p.mOffTimer);
    j.at("state").get_to(p.mState);
    j.at("xyoff").get_to(p.mXYOff);
    p.mToggleStartStateSaved = j.value("toggle_start_state_saved", false);
    p.mToggleStartState_StartOn = j.value("toggle_start_state_start_on", false);
}

NLOHMANN_JSON_SERIALIZE_ENUM(FartStates, {
    {FartStates::eIdle_0, "idle"},
    {FartStates::eFlying_1, "flying"},
    {FartStates::eDechanting_2, "dechanting"},
})

inline void to_json(nlohmann::json& j, const EvilFartSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"red", p.mRed},
        {"green", p.mGreen},
        {"blue", p.mBlue},
        {"current_path", p.mCurrentPath},
        {"current_level", p.mCurrentLevel},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"velx", p.mVelX},
        {"vely", p.mVelY},
        {"sprite_scale", p.mSpriteScale},
        {"current_frame", p.mCurrentFrame},
        {"frame_change_counter", p.mFrameChangeCounter},
        {"anim_render", p.mAnimRender},
        {"drawable", p.mDrawable},
        {"abe_level", p.mAbeLevel},
        {"abe_path", p.mAbePath},
        {"abe_camera", p.mAbeCamera},
        {"controlled", p.mControlled},
        {"fart_exploded", p.mFartExploded},
        {"possessed_alive_timer", p.mPossessedAliveTimer},
        {"state", p.mState},
        {"unpossession_timer", p.mUnpossessionTimer},
        {"back_to_abe_timer", p.mBackToAbeTimer},
        {"possessed", p.mPossessed},
    };
}

inline void from_json(const nlohmann::json& j, EvilFartSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("red").get_to(p.mRed);
    j.at("green").get_to(p.mGreen);
    j.at("blue").get_to(p.mBlue);
    j.at("current_path").get_to(p.mCurrentPath);
    j.at("current_level").get_to(p.mCurrentLevel);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("velx").get_to(p.mVelX);
    j.at("vely").get_to(p.mVelY);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("current_frame").get_to(p.mCurrentFrame);
    j.at("frame_change_counter").get_to(p.mFrameChangeCounter);
    j.at("anim_render").get_to(p.mAnimRender);
    j.at("drawable").get_to(p.mDrawable);
    j.at("abe_level").get_to(p.mAbeLevel);
    j.at("abe_path").get_to(p.mAbePath);
    j.at("abe_camera").get_to(p.mAbeCamera);
    j.at("controlled").get_to(p.mControlled);
    j.at("fart_exploded").get_to(p.mFartExploded);
    j.at("possessed_alive_timer").get_to(p.mPossessedAliveTimer);
    j.at("state").get_to(p.mState);
    j.at("unpossession_timer").get_to(p.mUnpossessionTimer);
    j.at("back_to_abe_timer").get_to(p.mBackToAbeTimer);
    if (j.contains("possessed"))
    {
        j.at("possessed").get_to(p.mPossessed);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(eFleechMotions, {
    {eFleechMotions::eNone_m1, "none"},
    {eFleechMotions::Motion_0_Sleeping, "motion_sleeping"},
    {eFleechMotions::Motion_1_WakingUp, "motion_waking_up"},
    {eFleechMotions::Motion_2_Unknown, "motion_unknown"},
    {eFleechMotions::Motion_3_Idle, "motion_idle"},
    {eFleechMotions::Motion_4_Crawl, "motion_crawl"},
    {eFleechMotions::Motion_5_PatrolCry, "motion_patrol_cry"},
    {eFleechMotions::Motion_6_Knockback, "motion_knockback"},
    {eFleechMotions::Motion_7_StopCrawling, "motion_stop_crawling"},
    {eFleechMotions::Motion_8_StopMidCrawlCycle, "motion_stop_mid_crawl_cycle"},
    {eFleechMotions::Motion_9_Fall, "motion_fall"},
    {eFleechMotions::Motion_10_Land, "motion_land"},
    {eFleechMotions::Motion_11_RaiseHead, "motion_raise_head"},
    {eFleechMotions::Motion_12_Climb, "motion_climb"},
    {eFleechMotions::Motion_13_SettleOnGround, "motion_settle_on_ground"},
    {eFleechMotions::Motion_14_ExtendTongueFromEnemy, "motion_extend_tongue_from_enemy"},
    {eFleechMotions::Motion_15_RetractTongueFromEnemey, "motion_retract_tongue_from_enemy"},
    {eFleechMotions::Motion_16_DeathByFalling, "motion_death_by_falling"},
    {eFleechMotions::Motion_17_SleepingWithTongue, "motion_sleeping_with_tongue"},
    {eFleechMotions::Motion_18_Consume, "motion_consume"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(IFleechBrain::EBrainTypes, {
    {IFleechBrain::EBrainTypes::Patrol, "patrol"},
    {IFleechBrain::EBrainTypes::ChasingAbe, "chasing_abe"},
    {IFleechBrain::EBrainTypes::Scared, "scared"},
    {IFleechBrain::EBrainTypes::Death, "death"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(PatrolBrain::EState, {
    {PatrolBrain::EState::eInit, "state_0_init"},
    {PatrolBrain::EState::eSleeping, "sleeping_1"},
    {PatrolBrain::EState::eWakingUp, "state_2"},
    {PatrolBrain::EState::eGoingBackToSleep, "going_back_to_sleep"},
    {PatrolBrain::EState::eAlerted, "alerted_4"},
    {PatrolBrain::EState::eHearingScrabOrParamite, "hearing_scrab_or_paramite_5"},
    {PatrolBrain::EState::eDangerNearby, "state_6"},
    {PatrolBrain::EState::eBackToAlerted, "state_7"},
    {PatrolBrain::EState::eAlertedByAbe, "alerted_by_abe_8"},
    {PatrolBrain::EState::eNearHoist, "state_9"},
    {PatrolBrain::EState::eClimbHoist, "state_10"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(ChasingAbeBrain::EState, {
    {ChasingAbeBrain::EState::eInit, "init_0"},
    {ChasingAbeBrain::EState::eChasingAbe_1, "chasing_abe_1"},
    {ChasingAbeBrain::EState::eUnknown_2, "unknown_2"},
    {ChasingAbeBrain::EState::eContinueChaseAfterFall_3, "continue_chase_after_fall_3"},
    {ChasingAbeBrain::EState::eBlockedByWall_4, "blocked_by_wall_4"},
    {ChasingAbeBrain::EState::eUnknown_5, "unknown_5"},
    {ChasingAbeBrain::EState::eScrabOrParamiteNearby_6, "scrab_or_paramite_nearby_6"},
    {ChasingAbeBrain::EState::eUnknown_7, "unknown_7"},
    {ChasingAbeBrain::EState::eFleechUnknown_8, "fleech_unknown_8"},
    {ChasingAbeBrain::EState::eUnknown_9, "unknown_9"},
    {ChasingAbeBrain::EState::eAbeIsInTongueRange_10, "abe_is_in_tongue_range_10"},
    {ChasingAbeBrain::EState::eIsAbeDead_11, "is_abe_dead_11"},
    {ChasingAbeBrain::EState::eUnknown_12, "unknown_12"},
    {ChasingAbeBrain::EState::eBackToPatrol_13, "back_to_patrol_13"},
    {ChasingAbeBrain::EState::ePrepareToHoist_14, "prepare_to_hoist_14"},
    {ChasingAbeBrain::EState::eHoistDone_15, "hoist_done_15"},
    {ChasingAbeBrain::EState::eGoBackToChasingAbe_16, "go_back_to_chasing_abe_16"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(ScaredBrain::EState, {
    {ScaredBrain::EState::eScared_0, "scared_0"},
    {ScaredBrain::EState::eReactToDanger_1, "react_to_danger_1"},
    {ScaredBrain::EState::eCrawl_2, "crawl_2"},
    {ScaredBrain::EState::eLookForHoist_3, "look_for_hoist_3"},
    {ScaredBrain::EState::eCornered_4, "cornered_4"},
    {ScaredBrain::EState::eCorneredPrepareAttack_5, "cornered_prepare_attack_5"},
    {ScaredBrain::EState::eCorneredAttack_6, "cornered_attack_6"},
    {ScaredBrain::EState::eCheckIfEnemyDead_7, "check_if_enemy_dead_7"},
    {ScaredBrain::EState::eEnemyStillAlive_8, "enemy_still_alive_8"},
    {ScaredBrain::EState::ePatrolArea_9, "patrol_area_9"},
    {ScaredBrain::EState::ePrepareToHoist_10, "prepare_to_hoist_10"},
    {ScaredBrain::EState::eHoisting_11, "hoisting_11"},
})

inline void to_json(nlohmann::json& j, const FleechSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"field_4_obj_id", p.field_4_obj_id},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"velx", p.mVelX},
        {"vely", p.mVelY},
        {"path_number", p.mCurrentPath},
        {"lvl_number", p.mCurrentLevel},
        {"sprite_scale", p.mSpriteScale},
        {"red", p.mRed},
        {"green", p.mGreen},
        {"blue", p.mBlue},
        {"flip_x", p.mFlipX},
        {"current_motion", p.field_28_current_motion},
        {"anim_current_frame", p.field_2A_anim_current_frame},
        {"frame_change_counter", p.field_2C_frame_change_counter},
        {"render", p.mRender},
        {"drawable", p.mDrawable},
        {"health", p.mHealth},
        {"motion", p.mCurrentMotion},
        {"next_motion", p.mNextMotion},
        {"last_line_y_pos", p.mLastLineYPos},
        {"collision_line_type", p.mCollisionLineType},
        {"platform_id", p.mPlatformId},
        {"tlv_info", p.mTlvInfo},
        {"field_44_obj_id", p.mFoodObjId},
        {"tongue_state", p.mTongueState},
        {"tongue_sub_state", p.mTongueSubState},
        {"enemy_x_pos", p.mEnemyXPos},
        {"enemy_y_pos", p.mEnemyYPos},
        {"tongue_origin_x", p.mTongueOriginX},
        {"tongue_origin_y", p.mTongueOriginY},
        {"tongue_destination_x", p.mTongueDestinationX},
        {"tongue_destination_y", p.mTongueDestinationY},
        {"field_5a", p.field_5A},
        {"tongue_active_flag", p.mTongueActive},
        {"render_flag", p.mRenderTongue},
        {"brain_type", p.mBrainType},
        {"patrol_brain_state", p.mPatrolBrainState},
        {"chasing_abe_brain_state", p.mChasingAbeBrainState},
        {"scared_brain_state", p.mScaredBrainState},
        {"field_62", p.mReturnToPreviousMotion},
        {"shrivel_timer", p.field_64_shrivel_timer},
        {"fleech_random_idx", p.field_68_fleech_random_idx},
        {"field_69", p.field_69},
        {"did_map_follow_me", p.field_6A_bDidMapFollowMe},
        {"vel_x_factor", p.field_70_velx_factor},
        {"current_anger", p.field_76_current_anger},
        {"max_anger", p.mMaxAnger},
        {"attack_anger", p.mAttackAngerIncreaser},
        {"wakeup_id", p.mWakeUpSwitchId},
        {"wake_up_switch_anger_value", p.mWakeUpSwitchAngerValue},
        {"wake_up_switch_value", p.mWakeUpSwitchValue},
        {"can_wake_up_id", p.mCanWakeUpSwitchId},
        {"event_x_pos", p.field_84_EventXPos},
        {"scrab_paramite_event_x_pos", p.field_86_ScrabParamiteEventXPos},
        {"patrol_range", p.mPatrolRange},
        {"old_x_pos", p.field_8A_old_xpos},
        {"field_8c", p.field_8C},
        {"rnd_crawl", p.field_8E_rnd_crawl},
        {"chase_delay", p.field_90_chase_delay},
        {"chase_timer", p.field_92_chase_timer},
        {"lost_target_timeout", p.mLostTargetTimeout},
        {"lost_target_timer", p.field_96_lost_target_timer},
        {"hoist_x", p.mHoistX},
        {"hoist_y", p.mHoistY},
        {"angle", p.field_9E_angle},
        {"field_9f", p.field_9F},
        {"hoist_y_distance", p.mHoistYDistance},
        {"hoist_x_distance", p.mHoistXDistance},
        {"field_a8", p.mScrabOrParamite},
        {"field_ac_obj_id", p.field_AC_obj_id},
        {"hoist_done", p.mHoistDone},
        {"chasing_or_scared_crawling_left", p.mChasingOrScaredCrawlingLeft},
        {"shrivel_death", p.mShrivelDeath},
        {"scared_sound", p.mScaredSound},
        {"asleep", p.mAsleep},
        {"goes_to_sleep", p.mGoesToSleep},
        {"persistant", p.mPersistant},
        {"field_b2", p.field_B2},
        {"previous_motion_saved", p.mPreviousMotionSaved},
        {"previous_motion", p.mPreviousMotion},
        {"collision_line_idx", p.mCollisionLineIdx},
    };
}

inline void from_json(const nlohmann::json& j, FleechSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("field_4_obj_id").get_to(p.field_4_obj_id);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("velx").get_to(p.mVelX);
    j.at("vely").get_to(p.mVelY);
    j.at("path_number").get_to(p.mCurrentPath);
    j.at("lvl_number").get_to(p.mCurrentLevel);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("red").get_to(p.mRed);
    j.at("green").get_to(p.mGreen);
    j.at("blue").get_to(p.mBlue);
    j.at("flip_x").get_to(p.mFlipX);
    j.at("current_motion").get_to(p.field_28_current_motion);
    j.at("anim_current_frame").get_to(p.field_2A_anim_current_frame);
    j.at("frame_change_counter").get_to(p.field_2C_frame_change_counter);
    j.at("render").get_to(p.mRender);
    j.at("drawable").get_to(p.mDrawable);
    j.at("health").get_to(p.mHealth);
    j.at(j.contains("motion") ? "motion" : "current_motion").get_to(p.mCurrentMotion);
    j.at("next_motion").get_to(p.mNextMotion);
    j.at("last_line_y_pos").get_to(p.mLastLineYPos);
    j.at("collision_line_type").get_to(p.mCollisionLineType);
    j.at("platform_id").get_to(p.mPlatformId);
    j.at("tlv_info").get_to(p.mTlvInfo);
    j.at("field_44_obj_id").get_to(p.mFoodObjId);
    j.at("tongue_state").get_to(p.mTongueState);
    j.at("tongue_sub_state").get_to(p.mTongueSubState);
    j.at("enemy_x_pos").get_to(p.mEnemyXPos);
    j.at("enemy_y_pos").get_to(p.mEnemyYPos);
    j.at("tongue_origin_x").get_to(p.mTongueOriginX);
    j.at("tongue_origin_y").get_to(p.mTongueOriginY);
    j.at("tongue_destination_x").get_to(p.mTongueDestinationX);
    j.at("tongue_destination_y").get_to(p.mTongueDestinationY);
    j.at("field_5a").get_to(p.field_5A);
    j.at("tongue_active_flag").get_to(p.mTongueActive);
    j.at("render_flag").get_to(p.mRenderTongue);
    j.at("brain_type").get_to(p.mBrainType);
    j.at("patrol_brain_state").get_to(p.mPatrolBrainState);
    j.at("chasing_abe_brain_state").get_to(p.mChasingAbeBrainState);
    j.at("scared_brain_state").get_to(p.mScaredBrainState);
    j.at("field_62").get_to(p.mReturnToPreviousMotion);
    j.at("shrivel_timer").get_to(p.field_64_shrivel_timer);
    j.at("fleech_random_idx").get_to(p.field_68_fleech_random_idx);
    j.at("field_69").get_to(p.field_69);
    j.at("did_map_follow_me").get_to(p.field_6A_bDidMapFollowMe);
    j.at("vel_x_factor").get_to(p.field_70_velx_factor);
    j.at("current_anger").get_to(p.field_76_current_anger);
    j.at("max_anger").get_to(p.mMaxAnger);
    j.at("attack_anger").get_to(p.mAttackAngerIncreaser);
    j.at("wakeup_id").get_to(p.mWakeUpSwitchId);
    j.at("wake_up_switch_anger_value").get_to(p.mWakeUpSwitchAngerValue);
    j.at("wake_up_switch_value").get_to(p.mWakeUpSwitchValue);
    j.at("can_wake_up_id").get_to(p.mCanWakeUpSwitchId);
    j.at("event_x_pos").get_to(p.field_84_EventXPos);
    j.at("scrab_paramite_event_x_pos").get_to(p.field_86_ScrabParamiteEventXPos);
    j.at("patrol_range").get_to(p.mPatrolRange);
    j.at("old_x_pos").get_to(p.field_8A_old_xpos);
    j.at("field_8c").get_to(p.field_8C);
    j.at("rnd_crawl").get_to(p.field_8E_rnd_crawl);
    j.at("chase_delay").get_to(p.field_90_chase_delay);
    j.at("chase_timer").get_to(p.field_92_chase_timer);
    j.at("lost_target_timeout").get_to(p.mLostTargetTimeout);
    j.at("lost_target_timer").get_to(p.field_96_lost_target_timer);
    j.at("hoist_x").get_to(p.mHoistX);
    j.at("hoist_y").get_to(p.mHoistY);
    j.at("angle").get_to(p.field_9E_angle);
    j.at("field_9f").get_to(p.field_9F);
    j.at("hoist_y_distance").get_to(p.mHoistYDistance);
    j.at("hoist_x_distance").get_to(p.mHoistXDistance);
    j.at("field_a8").get_to(p.mScrabOrParamite);
    j.at("field_ac_obj_id").get_to(p.field_AC_obj_id);
    j.at("hoist_done").get_to(p.mHoistDone);
    j.at("chasing_or_scared_crawling_left").get_to(p.mChasingOrScaredCrawlingLeft);
    j.at("shrivel_death").get_to(p.mShrivelDeath);
    j.at("scared_sound").get_to(p.mScaredSound);
    j.at("asleep").get_to(p.mAsleep);
    j.at("goes_to_sleep").get_to(p.mGoesToSleep);
    j.at("persistant").get_to(p.mPersistant);
    j.at("field_b2").get_to(p.field_B2);
    if (j.contains("previous_motion_saved") && j.at("previous_motion_saved").get<bool>())
    {
        p.mPreviousMotionSaved = true;
        j.at("previous_motion").get_to(p.mPreviousMotion);
    }
    if (j.contains("collision_line_idx"))
    {
        j.at("collision_line_idx").get_to(p.mCollisionLineIdx);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(eFlyingSligMotions, {
    {eFlyingSligMotions::None_m1, "none"},
    {eFlyingSligMotions::Motion_0_Idle, "idle"},
    {eFlyingSligMotions::Motion_1_HorizontalMovement, "horizontal_movement"},
    {eFlyingSligMotions::Motion_2_IdleToTurn, "idle_to_turn"},
    {eFlyingSligMotions::Motion_3_DownMovement, "down_movement"},
    {eFlyingSligMotions::Motion_4_DownMovementToTurn, "down_movement_to_turn"},
    {eFlyingSligMotions::Motion_5_UpMovement, "up_movement"},
    {eFlyingSligMotions::Motion_6_UpMovementToTurn, "up_movement_to_turn"},
    {eFlyingSligMotions::Motion_7_LeverPull, "lever_pull"},
    {eFlyingSligMotions::Motion_8_GameSpeak, "game_speak"},
    {eFlyingSligMotions::Motion_9_Possession, "possession"},
    {eFlyingSligMotions::Motion_10_EndHorizontalMovement, "end_horizontal_movement"},
    {eFlyingSligMotions::Motion_11_BeginUpMovement, "begin_up_movement"},
    {eFlyingSligMotions::Motion_12_HorizontalToDownMovement, "horizontal_to_down_movement"},
    {eFlyingSligMotions::Motion_13_UpToHorizontalMovement, "up_to_horizontal_movement"},
    {eFlyingSligMotions::Motion_14_DownToHorizontalMovement, "down_to_horizontal_movement"},
    {eFlyingSligMotions::Motion_15_QuickTurn, "quick_turn"},
    {eFlyingSligMotions::Motion_16_IdleToHorizontalMovement, "idle_to_horizontal_movement"},
    {eFlyingSligMotions::Motion_17_BeginDownMovement, "begin_down_movement"},
    {eFlyingSligMotions::Motion_18_EndDownMovement, "end_down_movement"},
    {eFlyingSligMotions::Motion_19_DownKnockback, "down_knockback"},
    {eFlyingSligMotions::Motion_20_UpKnockback, "up_knockback"},
    {eFlyingSligMotions::Motion_21_EndUpMovement, "end_up_movement"},
    {eFlyingSligMotions::Motion_22_InstantUpXTurn, "instant_up_x_turn"},
    {eFlyingSligMotions::Motion_23_InstantDownXTurn, "instant_down_x_turn"},
    {eFlyingSligMotions::Motion_24_HorizontalToUpMovement, "horizontal_to_up_movement"},
    {eFlyingSligMotions::Motion_25_TurnToHorizontalMovement, "turn_to_horizontal_movement"},
    })

inline void to_json(nlohmann::json& j, const FlyingSligSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"xpos", p.field_4_xpos},
        {"ypos", p.field_8_ypos},
        {"velx", p.field_C_velx},
        {"vely", p.field_10_vely},
        {"path_number", p.field_14_path_number},
        {"lvl_number", p.field_16_lvl_number},
        {"sprite_scale", p.field_18_sprite_scale},
        {"old_r", p.field_1C_oldr},
        {"old_g", p.field_1E_oldg},
        {"old_b", p.field_20_oldb},
        {"anim_flip_x", p.field_22_bAnimFlipX},
        {"current_state", p.field_24_current_state},
        {"current_frame", p.field_26_current_frame},
        {"frame_change_counter", p.field_28_frame_change_counter},
        {"anim_render", p.field_2A_bAnimRender},
        {"drawable", p.field_2B_bDrawable},
        {"current_health", p.field_2C_current_health},
        {"state", p.field_30_current_state},
        {"last_line_y_pos", p.field_34_lastLineYPos},
        {"line_idx", p.field_36_line_idx},
        {"launch_switch_id", p.field_38_launch_switch_id},
        {"possessed", p.mPossessed},
        {"throw_grenade", p.mThrowGrenade},
        {"alerted_and_not_facing_abe", p.mAlertedAndNotFacingAbe},
        {"do_action", p.mDoAction},
        {"chanting", p.mChanting},
        {"speaking2", p.mSpeaking2},
        {"speaking1", p.mSpeaking1},
        {"last_line", p.mLastLine},
        {"unknown1", p.mUnknown1},
        {"unknown2", p.mUnknown2},
        {"tlv_info", p.field_3C_tlvInfo},
        {"timer", p.field_40_timer},
        {"grenade_delay", p.field_44_grenade_delay},
        {"collision_reaction_timer", p.field_48_collision_reaction_timer},
        {"x_speed", p.field_4C_xSpeed},
        {"y_speed", p.field_50_ySpeed},
        {"next_speak", p.field_54_next_speak},
        {"voice_pitch_min", p.field_56_voice_pitch_min},
        {"voice_pitch_min_15c", p.field_15C_voice_pitch_min},
        {"obj_id", p.field_58_obj_id},
        {"field_5c", p.field_5C},
        {"field_60", p.field_60},
        {"field_64", p.field_64},
        {"line_length", p.field_68_line_length},
        {"field_6c", p.field_6C},
        {"lever_pull_range_x_pos", p.field_70_lever_pull_range_xpos},
        {"lever_pull_range_y_pos", p.field_74_lever_pull_range_ypos},
        {"next_x_pos", p.field_88_nextXPos},
        {"next_y_pos", p.field_8C_nextYPos},
        {"fns1_idx", p.field_90_fns1_idx},
        {"abe_level", p.field_9A_abe_level},
        {"abe_path", p.field_9C_abe_path},
        {"abe_camera", p.field_9E_abe_camera},
        {"bobbing_values_table_index", p.field_A0_bobbing_values_table_index},
        {"bobbing_values_index", p.field_A4_bobbing_values_index},
        {"bobbing_value", p.field_A8_bobbing_value},
    };
}

inline void from_json(const nlohmann::json& j, FlyingSligSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("xpos").get_to(p.field_4_xpos);
    j.at("ypos").get_to(p.field_8_ypos);
    j.at("velx").get_to(p.field_C_velx);
    j.at("vely").get_to(p.field_10_vely);
    j.at("path_number").get_to(p.field_14_path_number);
    j.at("lvl_number").get_to(p.field_16_lvl_number);
    j.at("sprite_scale").get_to(p.field_18_sprite_scale);
    j.at("old_r").get_to(p.field_1C_oldr);
    j.at("old_g").get_to(p.field_1E_oldg);
    j.at("old_b").get_to(p.field_20_oldb);
    j.at("anim_flip_x").get_to(p.field_22_bAnimFlipX);
    j.at("current_state").get_to(p.field_24_current_state);
    j.at("current_frame").get_to(p.field_26_current_frame);
    j.at("frame_change_counter").get_to(p.field_28_frame_change_counter);
    j.at("anim_render").get_to(p.field_2A_bAnimRender);
    j.at("drawable").get_to(p.field_2B_bDrawable);
    j.at("current_health").get_to(p.field_2C_current_health);
    j.at(j.contains("state") ? "state" : "current_state").get_to(p.field_30_current_state);
    j.at("last_line_y_pos").get_to(p.field_34_lastLineYPos);
    j.at("line_idx").get_to(p.field_36_line_idx);
    j.at("launch_switch_id").get_to(p.field_38_launch_switch_id);
    j.at("possessed").get_to(p.mPossessed);
    j.at("throw_grenade").get_to(p.mThrowGrenade);
    j.at("alerted_and_not_facing_abe").get_to(p.mAlertedAndNotFacingAbe);
    j.at("do_action").get_to(p.mDoAction);
    j.at("chanting").get_to(p.mChanting);
    j.at("speaking2").get_to(p.mSpeaking2);
    j.at("speaking1").get_to(p.mSpeaking1);
    j.at("last_line").get_to(p.mLastLine);
    j.at("unknown1").get_to(p.mUnknown1);
    j.at("unknown2").get_to(p.mUnknown2);
    j.at("tlv_info").get_to(p.field_3C_tlvInfo);
    j.at("timer").get_to(p.field_40_timer);
    j.at("grenade_delay").get_to(p.field_44_grenade_delay);
    j.at("collision_reaction_timer").get_to(p.field_48_collision_reaction_timer);
    j.at("x_speed").get_to(p.field_4C_xSpeed);
    j.at("y_speed").get_to(p.field_50_ySpeed);
    j.at("next_speak").get_to(p.field_54_next_speak);
    j.at("voice_pitch_min").get_to(p.field_56_voice_pitch_min);
    if (j.contains("voice_pitch_min_15c"))
    {
        j.at("voice_pitch_min_15c").get_to(p.field_15C_voice_pitch_min);
    }
    j.at("obj_id").get_to(p.field_58_obj_id);
    j.at("field_5c").get_to(p.field_5C);
    j.at("field_60").get_to(p.field_60);
    j.at("field_64").get_to(p.field_64);
    j.at("line_length").get_to(p.field_68_line_length);
    j.at("field_6c").get_to(p.field_6C);
    j.at("lever_pull_range_x_pos").get_to(p.field_70_lever_pull_range_xpos);
    j.at("lever_pull_range_y_pos").get_to(p.field_74_lever_pull_range_ypos);
    j.at("next_x_pos").get_to(p.field_88_nextXPos);
    j.at("next_y_pos").get_to(p.field_8C_nextYPos);
    j.at("fns1_idx").get_to(p.field_90_fns1_idx);
    j.at("abe_level").get_to(p.field_9A_abe_level);
    j.at("abe_path").get_to(p.field_9C_abe_path);
    j.at("abe_camera").get_to(p.field_9E_abe_camera);
    j.at("bobbing_values_table_index").get_to(p.field_A0_bobbing_values_table_index);
    j.at("bobbing_values_index").get_to(p.field_A4_bobbing_values_index);
    j.at("bobbing_value").get_to(p.field_A8_bobbing_value);
}

inline void to_json(nlohmann::json& j, const FlyingSligSpawnerSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"tlv_info", p.field_4_tlvInfo},
        {"spawned", p.field_8_bSpawned},
        {"spawned_slig_obj_id", p.field_C_spawned_slig_obj_id},
    };
}

inline void from_json(const nlohmann::json& j, FlyingSligSpawnerSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("tlv_info").get_to(p.field_4_tlvInfo);
    j.at("spawned").get_to(p.field_8_bSpawned);
    j.at("spawned_slig_obj_id").get_to(p.field_C_spawned_slig_obj_id);
}

NLOHMANN_JSON_SERIALIZE_ENUM(GameEnderControllerStates, {
    {GameEnderControllerStates::eInit_0, "init"},
    {GameEnderControllerStates::eDetermineEnding_1, "determine_ending"},
    {GameEnderControllerStates::eFinish_2, "finish"},
    {GameEnderControllerStates::eBadEnding_3, "bad_ending"},
    {GameEnderControllerStates::eGoodEnding_4, "good_ending"},
    {GameEnderControllerStates::eAngelicEnding_5, "angelic_ending"},
    {GameEnderControllerStates::eAngelicEndingCredits_6, "angelic_ending_credits"},
    })

inline void to_json(nlohmann::json& j, const GameEnderControllerSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"obj_id", p.mObjId},
        {"timer", p.mTimer},
        {"state", p.mState},
    };
}

inline void from_json(const nlohmann::json& j, GameEnderControllerSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("obj_id").get_to(p.mObjId);
    j.at("timer").get_to(p.mTimer);
    j.at("state").get_to(p.mState);
}
inline void to_json(nlohmann::json& j, const SlapLockWhirlWindSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"switch_id", p.mSwitchId},
        {"full_state", p.mFullState},
        {"door_x", p.mDoorX},
        {"door_y", p.mDoorY},
        {"door_sprite_scale", p.mDoorSpriteScale},
        {"orb_whirl_wind_id", p.mOrbWhirlWindId},
        {"state", p.mState},
        {"timer", p.mTimer},
    };
}

inline void from_json(const nlohmann::json& j, SlapLockWhirlWindSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("switch_id").get_to(p.mSwitchId);
    if (j.contains("full_state") && j.at("full_state").get<bool>())
    {
        p.mFullState = true;
        j.at("door_x").get_to(p.mDoorX);
        j.at("door_y").get_to(p.mDoorY);
        j.at("door_sprite_scale").get_to(p.mDoorSpriteScale);
        j.at("orb_whirl_wind_id").get_to(p.mOrbWhirlWindId);
        j.at("state").get_to(p.mState);
        j.at("timer").get_to(p.mTimer);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(SlapLockStates, {
    {SlapLockStates::eShaking_0, "shaking"},
    {SlapLockStates::eIdle_1, "idle"},
    {SlapLockStates::eSlapped_2, "slapped"},
    {SlapLockStates::eBroken_3, "broken"},
    {SlapLockStates::eEmitInvisibilityPowerupRing_4, "emit_invisibility_powerup_ring"},
    {SlapLockStates::eFlickerHero_5, "flicker_hero"},
    {SlapLockStates::eGiveInvisibilityFromFlicker_6, "give_invisibility_from_flicker"},
    {SlapLockStates::eGiveInvisibility_7, "give_invisibility"},
})

inline void to_json(nlohmann::json& j, const SlapLockSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"anim_render", p.mAnimRender},
        {"tlv_info", p.mTlvInfo},
        {"tlv_state", p.mTlvState},
        {"state", p.mState},
        {"timer", p.mTimer1},
        {"ability_ring_id", p.mAbilityRingId},
        {"shiny_particle_timer", p.mShinyParticleTimer},
        {"has_ghost", p.mHasGhost},
        {"possession_flicker_id", p.mPossessionFlickerId},
    };
}

inline void from_json(const nlohmann::json& j, SlapLockSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("anim_render").get_to(p.mAnimRender);
    j.at("tlv_info").get_to(p.mTlvInfo);
    j.at("tlv_state").get_to(p.mTlvState);
    j.at("state").get_to(p.mState);
    j.at("timer").get_to(p.mTimer1);
    j.at("ability_ring_id").get_to(p.mAbilityRingId);
    j.at("shiny_particle_timer").get_to(p.mShinyParticleTimer);
    if (j.contains("has_ghost"))
    {
        j.at("has_ghost").get_to(p.mHasGhost);
        j.at("possession_flicker_id").get_to(p.mPossessionFlickerId);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(GreeterBrainStates, {
    {GreeterBrainStates::eBrain_0_Patrol, "patrol"},
    {GreeterBrainStates::eBrain_1_PatrolTurn, "patrol_turn"},
    {GreeterBrainStates::eBrain_2_Speak, "speak"},
    {GreeterBrainStates::eBrain_3_ChaseSpeak, "chase_speak"},
    {GreeterBrainStates::eBrain_4_Chase, "chase"},
    {GreeterBrainStates::eBrain_5_Knockback, "knockback"},
    {GreeterBrainStates::eBrain_6_ToChase, "to_chase"},
    {GreeterBrainStates::eBrain_7_Fall, "fall"},
})

inline void to_json(nlohmann::json& j, const GreeterSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"r", p.field_2_r},
        {"g", p.field_4_g},
        {"b", p.field_6_b},
        {"path_number", p.field_8_path_number},
        {"lvl_number", p.field_A_lvl_number},
        {"xpos", p.field_C_xpos},
        {"ypos", p.field_10_ypos},
        {"velx", p.field_14_velx},
        {"vely", p.field_18_vely},
        {"sprite_scale", p.field_1C_sprite_scale},
        {"current_frame", p.mCurrentFrame},
        {"frame_change_counter", p.mFrameChangeCounter},
        {"anim_render", p.mAnimRender},
        {"drawable", p.mDrawable},
        {"tlv_info", p.mTlvId},
        {"last_turn_time", p.field_30_last_turn_time},
        {"timer", p.field_34_timer},
        {"times_shot", p.mTimesShot},
        {"search_tlv_id", p.mSearchTlvId},
        {"dont_set_destroyed", p.field_3A_bDontSetDestroyed},
        {"chasing", p.mChasing},
        {"speed", p.field_40_speed},
        {"brain_state", p.mBrainState},
        {"target_on_left", p.field_46_targetOnLeft},
        {"target_on_right", p.field_48_targetOnRight},
        {"motion_laser_xpos", p.mMotionLaserXPos},
    };
}

inline void from_json(const nlohmann::json& j, GreeterSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("r").get_to(p.field_2_r);
    j.at("g").get_to(p.field_4_g);
    j.at("b").get_to(p.field_6_b);
    j.at("path_number").get_to(p.field_8_path_number);
    j.at("lvl_number").get_to(p.field_A_lvl_number);
    j.at("xpos").get_to(p.field_C_xpos);
    j.at("ypos").get_to(p.field_10_ypos);
    j.at("velx").get_to(p.field_14_velx);
    j.at("vely").get_to(p.field_18_vely);
    j.at("sprite_scale").get_to(p.field_1C_sprite_scale);
    j.at("current_frame").get_to(p.mCurrentFrame);
    j.at("frame_change_counter").get_to(p.mFrameChangeCounter);
    j.at("anim_render").get_to(p.mAnimRender);
    j.at("drawable").get_to(p.mDrawable);
    j.at("tlv_info").get_to(p.mTlvId);
    j.at("last_turn_time").get_to(p.field_30_last_turn_time);
    j.at("timer").get_to(p.field_34_timer);
    j.at("times_shot").get_to(p.mTimesShot);
    if (j.contains("search_tlv_id"))
    {
        j.at("search_tlv_id").get_to(p.mSearchTlvId);
    }
    j.at("dont_set_destroyed").get_to(p.field_3A_bDontSetDestroyed);
    j.at("chasing").get_to(p.mChasing);
    j.at("speed").get_to(p.field_40_speed);
    j.at("brain_state").get_to(p.mBrainState);
    j.at("target_on_left").get_to(p.field_46_targetOnLeft);
    j.at("target_on_right").get_to(p.field_48_targetOnRight);
    j.at("motion_laser_xpos").get_to(p.mMotionLaserXPos);
}

NLOHMANN_JSON_SERIALIZE_ENUM(GrenadeStates, {
    {GrenadeStates::eFallingToBeCollected_0, "falling_to_be_collected"},
    {GrenadeStates::eWaitToBeCollected_1, "wait_to_be_collected"},
    {GrenadeStates::eDoesNothing_2, "does_nothing_2"},
    {GrenadeStates::eCountingDown_3, "counting_down"},
    {GrenadeStates::eFalling_4, "falling"},
    {GrenadeStates::eHitGround_5, "hit_ground"},
    {GrenadeStates::eWaitForExplodeEnd_6, "wait_for_explode_end"},
    {GrenadeStates::eExploded_7, "exploded"},
    {GrenadeStates::eDoesNothing_8, "does_nothing_8"},
    {GrenadeStates::eFallingBlowUpOnGround_9, "falling_blow_up_on_ground"},
})

inline void to_json(nlohmann::json& j, const GrenadeSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"tlv_info", p.mTlvInfo},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"velx", p.mVelX},
        {"vely", p.mVelY},
        {"sprite_scale", p.mSpriteScale},
        {"current_path", p.mCurrentPath},
        {"current_level", p.mCurrentLevel},
        {"render", p.mRender},
        {"drawable", p.mDrawable},
        {"loop", p.mLoop},
        {"interactive", p.mInteractive},
        {"explode_now", p.mExplodeNow},
        {"blow_up_on_collision", p.mBlowUpOnCollision},
        {"platform_id", p.mPlatformId},
        {"collision_line_type", p.mCollisionLineType},
        {"throwable_count", p.mThrowableCount},
        {"state", p.mState},
        {"bounce_count", p.mBounceCount},
        {"explode_countdown", p.mExplodeCountdown},
        {"previous_xpos", p.mPreviousXPos},
        {"previous_ypos", p.mPreviousYPos},
        {"throwable_dead", p.mBaseThrowableDead},
        {"grenade_owner_id", p.mGrenadeOwnerId},
        {"explosion_id", p.mExplosionId},
    };
}

inline void from_json(const nlohmann::json& j, GrenadeSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("tlv_info").get_to(p.mTlvInfo);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("velx").get_to(p.mVelX);
    j.at("vely").get_to(p.mVelY);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("current_path").get_to(p.mCurrentPath);
    j.at("current_level").get_to(p.mCurrentLevel);
    j.at("render").get_to(p.mRender);
    j.at("drawable").get_to(p.mDrawable);
    j.at("loop").get_to(p.mLoop);
    j.at("interactive").get_to(p.mInteractive);
    j.at("explode_now").get_to(p.mExplodeNow);
    j.at("blow_up_on_collision").get_to(p.mBlowUpOnCollision);
    j.at("platform_id").get_to(p.mPlatformId);
    j.at("collision_line_type").get_to(p.mCollisionLineType);
    j.at("throwable_count").get_to(p.mThrowableCount);
    j.at("state").get_to(p.mState);
    j.at("bounce_count").get_to(p.mBounceCount);
    j.at("explode_countdown").get_to(p.mExplodeCountdown);
    j.at("previous_xpos").get_to(p.mPreviousXPos);
    j.at("previous_ypos").get_to(p.mPreviousYPos);
    if (j.contains("throwable_dead"))
    {
        j.at("throwable_dead").get_to(p.mBaseThrowableDead);
        j.at("grenade_owner_id").get_to(p.mGrenadeOwnerId);
        j.at("explosion_id").get_to(p.mExplosionId);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(GlukkonSpeak, {
    {GlukkonSpeak::None, "none"},
    {GlukkonSpeak::Hey_0, "hey"},
    {GlukkonSpeak::DoIt_1, "do_it"},
    {GlukkonSpeak::StayHere_2, "stay_here"},
    {GlukkonSpeak::Commere_3, "commere"},
    {GlukkonSpeak::AllOYa_4, "alloya"},
    {GlukkonSpeak::Heh_5, "heh"},
    {GlukkonSpeak::Help_6, "help"},
    {GlukkonSpeak::Laugh_7, "laugh"},
    {GlukkonSpeak::KillEm_8, "kill_em"},
    {GlukkonSpeak::Unused_9, "unused_9"},
    {GlukkonSpeak::What_11, "what"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(eGlukkonMotions, {
    {eGlukkonMotions::eNone_m1, "none"},
    {eGlukkonMotions::Motion_0_Idle, "idle"},
    {eGlukkonMotions::Motion_1_Walk, "walk"},
    {eGlukkonMotions::Motion_2_Turn, "turn"},
    {eGlukkonMotions::Motion_3_KnockBack, "knockback"},
    {eGlukkonMotions::Motion_4_Jump, "jump"},
    {eGlukkonMotions::Motion_5_JumpToFall, "jump_to_fall"},
    {eGlukkonMotions::Motion_6_WalkToFall, "walk_to_fall"},
    {eGlukkonMotions::Motion_7_Fall, "fall"},
    {eGlukkonMotions::Motion_8_DeathFall, "death_fall"},
    {eGlukkonMotions::Motion_9_Land, "land"},
    {eGlukkonMotions::Motion_10_ChantShake, "chant_shake"},
    {eGlukkonMotions::Motion_11_Speak1, "speak1"},
    {eGlukkonMotions::Motion_12_Speak2, "speak2"},
    {eGlukkonMotions::Motion_13_LongLaugh, "long_laugh"},
    {eGlukkonMotions::Motion_14_BeginWalk, "begin_walk"},
    {eGlukkonMotions::Motion_15_EndWalk, "end_walk"},
    {eGlukkonMotions::Motion_16_StandToJump, "stand_to_jump"},
    {eGlukkonMotions::Motion_17_JumpToStand, "jump_to_stand"},
    {eGlukkonMotions::Motion_18_WalkToJump, "walk_to_jump"},
    {eGlukkonMotions::Motion_19_JumpToWalk, "jump_to_walk"},
    {eGlukkonMotions::Motion_20_KnockBackStandBegin, "knockback_stand_begin"},
    {eGlukkonMotions::Motion_21_GetShot, "get_shot"},
    {eGlukkonMotions::Motion_22_KnockBackStandEnd, "knockback_stand_end"},
    {eGlukkonMotions::Motion_23_Speak3, "speak3"},
    {eGlukkonMotions::Motion_24_EndSingleStep, "end_single_step"},
})

inline void to_json(nlohmann::json& j, const GlukkonSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"field_4_object_id", p.field_4_object_id},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"velx", p.mVelX},
        {"vely", p.mVelY},
        {"field_18_path", p.mCurrentPath},
        {"field_1a_level", p.mCurrentLevel},
        {"sprite_scale", p.mSpriteScale},
        {"ring_red", p.mRed},
        {"ring_green", p.mGreen},
        {"ring_blue", p.mBlue},
        {"flip_x", p.mFlipX},
        {"field_28_current_motion", p.mCurrentMotion},
        {"current_frame", p.mCurrentFrame},
        {"frame_change_counter", p.mFrameChangeCounter},
        {"render", p.mRender},
        {"drawable", p.mDrawable},
        {"health", p.mHealth},
        {"field_34_current_motion", p.mCurrentMotion2},
        {"next_motion", p.mNextMotion},
        {"last_line_ypos", p.field_38_last_line_ypos},
        {"line_type", p.mLineType},
        {"is_active_char", p.mIsActiveChar},
        {"tlv_info", p.mTlvId},
        {"brain_state_idx", p.mBrainStateIdx},
        {"brain_sub_state", p.mBrainSubState},
        {"timer", p.field_54_timer},
        {"vel_x_scale_factor", p.mFallingVelXScaleFactor},
        {"prevent_depossession", p.mPreventDepossession},
        {"field_60_level", p.mAbeLevel},
        {"field_62_path", p.mAbePath},
        {"camera", p.mAbeCamera},
        {"speak", p.mSpeak},
        {"gamespeak_pitch", p.mGamespeakPitch},
        {"previous_ypos", p.mPreviousYPos},
        {"randomish_speak_timer", p.mRandomishSpeakTimer},
        {"turn_or_help_timer", p.mTurnOrHelpTimer},
        {"panic_timer", p.mPanicTimer},
        {"field_7c", p.field_7C},
        {"knockback_delay_after_getting_shot_timer", p.mKnockbackDelayAfterGettingShotTimer},
        {"getting_shot_timer", p.mGettingShotTimer},
        {"field_88_obj_id", p.mFadeId},
        {"can_be_possessed", p.mCanBePossessed},
        {"current_type", p.mCurrentType},
        {"current_walk_pitch", p.mCurrentWalkPitch},
    };
}

inline void from_json(const nlohmann::json& j, GlukkonSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("field_4_object_id").get_to(p.field_4_object_id);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("velx").get_to(p.mVelX);
    j.at("vely").get_to(p.mVelY);
    j.at("field_18_path").get_to(p.mCurrentPath);
    j.at("field_1a_level").get_to(p.mCurrentLevel);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("ring_red").get_to(p.mRed);
    j.at("ring_green").get_to(p.mGreen);
    j.at("ring_blue").get_to(p.mBlue);
    j.at("flip_x").get_to(p.mFlipX);
    j.at("field_28_current_motion").get_to(p.mCurrentMotion);
    j.at("current_frame").get_to(p.mCurrentFrame);
    j.at("frame_change_counter").get_to(p.mFrameChangeCounter);
    j.at("render").get_to(p.mRender);
    j.at("drawable").get_to(p.mDrawable);
    j.at("health").get_to(p.mHealth);
    j.at("field_34_current_motion").get_to(p.mCurrentMotion2);
    j.at("next_motion").get_to(p.mNextMotion);
    j.at("last_line_ypos").get_to(p.field_38_last_line_ypos);
    j.at("line_type").get_to(p.mLineType);
    j.at("is_active_char").get_to(p.mIsActiveChar);
    j.at("tlv_info").get_to(p.mTlvId);
    j.at("brain_state_idx").get_to(p.mBrainStateIdx);
    j.at("brain_sub_state").get_to(p.mBrainSubState);
    j.at("timer").get_to(p.field_54_timer);
    j.at("vel_x_scale_factor").get_to(p.mFallingVelXScaleFactor);
    j.at("prevent_depossession").get_to(p.mPreventDepossession);
    j.at("field_60_level").get_to(p.mAbeLevel);
    j.at("field_62_path").get_to(p.mAbePath);
    j.at("camera").get_to(p.mAbeCamera);
    j.at("speak").get_to(p.mSpeak);
    j.at("gamespeak_pitch").get_to(p.mGamespeakPitch);
    j.at("previous_ypos").get_to(p.mPreviousYPos);
    j.at("randomish_speak_timer").get_to(p.mRandomishSpeakTimer);
    j.at("turn_or_help_timer").get_to(p.mTurnOrHelpTimer);
    j.at("panic_timer").get_to(p.mPanicTimer);
    j.at("field_7c").get_to(p.field_7C);
    j.at("knockback_delay_after_getting_shot_timer").get_to(p.mKnockbackDelayAfterGettingShotTimer);
    j.at("getting_shot_timer").get_to(p.mGettingShotTimer);
    j.at("field_88_obj_id").get_to(p.mFadeId);
    j.at("can_be_possessed").get_to(p.mCanBePossessed);
    j.at("current_type").get_to(p.mCurrentType);
    if (j.contains("current_walk_pitch"))
    {
        j.at("current_walk_pitch").get_to(p.mCurrentWalkPitch);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(Mud_Emotion, {
    {Mud_Emotion::eNormal_0, "normal"},
    {Mud_Emotion::eAngry_1, "angry"},
    {Mud_Emotion::eAggressive_2, "aggressive"},
    {Mud_Emotion::eSad_3, "sad"},
    {Mud_Emotion::eSuicidal_4, "suicidal"},
    {Mud_Emotion::eHappy_5, "happy"},
    {Mud_Emotion::eWired_6, "wired"},
    {Mud_Emotion::eSick_7, "sick"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(MudSounds, {
    {MudSounds::eNone, "none"},
    {MudSounds::eEmpty_0, "empty_0"},
    {MudSounds::eEmpty_1, "empty_1"},
    {MudSounds::eEmpty_2, "empty_2"},
    {MudSounds::eHelloNeutral_3, "hello_neutral"},
    {MudSounds::eFollowMe_4, "follow_me"},
    {MudSounds::eAnger_5, "anger"},
    {MudSounds::eWait_6, "wait"},
    {MudSounds::eFart_7, "fart"},
    {MudSounds::eGiggle_8, "giggle"},
    {MudSounds::eHurt2_9, "hurt2"},
    {MudSounds::eLaugh_10, "laugh"},
    {MudSounds::eGoodbye_11, "goodbye"},
    {MudSounds::eOkay_12, "okay"},
    {MudSounds::eNuhUh_13, "nuh_uh"},
    {MudSounds::eOops_14, "oops"},
    {MudSounds::eDeathDropScream_15, "death_drop_scream"},
    {MudSounds::eHurt1_16, "hurt1"},
    {MudSounds::eAllOYa_17, "alloya"},
    {MudSounds::eHiAngry_18, "hi_angry"},
    {MudSounds::eHiHappy_19, "hi_happy"},
    {MudSounds::eHiSad_20, "hi_sad"},
    {MudSounds::eNoAngry_21, "no_angry"},
    {MudSounds::eNoSad_22, "no_sad"},
    {MudSounds::eFartPuh_23, "fart_puh"},
    {MudSounds::eSick_24, "sick"},
    {MudSounds::eWork_25, "work"},
    {MudSounds::eStopIt_26, "stop_it"},
    {MudSounds::eSorry_27, "sorry"},
    {MudSounds::eSadUgh_28, "sad_ugh"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(eAbeMotions, {
    { eAbeMotions::None_m1, "none"},
    { eAbeMotions::Motion_0_Idle_44EEB0, "idle"},
    { eAbeMotions::Motion_1_WalkLoop_44FBA0, "walk_loop"},
    { eAbeMotions::Motion_2_StandingTurn_451830, "standing_turn" },
    { eAbeMotions::Motion_3_Fall_459B60, "fall" },
    { eAbeMotions::Motion_4_WalkToIdle_44FFC0, "walk_to_idle" },
    { eAbeMotions::Motion_5_MidWalkToIdle_450080, "mid_walk_to_idle" },
    { eAbeMotions::Motion_6_WalkBegin_44FEE0, "walk_begin" },
    { eAbeMotions::Motion_7_Speak_45B140, "motion_7_speak" },
    { eAbeMotions::Motion_8_Speak_45B160, "motion_8_speak" },
    { eAbeMotions::Motion_9_Speak_45B180, "motion_9_speak" },
    { eAbeMotions::Motion_10_Fart_45B1A0, "fart" },
    { eAbeMotions::Motion_11_Speak_45B0A0, "motion_11_speak" },
    { eAbeMotions::Motion_12_Null_4569C0, "motion_12_null" },
    { eAbeMotions::Motion_13_HoistBegin_452B20, "hoist_begin" },
    { eAbeMotions::Motion_14_HoistIdle_452440, "hoist_idle" },
    { eAbeMotions::Motion_15_HoistLand_452BA0, "hoist_land" },
    { eAbeMotions::Motion_16_LandSoft_45A360, "land_soft" },
    { eAbeMotions::Motion_17_CrouchIdle_456BC0, "crouch_idle" },
    { eAbeMotions::Motion_18_CrouchToStand_454600, "crouch_to_stand" },
    { eAbeMotions::Motion_19_StandToCrouch_453DC0, "stand_to_crouch" },
    { eAbeMotions::Motion_20_CrouchSpeak_454550, "crouch_speak" },
    { eAbeMotions::jMotion_21_ToCrouchSpeak_4545E0, "to_crouch_speak" },
    { eAbeMotions::Motion_22_RollBegin_4539A0, "roll_begin" },
    { eAbeMotions::Motion_23_RollLoop_453A90, "roll_loop" },
    { eAbeMotions::Motion_24_453D00, "motion_24" },
    { eAbeMotions::Motion_25_RunSlideStop_451330, "run_slide_stop" },
    { eAbeMotions::Motion_26_RunTurn_451500, "run_turn" },
    { eAbeMotions::Motion_27_HopBegin_4521C0, "hop_begin" },
    { eAbeMotions::Motion_28_HopMid_451C50, "hop_mid" },
    { eAbeMotions::Motion_29_HopLand_4523D0, "hop_land" },
    { eAbeMotions::Motion_30_RunJumpBegin_4532E0, "run_jump_begin" },
    { eAbeMotions::Motion_31_RunJumpMid_452C10, "run_jump_mid" },
    { eAbeMotions::Motion_32_RunJumpLand_453460, "run_jump_land" },
    { eAbeMotions::Motion_33_RunLoop_4508E0, "run_loop" },
    { eAbeMotions::Motion_34_DunnoBegin_44ECF0, "dunno_begin" },
    { eAbeMotions::Motion_35_DunnoEnd_44ED10, "dunno_end" },
    { eAbeMotions::Motion_36_Null_45BC50, "motion_36_null" },
    { eAbeMotions::Motion_37_CrouchTurn_454390, "crouch_turn" },
    { eAbeMotions::jMotion_38_RunToRoll_453A70, "run_to_roll" },
    { eAbeMotions::Motion_39_StandingToRun_450D40, "standing_to_run" },
    { eAbeMotions::Motion_40_SneakLoop_450550, "sneak_loop" },
    { eAbeMotions::Motion_41_WalkToSneak_450250, "walk_to_sneak" },
    { eAbeMotions::Motion_42_SneakToWalk_4503D0, "sneak_to_walk" },
    { eAbeMotions::Motion_43_MidWalkToSneak_450380, "mid_walk_to_sneak" },
    { eAbeMotions::Motion_44_MidSneakToWalk_450500, "mid_sneak_to_walk" },
    { eAbeMotions::Motion_45_SneakBegin_4507A0, "sneak_begin" },
    { eAbeMotions::Motion_46_SneakToIdle_450870, "sneak_to_idle" },
    { eAbeMotions::jMotion_47_MidSneakToIdle_4508C0, "mid_sneak_to_idle" },
    { eAbeMotions::Motion_48_WalkToRun_4500A0, "walk_to_run" },
    { eAbeMotions::Motion_49_MidWalkToRun_450200, "mid_walk_to_run" },
    { eAbeMotions::Motion_50_RunToWalk_450E20, "run_to_walk" },
    { eAbeMotions::Motion_51_MidRunToWalk_450F50, "mid_run_to_walk" },
    { eAbeMotions::Motion_52_RunTurnToRun_451710, "run_turn_to_run" },
    { eAbeMotions::Motion_53_RunTurnToWalk_451800, "run_turn_to_walk" },
    { eAbeMotions::Motion_54_RunJumpLandRun_4538F0, "run_jump_land_run" },
    { eAbeMotions::Motion_55_RunJumpLandWalk_453970, "run_jump_land_walk" },
    { eAbeMotions::Motion_56_DeathDropFall_4591F0, "death_drop_fall" },
    { eAbeMotions::Motion_57_Dead_4589A0, "dead" },
    { eAbeMotions::Motion_58_DeadPre_4593E0, "dead_pre" },
    { eAbeMotions::Motion_59_Null_459450, "motion_59_null" },
    { eAbeMotions::Motion_60_Unused_4A3200, "motion_60_unused" },
    { eAbeMotions::Motion_61_TurnToRun_456530, "turn_to_run" },
    { eAbeMotions::Motion_62_Punch_454750, "punch" },
    { eAbeMotions::Motion_63_Sorry_454670, "sorry" },
    { eAbeMotions::Motion_64_AfterSorry_454730, "after_sorry" },
    { eAbeMotions::Motion_65_LedgeAscend_4548E0, "ledge_ascend" },
    { eAbeMotions::Motion_66_LedgeDescend_454970, "ledge_descend" },
    { eAbeMotions::Motion_67_LedgeHang_454E20, "ledge_hang" },
    { eAbeMotions::Motion_68_ToOffScreenHoist_454B80, "to_off_screen_hoist" },
    { eAbeMotions::Motion_69_LedgeHangWobble_454EF0, "ledge_hang_wobble" },
    { eAbeMotions::Motion_70_RingRopePullHang_455AF0, "ring_rope_pull_hang" },
    { eAbeMotions::Motion_71_Knockback_455090, "knockback" },
    { eAbeMotions::Motion_72_KnockbackGetUp_455340, "knockback_get_up" },
    { eAbeMotions::Motion_73_PushWall_4553A0, "push_wall" },
    { eAbeMotions::Motion_74_RollingKnockback_455290, "rolling_knockback" },
    { eAbeMotions::Motion_75_JumpIntoWell_45C7B0, "jump_into_well" },
    { eAbeMotions::Motion_76_ToInsideOfAWellLocal_45CA40, "to_inside_of_a_well_local" },
    { eAbeMotions::Motion_77_ToWellShotOut_45D130, "to_well_shot_out" },
    { eAbeMotions::Motion_78_WellBegin_45C810, "motion_78_well_begin" },
    { eAbeMotions::Motion_79_InsideWellLocal_45CA60, "inside_well_local" },
    { eAbeMotions::Motion_80_WellShotOut_45D150, "well_shot_out" },
    { eAbeMotions::jMotion_81_WellBegin_45C7F0, "jmotion_81_well_begin" },
    { eAbeMotions::Motion_82_InsideWellExpress_45CC80, "inside_well_express" },
    { eAbeMotions::Motion_83_WellExpressShotOut_45CF70, "well_express_shot_out" },
    { eAbeMotions::Motion_84_FallLandDie_45A420, "fall_land_die" },
    { eAbeMotions::jMotion_85_Fall_455070, "jmotion_85_fall" },
    { eAbeMotions::Motion_86_HandstoneBegin, "handstone_begin" },
    { eAbeMotions::Motion_87_HandstoneEnd, "handstone_end" },
    { eAbeMotions::Motion_88_GrenadeMachineUse, "grenade_machine_use" },
    { eAbeMotions::Motion_89_BrewMachineBegin, "brew_machine_begin" },
    { eAbeMotions::Motion_90_BrewMachineEnd, "brew_machine_end" },
    { eAbeMotions::Motion_91_FallingFromGrab, "falling_from_grab" },
    { eAbeMotions::Motion_92_ForceDownFromHoist, "force_down_from_hoist" },
    { eAbeMotions::Motion_93_WalkOffEdge, "walk_off_edge" },
    { eAbeMotions::Motion_94_RunOffEdge, "run_off_edge" },
    { eAbeMotions::Motion_95_SneakOffEdge, "sneak_off_edge" },
    { eAbeMotions::Motion_96_HopToFall, "hop_to_fall" },
    { eAbeMotions::Motion_97_RunJumpToFall, "run_jump_to_fall" },
    { eAbeMotions::Motion_98_RollOffEdge, "roll_off_edge" },
    { eAbeMotions::Motion_99_LeverUse, "lever_use" },
    { eAbeMotions::Motion_100_SlapBomb, "slap_bomb" },
    { eAbeMotions::Motion_101_KnockForward, "knock_forward" },
    { eAbeMotions::Motion_102_RollingKnockForward, "rolling_knock_forward" },
    { eAbeMotions::jMotion_103_KnockForwardGetUp, "knock_forward_get_up" },
    { eAbeMotions::Motion_104_RockThrowStandingHold, "rock_throw_standing_hold" },
    { eAbeMotions::Motion_105_RockThrowStandingThrow, "rock_throw_standing_throw" },
    { eAbeMotions::Motion_106_RockThrowStandingEnd, "rock_throw_standing_end" },
    { eAbeMotions::Motion_107_RockThrowCrouchingHold, "rock_throw_crouching_hold" },
    { eAbeMotions::Motion_108_RockThrowCrouchingThrow, "rock_throw_crouching_throw" },
    { eAbeMotions::Motion_109_ZShotRolling, "z_shot_rolling" },
    { eAbeMotions::Motion_110_ZShot, "z_shot" },
    { eAbeMotions::Motion_111_PickupItem, "pickup_item" },
    { eAbeMotions::Motion_112_Chant, "chant" },
    { eAbeMotions::Motion_113_ChantEnd, "chant_end" },
    { eAbeMotions::Motion_114_DoorEnter, "door_enter" },
    { eAbeMotions::Motion_115_DoorExit, "door_exit" },
    { eAbeMotions::Motion_116_MineCarEnter, "mine_car_enter" },
    { eAbeMotions::Motion_117_InMineCar, "in_mine_car" },
    { eAbeMotions::Motion_118_MineCarExit, "mine_car_exit" },
    { eAbeMotions::Motion_119_ToShrykull, "to_shrykull" },
    { eAbeMotions::Motion_120_EndShrykull, "end_shrykull" },
    { eAbeMotions::Motion_121_LiftGrabBegin, "lift_grab_begin" },
    { eAbeMotions::Motion_122_LiftGrabEnd, "lift_grab_end" },
    { eAbeMotions::Motion_123_LiftGrabIdle, "lift_grab_idle" },
    { eAbeMotions::Motion_124_LiftUseUp, "lift_use_up" },
    { eAbeMotions::Motion_125_LiftUseDown, "lift_use_down" },
    { eAbeMotions::Motion_126_TurnWheelBegin, "turn_wheel_begin" },
    { eAbeMotions::Motion_127_TurnWheelLoop, "turn_wheel_loop" },
    { eAbeMotions::Motion_128_TurnWheelEnd, "turn_wheel_end" },
    { eAbeMotions::Motion_129_PoisonGasDeath, "poison_gas_death" }
    })

inline void to_json(nlohmann::json& j, const AbeSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"velx", p.mVelX},
        {"vely", p.mVelY},
        {"current_path", p.mCurrentPath},
        {"current_level", p.mCurrentLevel},
        {"sprite_scale", p.mSpriteScale},
        {"scale", p.mScale},
        {"red", p.mRed},
        {"green", p.mGreen},
        {"blue", p.mBlue},
        {"anim_flip_x", p.bAnimFlipX},
        {"current_motion", p.mCurrentMotion},
        {"current_frame", p.mCurrentFrame},
        {"frame_change_counter", p.mFrameChangeCounter},
        {"render_layer", p.mRenderLayer},
        {"anim_render", p.mAnimRender},
        {"is_drawable", p.mIsDrawable},
        {"health", p.mHealth},
        {"current_motion_2", p.mCurrentMotion2},
        {"next_motion", p.mNextMotion},
        {"last_line_y_pos", p.mLastLineYPos},
        {"collision_line_type", p.mCollisionLineType},
        {"platform_id", p.mPlatformId},
        {"is_electrocuted", p.mIsElectrocuted},
        {"is_invisible", p.mIsInvisible},
        {"is_abe_controlled", p.mIsAbeControlled},
        {"x_vel_slow_by", p.field_48_x_vel_slow_by},
        {"state", p.field_50_state},
        {"timer", p.field_54_timer},
        {"abe_timer", p.field_58_abe_timer},
        {"regen_health_timer", p.mRegenHealthTimer},
        {"mood", p.mMood},
        {"say", p.mSay},
        {"auto_say_timer", p.mAutoSayTimer},
        {"ring_pulse_timer", p.mRingPulseTimer},
        {"base_throwable_count", p.mBaseThrowableCount},
        {"shrivel", p.mShrivel},
        {"have_shrykull", p.mHaveShrykull},
        {"have_invisibility", p.bHaveInvisiblity},
        {"prev_input", p.mPrevInput},
        {"released_buttons", p.mReleasedButtons},
        {"prev_input_raw", p.mPrevInputRaw},
        {"released_buttons_raw", p.mReleasedButtonsRaw},
        {"do_quicksave", p.mDoQuicksave},
        {"save_file_id", p.mSaveFileId},
        {"knockdown_motion", p.mKnockdownMotion},
        {"rolling_motion_timer", p.mRollingMotionTimer},
        {"death_fade_out_id", p.mDeathFadeOutId},
        {"pull_ring_rope_id", p.mPullRingRopeId},
        {"slappable_or_pickup_id", p.mSlappableOrPickupId},
        {"work_wheel_id", p.mWorkWheelId},
        {"circular_fade_id", p.mCircularFadeId},
        {"orb_whirlwind_id", p.mOrbWhirlWindId},
        {"possessed_object_id", p.mPossessedObjectId},
        {"throwable_id", p.mThrowableId},
        {"invisibility_timer", p.mInvisibilityTimer},
        {"invisibility_duration", p.mInvisibilityDuration},
        {"handstone_cam_idx", p.mHandStoneCamIdx},
        {"handstone_type", p.mHandStoneType},
        {"fmv_id", p.mFmvId},
        {"handstone_cam_1", p.mHandStoneCam1},
        {"handstone_cam_2", p.mHandStoneCam2},
        {"handstone_cam_3", p.mHandStoneCam3},
        {"has_evil_fart", p.mHasEvilFart},
        {"dest_well_level", p.mDstWellLevel},
        {"dest_well_path", p.mDstWellPath},
        {"dest_well_camera", p.mDstWellCamera},
        {"door_id", p.door_id},
        {"throw_direction", p.mThrowDirection},
        {"bird_portal_sub_state", p.mBirdPortalSubState},
        {"bird_portal_id", p.mBirdPortalId},
        {"return_to_previous_motion", p.mReturnToPreviousMotion},
        {"prevent_chanting", p.mPreventChanting},
        {"land_softly", p.mLandSoftly},
        {"laugh_at_chant_end", p.mLaughAtChantEnd},
        {"play_ledge_grab_sounds", p.mPlayLedgeGrabSounds},
        {"have_healing", p.mHaveHealing},
        {"teleporting", p.mTeleporting},
        {"mudanchee_done", p.mMudancheeDone},
        {"mudomo_done", p.mMudomoDone},
        {"shadow_enabled", p.mShadowEnabled},
        {"shadow_at_bottom", p.mShadowAtBottom},
        {"previous_motion", p.mPreviousMotion},
        {"anim", p.mAnim},
    };
}

inline void from_json(const nlohmann::json& j, AbeSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("velx").get_to(p.mVelX);
    j.at("vely").get_to(p.mVelY);
    j.at("current_path").get_to(p.mCurrentPath);
    j.at("current_level").get_to(p.mCurrentLevel);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("scale").get_to(p.mScale);
    j.at("red").get_to(p.mRed);
    j.at("green").get_to(p.mGreen);
    j.at("blue").get_to(p.mBlue);
    j.at("anim_flip_x").get_to(p.bAnimFlipX);
    j.at("current_motion").get_to(p.mCurrentMotion);
    j.at("current_frame").get_to(p.mCurrentFrame);
    j.at("frame_change_counter").get_to(p.mFrameChangeCounter);
    j.at("render_layer").get_to(p.mRenderLayer);
    j.at("anim_render").get_to(p.mAnimRender);
    j.at("is_drawable").get_to(p.mIsDrawable);
    j.at("health").get_to(p.mHealth);
    j.at("current_motion_2").get_to(p.mCurrentMotion2);
    j.at("next_motion").get_to(p.mNextMotion);
    j.at("last_line_y_pos").get_to(p.mLastLineYPos);
    j.at("collision_line_type").get_to(p.mCollisionLineType);
    j.at("platform_id").get_to(p.mPlatformId);
    j.at("is_electrocuted").get_to(p.mIsElectrocuted);
    j.at("is_invisible").get_to(p.mIsInvisible);
    j.at("is_abe_controlled").get_to(p.mIsAbeControlled);
    j.at("x_vel_slow_by").get_to(p.field_48_x_vel_slow_by);
    j.at("state").get_to(p.field_50_state);
    j.at("timer").get_to(p.field_54_timer);
    j.at("abe_timer").get_to(p.field_58_abe_timer);
    j.at("regen_health_timer").get_to(p.mRegenHealthTimer);
    j.at("mood").get_to(p.mMood);
    j.at("say").get_to(p.mSay);
    j.at("auto_say_timer").get_to(p.mAutoSayTimer);
    j.at("ring_pulse_timer").get_to(p.mRingPulseTimer);
    j.at("base_throwable_count").get_to(p.mBaseThrowableCount);
    j.at("shrivel").get_to(p.mShrivel);
    j.at("have_shrykull").get_to(p.mHaveShrykull);
    j.at("have_invisibility").get_to(p.bHaveInvisiblity);
    j.at("prev_input").get_to(p.mPrevInput);
    j.at("released_buttons").get_to(p.mReleasedButtons);
    j.at("prev_input_raw").get_to(p.mPrevInputRaw);
    j.at("released_buttons_raw").get_to(p.mReleasedButtonsRaw);
    j.at("do_quicksave").get_to(p.mDoQuicksave);
    j.at("save_file_id").get_to(p.mSaveFileId);
    j.at("knockdown_motion").get_to(p.mKnockdownMotion);
    j.at("rolling_motion_timer").get_to(p.mRollingMotionTimer);
    j.at("death_fade_out_id").get_to(p.mDeathFadeOutId);
    if (j.contains("pull_ring_rope_id"))
    {
        j.at("pull_ring_rope_id").get_to(p.mPullRingRopeId);
        j.at("slappable_or_pickup_id").get_to(p.mSlappableOrPickupId);
        j.at("work_wheel_id").get_to(p.mWorkWheelId);
    }
    j.at("circular_fade_id").get_to(p.mCircularFadeId);
    j.at("orb_whirlwind_id").get_to(p.mOrbWhirlWindId);
    j.at("possessed_object_id").get_to(p.mPossessedObjectId);
    j.at("throwable_id").get_to(p.mThrowableId);
    j.at("invisibility_timer").get_to(p.mInvisibilityTimer);
    j.at("invisibility_duration").get_to(p.mInvisibilityDuration);
    j.at("handstone_cam_idx").get_to(p.mHandStoneCamIdx);
    j.at("handstone_type").get_to(p.mHandStoneType);
    j.at("fmv_id").get_to(p.mFmvId);
    j.at("handstone_cam_1").get_to(p.mHandStoneCam1);
    j.at("handstone_cam_2").get_to(p.mHandStoneCam2);
    j.at("handstone_cam_3").get_to(p.mHandStoneCam3);
    j.at("has_evil_fart").get_to(p.mHasEvilFart);
    j.at("dest_well_level").get_to(p.mDstWellLevel);
    j.at("dest_well_path").get_to(p.mDstWellPath);
    j.at("dest_well_camera").get_to(p.mDstWellCamera);
    j.at("door_id").get_to(p.door_id);
    j.at("throw_direction").get_to(p.mThrowDirection);
    j.at("bird_portal_sub_state").get_to(p.mBirdPortalSubState);
    j.at("bird_portal_id").get_to(p.mBirdPortalId);
    j.at("return_to_previous_motion").get_to(p.mReturnToPreviousMotion);
    j.at("prevent_chanting").get_to(p.mPreventChanting);
    j.at("land_softly").get_to(p.mLandSoftly);
    j.at("laugh_at_chant_end").get_to(p.mLaughAtChantEnd);
    j.at("play_ledge_grab_sounds").get_to(p.mPlayLedgeGrabSounds);
    j.at("have_healing").get_to(p.mHaveHealing);
    j.at("teleporting").get_to(p.mTeleporting);
    j.at("mudanchee_done").get_to(p.mMudancheeDone);
    j.at("mudomo_done").get_to(p.mMudomoDone);
    j.at("shadow_enabled").get_to(p.mShadowEnabled);
    j.at("shadow_at_bottom").get_to(p.mShadowAtBottom);
    j.at("previous_motion").get_to(p.mPreviousMotion);
    // Saves from before the animation was kept don't have it
    if (j.contains("anim"))
    {
        j.at("anim").get_to(p.mAnim);
    }
}
inline void to_json(nlohmann::json& j, const LiftPointSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"platform_id", p.mPlatformId},
        {"tlv_id", p.mTlvId},
        {"floor_level_y", p.mFloorLevelY},
        {"lift_point_stop_type", p.mLiftPointStopType},
        {"moving", p.mMoving},
        {"top_floor", p.mTopFloor},
        {"middle_floor", p.mMiddleFloor},
        {"bottom_floor", p.mBottomFloor},
        {"move_to_floor_level", p.mMoveToFloorLevel},
        {"keep_on_middle_floor", p.mKeepOnMiddleFloor},
        {"anim", p.mAnim},
        {"lift_wheel_anim", p.mLiftWheelAnim},
        {"pulley_anim", p.mPulleyAnim},
        {"platform_base_saved", p.mPlatformBaseSaved},
        {"platform_base_x_offset", p.mPlatformBaseXOffset},
        {"platform_base_width_offset", p.mPlatformBaseWidthOffset},
        {"platform_base_y_offset", p.mPlatformBaseYOffset},
        {"platform_base_height_offset", p.mPlatformBaseHeightOffset},
        {"platform_base_count", p.mPlatformBaseCount},
    };
}

inline void from_json(const nlohmann::json& j, LiftPointSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("platform_id").get_to(p.mPlatformId);
    j.at("tlv_id").get_to(p.mTlvId);
    j.at("floor_level_y").get_to(p.mFloorLevelY);
    j.at("lift_point_stop_type").get_to(p.mLiftPointStopType);
    j.at("moving").get_to(p.mMoving);
    j.at("top_floor").get_to(p.mTopFloor);
    j.at("middle_floor").get_to(p.mMiddleFloor);
    j.at("bottom_floor").get_to(p.mBottomFloor);
    j.at("move_to_floor_level").get_to(p.mMoveToFloorLevel);
    j.at("keep_on_middle_floor").get_to(p.mKeepOnMiddleFloor);
    // Saves from before these were kept don't have them
    if (j.contains("anim"))
    {
        j.at("anim").get_to(p.mAnim);
        j.at("lift_wheel_anim").get_to(p.mLiftWheelAnim);
        j.at("pulley_anim").get_to(p.mPulleyAnim);
        if (j.contains("platform_base_saved") && j.at("platform_base_saved").get<bool>())
        {
            p.mPlatformBaseSaved = true;
            j.at("platform_base_x_offset").get_to(p.mPlatformBaseXOffset);
            j.at("platform_base_width_offset").get_to(p.mPlatformBaseWidthOffset);
            j.at("platform_base_y_offset").get_to(p.mPlatformBaseYOffset);
            j.at("platform_base_height_offset").get_to(p.mPlatformBaseHeightOffset);
            j.at("platform_base_count").get_to(p.mPlatformBaseCount);
        }
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(MudAction, {
    {MudAction::eHelloOrAllYa_0, "hello_or_allya"},
    {MudAction::eFollowMe_1, "follow_me"},
    {MudAction::eWait_2, "wait"},
    {MudAction::eUnused_3, "unused_3"},
    {MudAction::eStopIt_4, "stop_it"},
    {MudAction::eUnused_5, "unused_5"},
    {MudAction::eFart_6, "fart"},
    {MudAction::eSlapOrWater_7, "slap_or_water"},
    {MudAction::eSorry_8, "sorry"},
    {MudAction::eMudAbuse_9, "mud_abuse"},
    {MudAction::eComfort_10, "comfort"},
    {MudAction::eSmackIntoWall_11, "smack_into_wall"},
    {MudAction::eLaugh_12, "laugh"},
    {MudAction::eDuck_13, "duck"},
    {MudAction::eMudDied_14, "mud_died"},
    {MudAction::eUnknown_15, "unknown_15"},
    {MudAction::eUnknown_16, "unknown_16"},
    {MudAction::eNone_17, "none"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(Mud_Brain_State, {
    {Mud_Brain_State::Brain_0_GiveRings, "give_rings"},
    {Mud_Brain_State::Brain_1_Chisel, "chisel"},
    {Mud_Brain_State::Brain_2_CrouchScrub, "crouch_scrub"},
    {Mud_Brain_State::Brain_3_TurnWheel, "turn_wheel"},
    {Mud_Brain_State::Brain_4_ListeningToAbe, "listening_to_abe"},
    {Mud_Brain_State::Brain_5_ShrivelDeath, "shrivel_death"},
    {Mud_Brain_State::Brain_6_Escape, "escape"},
    {Mud_Brain_State::Brain_7_FallAndSmackDeath, "fall_and_smack_death"},
    {Mud_Brain_State::Brain_8_AngryWorker, "angry_worker"},
    {Mud_Brain_State::Brain_9_Sick, "sick"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(eMudMotions, {
    {eMudMotions::None_m1, "none"},
    {eMudMotions::Motion_0_Idle, "idle"},
    {eMudMotions::Motion_1_WalkLoop, "walk_loop"},
    {eMudMotions::Motion_2_StandingTurn, "standing_turn"},
    {eMudMotions::M_Speak_3_472FA0, "speak_3"},
    {eMudMotions::M_Speak_4_472FA0, "speak_4"},
    {eMudMotions::M_Speak_5_472FA0, "speak_5"},
    {eMudMotions::M_Speak_6_472FA0, "speak_6"},
    {eMudMotions::Motion_7_WalkBegin, "walk_begin"},
    {eMudMotions::Motion_8_WalkToIdle, "walk_to_idle"},
    {eMudMotions::Motion_9_MidWalkToIdle, "mid_walk_to_idle"},
    {eMudMotions::Motion_10_LeverUse, "lever_use"},
    {eMudMotions::Motion_11_Chisel, "chisel"},
    {eMudMotions::Motion_12_StartChisel, "start_chisel"},
    {eMudMotions::Motion_13_StopChisel, "stop_chisel"},
    {eMudMotions::Motion_14_CrouchScrub, "crouch_scrub"},
    {eMudMotions::Motion_15_CrouchIdle, "crouch_idle"},
    {eMudMotions::Motion_16_CrouchTurn, "crouch_turn"},
    {eMudMotions::Motion_17_StandToCrouch, "stand_to_crouch"},
    {eMudMotions::Motion_18_CrouchToStand, "crouch_to_stand"},
    {eMudMotions::Motion_19_WalkToRun, "walk_to_run"},
    {eMudMotions::Motion_20_MidWalkToRun, "mid_walk_to_run"},
    {eMudMotions::Motion_21_RunLoop, "run_loop"},
    {eMudMotions::Motion_22_RunToWalk, "run_to_walk"},
    {eMudMotions::Motion_23_MidRunToWalk, "mid_run_to_walk"},
    {eMudMotions::Motion_24_RunSlideStop, "run_slide_stop"},
    {eMudMotions::Motion_25_RunSlideTurn, "run_slide_turn"},
    {eMudMotions::Motion_26_RunTurnToRun, "run_turn_to_run"},
    {eMudMotions::Motion_27_SneakLoop, "sneak_loop"},
    {eMudMotions::Motion_28_MidWalkToSneak, "mid_walk_to_sneak"},
    {eMudMotions::Motion_29_SneakToWalk, "sneak_to_walk"},
    {eMudMotions::Motion_30_WalkToSneak, "walk_to_sneak"},
    {eMudMotions::Motion_31_MidSneakToWalk, "mid_sneak_to_walk"},
    {eMudMotions::Motion_32_SneakBegin, "sneak_begin"},
    {eMudMotions::Motion_33_SneakToIdle, "sneak_to_idle"},
    {eMudMotions::Motion_34_MidSneakToIdle, "mid_sneak_to_idle"},
    {eMudMotions::Motion_35_RunJumpBegin, "run_jump_begin"},
    {eMudMotions::Motion_36_RunJumpMid, "run_jump_mid"},
    {eMudMotions::Motion_37_StandToRun, "stand_to_run"},
    {eMudMotions::Motion_38_Punch, "punch"},
    {eMudMotions::Motion_39_HoistBegin, "hoist_begin"},
    {eMudMotions::Motion_40_HoistLand, "hoist_land"},
    {eMudMotions::Motion_41_LandSoft1, "land_soft1"},
    {eMudMotions::Motion_42_LandSoft2, "land_soft2"},
    {eMudMotions::Motion_43_DunnoBegin, "dunno_begin"},
    {eMudMotions::Motion_44_DunnoEnd, "dunno_end"},
    {eMudMotions::Motion_45_KnockForward, "knock_forward"},
    {eMudMotions::Motion_46_Knockback, "knockback"},
    {eMudMotions::Motion_47_KnockbackGetUp, "knockback_get_up"},
    {eMudMotions::Motion_48_WalkOffEdge, "walk_off_edge"},
    {eMudMotions::Motion_49_Fall, "fall"},
    {eMudMotions::Motion_50_Chant, "chant"},
    {eMudMotions::Motion_51_ChantEnd, "chant_end"},
    {eMudMotions::Motion_52_ToDuck, "to_duck"},
    {eMudMotions::Motion_53_Duck, "duck"},
    {eMudMotions::Motion_54_DuckToCrouch, "duck_to_crouch"},
    {eMudMotions::Motion_55_DuckKnockback, "duck_knockback"},
    {eMudMotions::Motion_56_SlapOwnHead, "slap_own_head"},
    {eMudMotions::Motion_57_TurnWheelBegin, "turn_wheel_begin"},
    {eMudMotions::Motion_58_TurnWheelLoop, "turn_wheel_loop"},
    {eMudMotions::Motion_59_TurnWheelEnd, "turn_wheel_end"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(GameSpeakEvents, {
    {GameSpeakEvents::eSameAsLast, "same_as_last"},
    {GameSpeakEvents::eNone, "none"},
    {GameSpeakEvents::eUnknown_0, "unknown_0"},
    {GameSpeakEvents::eUnknown_1, "unknown_1"},
    {GameSpeakEvents::eAbe_Fart, "fart"},
    {GameSpeakEvents::eSlig_BS, "slig_bs"},
    {GameSpeakEvents::eSlig_LookOut, "slig_look_out"},
    {GameSpeakEvents::eSlig_BS2, "slig_bs2"},
    {GameSpeakEvents::eSlig_Laugh, "slig_laugh"},
    {GameSpeakEvents::eAbe_Hello, "hello"},
    {GameSpeakEvents::eAbe_FollowMe, "follow_me"},
    {GameSpeakEvents::eAbe_Anger, "anger"},
    {GameSpeakEvents::eAbe_Wait, "wait"},
    {GameSpeakEvents::eAbe_Work, "work"},
    {GameSpeakEvents::eAbe_StopIt, "stop_it"},
    {GameSpeakEvents::eAbe_AllYa, "allya"},
    {GameSpeakEvents::eAbe_Sorry, "sorry"},
    {GameSpeakEvents::eSlig_Hi, "slig_hi"},
    {GameSpeakEvents::eSlig_HereBoy, "slig_here_boy"},
    {GameSpeakEvents::eSlig_GetEm, "slig_get_em"},
    {GameSpeakEvents::eSlig_Freeze, "slig_freeze"},
    {GameSpeakEvents::eGlukkon_None, "unknown_35"},
    {GameSpeakEvents::eGlukkon_Hey, "glukkon_hey"},
    {GameSpeakEvents::eGlukkon_DoIt, "glukkon_do_it"},
    {GameSpeakEvents::eGlukkon_StayHere, "glukkon_stay_here"},
    {GameSpeakEvents::eGlukkon_Commere, "glukkon_commere"},
    {GameSpeakEvents::eGlukkon_AllOYa, "glukkon_alloya"},
    {GameSpeakEvents::eGlukkon_Heh, "glukkon_heh"},
    {GameSpeakEvents::eGlukkon_Help, "glukkon_help"},
    {GameSpeakEvents::eGlukkon_Laugh, "glukkon_laugh"},
    {GameSpeakEvents::eGlukkon_KillEm, "glukkon_kill_em"},
    {GameSpeakEvents::eGlukkon_Unknown, "glukkon_unknown_45"},
    {GameSpeakEvents::eGlukkon_What, "glukkon_what"},
    {GameSpeakEvents::eParamite_Howdy, "paramite_howdy"},
    {GameSpeakEvents::eParamite_Stay, "paramite_stay"},
    {GameSpeakEvents::eParamite_CMon_or_Attack, "paramite_cmon_or_attack"},
    {GameSpeakEvents::eParamite_DoIt, "paramite_do_it"},
    {GameSpeakEvents::eParamite_AllYa, "paramite_allya"},
    {GameSpeakEvents::eScrab_Howl, "scrab_howl"},
    {GameSpeakEvents::eScrab_Shriek, "scrab_shriek"},
})

inline void to_json(nlohmann::json& j, const MudokonSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"xpos", p.field_4_xpos},
        {"ypos", p.field_8_ypos},
        {"velx", p.field_C_velx},
        {"vely", p.field_10_vely},
        {"path_number", p.field_14_path_number},
        {"lvl_number", p.field_16_lvl_number},
        {"sprite_scale", p.field_18_sprite_scale},
        {"r", p.field_1C_r},
        {"g", p.field_1E_g},
        {"b", p.field_20_b},
        {"flip_x", p.field_22_bFlipX},
        {"field_24_current_motion", p.field_24_current_motion},
        {"anim_current_frame", p.field_26_anim_current_frame},
        {"anim_frame_change_counter", p.field_28_anim_frame_change_counter},
        {"anim_render", p.field_2A_bAnimRender},
        {"drawable", p.field_2B_bDrawable},
        {"health", p.field_2C_health},
        {"field_30_current_motion", p.field_30_current_motion},
        {"next_motion", p.field_32_next_motion},
        {"last_line_ypos", p.field_34_lastLineYPos},
        {"line_type", p.field_36_line_type},
        {"can_be_possessed", p.field_3C_can_be_possessed},
        {"is_player", p.field_3D_bIsPlayer},
        {"tlv_info", p.field_40_tlvInfo},
        {"velx_slow_by", p.field_44_velx_slow_by},
        {"portal_id", p.field_4C_portal_id},
        {"angry_trigger", p.field_50_angry_trigger},
        {"laugh_and_crouch_timer", p.field_54_laugh_and_crouch_timer},
        {"angry_timer", p.field_58_angry_timer},
        {"voice_pitch", p.field_5E_voice_pitch},
        {"wheel_id", p.field_60_wheel_id},
        {"field_68", p.field_68},
        {"max_x_offset", p.field_6A_maxXOffset},
        {"not_rescued", p.mNotRescued},
        {"persist_and_reset_offscreen", p.mPersistAndResetOffscreen},
        {"alerted", p.mAlerted},
        {"blind", p.mBlind},
        {"following_abe", p.mFollowingAbe},
        {"standing_for_sad_or_angry", p.mStandingForSadOrAngry},
        {"stopped_at_wheel", p.mStoppedAtWheel},
        {"do_angry", p.mDoAngry},
        {"seen_while_sick", p.mSeenWhileSick},
        {"work_after_turning_wheel", p.mWorkAfterTurningWheel},
        {"return_to_previous_motion", p.mReturnToPreviousMotion},
        {"get_depressed", p.mGetDepressed},
        {"alert_enemies", p.mAlertEnemies},
        {"noise_unknown", p.mNoiseUnknown},
        {"make_sad_noise", p.mMakeSadNoise},
        {"ring_and_angry_mud_timeout", p.mRingAndAngryMudTimeout},
        {"abe_has_ring", p.mAbeHasRing},
        {"is_mud_standing_up2", p.mIsMudStandingUp2},
        {"brain_sub_state2", p.field_70_brain_sub_state2},
        {"stand_idle_timer", p.field_72_stand_idle_timer},
        {"delayed_speak", p.field_74_delayed_speak},
        {"emo_tlb", p.field_76_emo_tlb},
        {"speak_event", p.field_78_speak_event},
        {"motion", p.field_7A_motion},
        {"brain_state", p.field_7C_brain_state},
        {"brain_sub_state", p.field_7E_brain_sub_state},
        {"timer", p.field_80_timer},
        {"response_entry_idx", p.field_84_response_entry_idx},
        {"anim", p.mAnim},
        {"previous_motion_saved", p.mPreviousMotionSaved},
        {"previous_motion", p.mPreviousMotion},
        {"motion_changed", p.mbMotionChanged},
        {"escaping", p.mEscaping},
        {"turning_wheel_timer", p.mTurningWheelTimer},
    };
}

inline void from_json(const nlohmann::json& j, MudokonSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("xpos").get_to(p.field_4_xpos);
    j.at("ypos").get_to(p.field_8_ypos);
    j.at("velx").get_to(p.field_C_velx);
    j.at("vely").get_to(p.field_10_vely);
    j.at("path_number").get_to(p.field_14_path_number);
    j.at("lvl_number").get_to(p.field_16_lvl_number);
    j.at("sprite_scale").get_to(p.field_18_sprite_scale);
    j.at("r").get_to(p.field_1C_r);
    j.at("g").get_to(p.field_1E_g);
    j.at("b").get_to(p.field_20_b);
    j.at("flip_x").get_to(p.field_22_bFlipX);
    j.at("field_24_current_motion").get_to(p.field_24_current_motion);
    j.at("anim_current_frame").get_to(p.field_26_anim_current_frame);
    j.at("anim_frame_change_counter").get_to(p.field_28_anim_frame_change_counter);
    j.at("anim_render").get_to(p.field_2A_bAnimRender);
    j.at("drawable").get_to(p.field_2B_bDrawable);
    j.at("health").get_to(p.field_2C_health);
    j.at("field_30_current_motion").get_to(p.field_30_current_motion);
    j.at("next_motion").get_to(p.field_32_next_motion);
    j.at("last_line_ypos").get_to(p.field_34_lastLineYPos);
    j.at("line_type").get_to(p.field_36_line_type);
    j.at("can_be_possessed").get_to(p.field_3C_can_be_possessed);
    j.at("is_player").get_to(p.field_3D_bIsPlayer);
    j.at("tlv_info").get_to(p.field_40_tlvInfo);
    j.at("velx_slow_by").get_to(p.field_44_velx_slow_by);
    j.at("portal_id").get_to(p.field_4C_portal_id);
    j.at("angry_trigger").get_to(p.field_50_angry_trigger);
    j.at("laugh_and_crouch_timer").get_to(p.field_54_laugh_and_crouch_timer);
    j.at("angry_timer").get_to(p.field_58_angry_timer);
    j.at("voice_pitch").get_to(p.field_5E_voice_pitch);
    j.at("wheel_id").get_to(p.field_60_wheel_id);
    j.at("field_68").get_to(p.field_68);
    j.at("max_x_offset").get_to(p.field_6A_maxXOffset);
    j.at("not_rescued").get_to(p.mNotRescued);
    j.at("persist_and_reset_offscreen").get_to(p.mPersistAndResetOffscreen);
    j.at("alerted").get_to(p.mAlerted);
    j.at("blind").get_to(p.mBlind);
    j.at("following_abe").get_to(p.mFollowingAbe);
    j.at("standing_for_sad_or_angry").get_to(p.mStandingForSadOrAngry);
    j.at("stopped_at_wheel").get_to(p.mStoppedAtWheel);
    j.at("do_angry").get_to(p.mDoAngry);
    j.at("seen_while_sick").get_to(p.mSeenWhileSick);
    j.at("work_after_turning_wheel").get_to(p.mWorkAfterTurningWheel);
    j.at("return_to_previous_motion").get_to(p.mReturnToPreviousMotion);
    j.at("get_depressed").get_to(p.mGetDepressed);
    j.at("alert_enemies").get_to(p.mAlertEnemies);
    j.at("noise_unknown").get_to(p.mNoiseUnknown);
    j.at("make_sad_noise").get_to(p.mMakeSadNoise);
    j.at("ring_and_angry_mud_timeout").get_to(p.mRingAndAngryMudTimeout);
    j.at("abe_has_ring").get_to(p.mAbeHasRing);
    j.at("is_mud_standing_up2").get_to(p.mIsMudStandingUp2);
    j.at("brain_sub_state2").get_to(p.field_70_brain_sub_state2);
    j.at("stand_idle_timer").get_to(p.field_72_stand_idle_timer);
    j.at("delayed_speak").get_to(p.field_74_delayed_speak);
    j.at("emo_tlb").get_to(p.field_76_emo_tlb);
    j.at("speak_event").get_to(p.field_78_speak_event);
    j.at("motion").get_to(p.field_7A_motion);
    j.at("brain_state").get_to(p.field_7C_brain_state);
    j.at("brain_sub_state").get_to(p.field_7E_brain_sub_state);
    j.at("timer").get_to(p.field_80_timer);
    j.at("response_entry_idx").get_to(p.field_84_response_entry_idx);
    if (j.contains("anim"))
    {
        j.at("anim").get_to(p.mAnim);
    }
    if (j.contains("previous_motion_saved") && j.at("previous_motion_saved").get<bool>())
    {
        p.mPreviousMotionSaved = true;
        j.at("previous_motion").get_to(p.mPreviousMotion);
        j.at("motion_changed").get_to(p.mbMotionChanged);
        j.at("escaping").get_to(p.mEscaping);
        j.at("turning_wheel_timer").get_to(p.mTurningWheelTimer);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(MeatStates, {
    {MeatStates::eCreated_0, "created"},
    {MeatStates::eIdle_1, "idle"},
    {MeatStates::eBeingThrown_2, "being_thrown"},
    {MeatStates::eBecomeAPickUp_3, "become_a_pick_up"},
    {MeatStates::eWaitForPickUp_4, "wait_for_pick_up"},
    {MeatStates::eFall_5, "fall"},
})

inline void to_json(nlohmann::json& j, const MeatSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"tlv_id", p.mTlvId},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"velx", p.mVelX},
        {"vely", p.mVelY},
        {"sprite_scale", p.mSpriteScale},
        {"current_path", p.mCurrentPath},
        {"current_level", p.mCurrentLevel},
        {"render", p.mRender},
        {"drawable", p.mDrawable},
        {"loop", p.mLoop},
        {"interactive", p.mInteractive},
        {"platform_id", p.mPlatformId},
        {"line_type", p.mLineType},
        {"throwable_count", p.mThrowableCount},
        {"state", p.mState},
        {"previous_xpos", p.mPreviousXPos},
        {"previous_ypos", p.mPreviousYPos},
        {"dead_timer", p.mDeadTimer},
        {"shimmer_timer_saved", p.mShimmerTimerSaved},
        {"shimmer_timer", p.mShimmerTimer},
    };
}

inline void from_json(const nlohmann::json& j, MeatSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("tlv_id").get_to(p.mTlvId);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("velx").get_to(p.mVelX);
    j.at("vely").get_to(p.mVelY);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("current_path").get_to(p.mCurrentPath);
    j.at("current_level").get_to(p.mCurrentLevel);
    j.at("render").get_to(p.mRender);
    j.at("drawable").get_to(p.mDrawable);
    j.at("loop").get_to(p.mLoop);
    j.at("interactive").get_to(p.mInteractive);
    j.at("platform_id").get_to(p.mPlatformId);
    j.at("line_type").get_to(p.mLineType);
    j.at("throwable_count").get_to(p.mThrowableCount);
    j.at("state").get_to(p.mState);
    j.at("previous_xpos").get_to(p.mPreviousXPos);
    j.at("previous_ypos").get_to(p.mPreviousYPos);
    j.at("dead_timer").get_to(p.mDeadTimer);
    if (j.contains("shimmer_timer_saved") && j.at("shimmer_timer_saved").get<bool>())
    {
        p.mShimmerTimerSaved = true;
        j.at("shimmer_timer").get_to(p.mShimmerTimer);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(MineCarStates, {
    {MineCarStates::eParkedWithoutAbe_0, "parked_without_abe"},
    {MineCarStates::eParkedWithAbe_1, "parked_with_abe"},
    {MineCarStates::eMoving_2, "moving"},
    {MineCarStates::eFalling_3, "falling"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(MineCarDirs, {
    {MineCarDirs::eDown_0, "down"},
    {MineCarDirs::eRight_1, "right"},
    {MineCarDirs::eLeft_2, "left"},
    {MineCarDirs::eUp_3, "up"},
})

inline void to_json(nlohmann::json& j, const MineCarSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"xpos", p.field_4_xpos},
        {"ypos", p.field_8_ypos},
        {"velx", p.field_C_velx},
        {"vely", p.field_10_vely},
        {"sprite_scale", p.field_14_sprite_scale},
        {"path_number", p.field_18_path_number},
        {"lvl_number", p.field_1A_lvl_number},
        {"r", p.field_1C_r},
        {"g", p.field_1E_g},
        {"b", p.field_20_b},
        {"x_flip", p.field_22_xFlip},
        {"frame_table", p.field_24_frame_table},
        {"current_anim_frame", p.field_2A_current_anim_frame},
        {"frame_change_counter", p.field_2C_frame_change_counter},
        {"render", p.field_2E_render},
        {"drawable", p.field_2F_drawable},
        {"frame_table_offset2", p.field_38_frame_table_offset2},
        {"health", p.field_3C_health},
        {"last_line_ypos", p.field_44_last_line_ypos},
        {"collision_line_type", p.field_46_collision_line_type},
        {"tlv_info", p.field_4C_tlvInfo},
        {"state", p.field_50_state},
        {"turn_direction", p.field_52_turn_direction},
        {"falling_counter", p.field_58_falling_counter},
        {"abe_in_car", p.field_5A_bAbeInCar},
        {"frame_mod_16", p.field_5C_frame_mod_16},
        {"spawned_path", p.field_60_spawned_path},
        {"spawned_camera", p.field_62_spawned_camera},
        {"throw_item_key1", p.field_64_throw_item_key1},
        {"continue_move_input", p.field_66_continue_move_input},
        {"velx_index_saved", p.mVelxIndexSaved},
        {"velx_index", p.field_1C4_velx_index},
        {"tread_anim_saved", p.mTreadAnimSaved},
        {"tread_anim", p.mTreadAnim},
    };
}

inline void from_json(const nlohmann::json& j, MineCarSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("xpos").get_to(p.field_4_xpos);
    j.at("ypos").get_to(p.field_8_ypos);
    j.at("velx").get_to(p.field_C_velx);
    j.at("vely").get_to(p.field_10_vely);
    j.at("sprite_scale").get_to(p.field_14_sprite_scale);
    j.at("path_number").get_to(p.field_18_path_number);
    j.at("lvl_number").get_to(p.field_1A_lvl_number);
    j.at("r").get_to(p.field_1C_r);
    j.at("g").get_to(p.field_1E_g);
    j.at("b").get_to(p.field_20_b);
    j.at("x_flip").get_to(p.field_22_xFlip);
    j.at("frame_table").get_to(p.field_24_frame_table);
    j.at("current_anim_frame").get_to(p.field_2A_current_anim_frame);
    j.at("frame_change_counter").get_to(p.field_2C_frame_change_counter);
    j.at("render").get_to(p.field_2E_render);
    j.at("drawable").get_to(p.field_2F_drawable);
    j.at("frame_table_offset2").get_to(p.field_38_frame_table_offset2);
    j.at("health").get_to(p.field_3C_health);
    j.at("last_line_ypos").get_to(p.field_44_last_line_ypos);
    j.at("collision_line_type").get_to(p.field_46_collision_line_type);
    j.at("tlv_info").get_to(p.field_4C_tlvInfo);
    j.at("state").get_to(p.field_50_state);
    j.at("turn_direction").get_to(p.field_52_turn_direction);
    j.at("falling_counter").get_to(p.field_58_falling_counter);
    j.at("abe_in_car").get_to(p.field_5A_bAbeInCar);
    j.at("frame_mod_16").get_to(p.field_5C_frame_mod_16);
    j.at("spawned_path").get_to(p.field_60_spawned_path);
    j.at("spawned_camera").get_to(p.field_62_spawned_camera);
    j.at("throw_item_key1").get_to(p.field_64_throw_item_key1);
    j.at("continue_move_input").get_to(p.field_66_continue_move_input);
    if (j.contains("velx_index_saved") && j.at("velx_index_saved").get<bool>())
    {
        p.mVelxIndexSaved = true;
        j.at("velx_index").get_to(p.field_1C4_velx_index);
    }
    if (j.contains("tread_anim_saved") && j.at("tread_anim_saved").get<bool>())
    {
        p.mTreadAnimSaved = true;
        j.at("tread_anim").get_to(p.mTreadAnim);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(eParamiteMotions, {
    {eParamiteMotions::eNone_m1, "none"},
    {eParamiteMotions::Motion_0_Idle, "idle"},
    {eParamiteMotions::Motion_1_WalkBegin, "walk_begin"},
    {eParamiteMotions::Motion_2_Walking, "walking"},
    {eParamiteMotions::Motion_3_Running, "running"},
    {eParamiteMotions::Motion_4_Turn, "turn"},
    {eParamiteMotions::Motion_5_Hop, "hop"},
    {eParamiteMotions::Motion_6_Unused, "motion_6_unused"},
    {eParamiteMotions::Motion_7_WalkRunTransition, "walk_run_transition"},
    {eParamiteMotions::Motion_8_WalkEnd, "walk_end"},
    {eParamiteMotions::Motion_9_RunBegin, "run_begin"},
    {eParamiteMotions::Motion_10_RunEnd, "run_end"},
    {eParamiteMotions::Motion_11_Falling, "falling"},
    {eParamiteMotions::Motion_12_JumpUpBegin, "jump_up_begin"},
    {eParamiteMotions::Motion_13_JumpUpMidair, "jump_up_midair"},
    {eParamiteMotions::Motion_14_JumpUpLand, "jump_up_land"},
    {eParamiteMotions::Motion_15_RopePull, "rope_pull"},
    {eParamiteMotions::Motion_16_CloseAttack, "close_attack"},
    {eParamiteMotions::Motion_17_Landing, "landing"},
    {eParamiteMotions::Motion_18_Unused, "motion_18_unused"},
    {eParamiteMotions::Motion_19_Knockback, "knockback"},
    {eParamiteMotions::Motion_20_GameSpeakBegin, "gamespeak_begin"},
    {eParamiteMotions::Motion_21_PreHiss, "pre_hiss"},
    {eParamiteMotions::Motion_22_Hiss1, "hiss1"},
    {eParamiteMotions::Motion_23_Hiss2, "hiss2"},
    {eParamiteMotions::Motion_24_Empty, "motion_24_empty"},
    {eParamiteMotions::Motion_25_AllOYaGameSpeakBegin, "all_o_ya_gamespeak_begin"},
    {eParamiteMotions::Motion_26_Hiss3, "hiss3"},
    {eParamiteMotions::Motion_27_PostHiss, "post_hiss"},
    {eParamiteMotions::Motion_28_GameSpeakEnd, "gamespeak_end"},
    {eParamiteMotions::Motion_29_GetDepossessedBegin, "get_depossessed_begin"},
    {eParamiteMotions::Motion_30_GetDepossessedEnd, "get_depossessed_end"},
    {eParamiteMotions::Motion_31_RunningAttack, "running_attack"},
    {eParamiteMotions::Motion_32_Empty, "motion_32_empty"},
    {eParamiteMotions::Motion_33_SurpriseWeb, "surprise_web"},
    {eParamiteMotions::Motion_34_WebLeaveDown, "web_leave_down"},
    {eParamiteMotions::Motion_35_WebIdle, "web_idle"},
    {eParamiteMotions::Motion_36_WebGoingUp, "web_going_up"},
    {eParamiteMotions::Motion_37_WebGoingDown, "web_going_down"},
    {eParamiteMotions::Motion_38_WebGrab, "web_grab"},
    {eParamiteMotions::Motion_39_WebLeaveUp, "web_leave_up"},
    {eParamiteMotions::Motion_40_Eating, "eating"},
    {eParamiteMotions::Motion_41_Death, "death"},
    {eParamiteMotions::Motion_42_Squawk, "squawk"},
    {eParamiteMotions::Motion_43_Attack, "attack"},
})

inline void to_json(nlohmann::json& j, const ParamiteSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"velx", p.mVelX},
        {"vely", p.mVelY},
        {"path_number", p.mCurrentPath},
        {"lvl_number", p.mCurrentLevel},
        {"sprite_scale", p.mSpriteScale},
        {"r", p.mR},
        {"g", p.mG},
        {"b", p.mB},
        {"x_flip", p.mFlipX},
        {"field_24_current_motion", p.field_24_current_motion},
        {"anim_current_frame", p.mAnimCurrentFrame},
        {"frame_change_counter", p.mFrameChangeCounter},
        {"render", p.mRender},
        {"drawable", p.mDrawable},
        {"health", p.mHealth},
        {"field_30_current_motion", p.field_30_current_motion},
        {"next_motion", p.field_32_next_motion},
        {"last_line_ypos", p.field_34_last_line_ypos},
        {"line_type", p.field_36_line_type},
        {"tlv_info", p.field_3C_tlvInfo},
        {"meat_id", p.field_40_meat_id},
        {"web_id", p.field_44_web_id},
        {"obj_id", p.field_48_obj_id},
        {"pull_ring_rope_id", p.field_4C_pull_ring_rope_id},
        {"brain_idx", p.mBrainIdx},
        {"brain_ret", p.mBrainSubState},
        {"field_5c_timer", p.field_5C_timer},
        {"depossession_timer", p.field_60_depossession_timer},
        {"velx_offset", p.field_64_velx_offset},
        {"field_68_timer", p.field_68_timer},
        {"return_level", p.mAbeLevel},
        {"return_path", p.mAbePath},
        {"return_camera", p.mAbeCamera},
        {"input", p.field_72_input},
        {"input_raw", p.mInputRaw},
        {"next_brain_ret", p.field_74_next_brain_ret},
        {"controlled", p.mControlled},
        {"running", p.mRunning},
        {"hissed_or_left_screen", p.mHissedOrLeftScreen},
        {"prevent_depossession", p.mPreventDepossession},
        {"spawned", p.mSpawned},
        {"alerted", p.mAlerted},
        {"can_be_possessed", p.mCanBePossessed},
        {"previous_motion_saved", p.mPreviousMotionSaved},
        {"previous_motion", p.mPreviousMotion},
        {"gamespeak_next_motion", p.mGamespeakNextMotion},
        {"paramite_x_offset_saved", p.mParamiteXOffsetSaved},
        {"paramite_x_offset", p.mParamiteXOffset},
    };
}

inline void from_json(const nlohmann::json& j, ParamiteSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("velx").get_to(p.mVelX);
    j.at("vely").get_to(p.mVelY);
    j.at("path_number").get_to(p.mCurrentPath);
    j.at("lvl_number").get_to(p.mCurrentLevel);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("r").get_to(p.mR);
    j.at("g").get_to(p.mG);
    j.at("b").get_to(p.mB);
    j.at("x_flip").get_to(p.mFlipX);
    j.at("field_24_current_motion").get_to(p.field_24_current_motion);
    j.at("anim_current_frame").get_to(p.mAnimCurrentFrame);
    j.at("frame_change_counter").get_to(p.mFrameChangeCounter);
    j.at("render").get_to(p.mRender);
    j.at("drawable").get_to(p.mDrawable);
    j.at("health").get_to(p.mHealth);
    j.at("field_30_current_motion").get_to(p.field_30_current_motion);
    j.at("next_motion").get_to(p.field_32_next_motion);
    j.at("last_line_ypos").get_to(p.field_34_last_line_ypos);
    j.at("line_type").get_to(p.field_36_line_type);
    j.at("tlv_info").get_to(p.field_3C_tlvInfo);
    j.at("meat_id").get_to(p.field_40_meat_id);
    j.at("web_id").get_to(p.field_44_web_id);
    j.at("obj_id").get_to(p.field_48_obj_id);
    j.at("pull_ring_rope_id").get_to(p.field_4C_pull_ring_rope_id);
    j.at("brain_idx").get_to(p.mBrainIdx);
    j.at("brain_ret").get_to(p.mBrainSubState);
    j.at("field_5c_timer").get_to(p.field_5C_timer);
    j.at("depossession_timer").get_to(p.field_60_depossession_timer);
    j.at("velx_offset").get_to(p.field_64_velx_offset);
    j.at("field_68_timer").get_to(p.field_68_timer);
    j.at("return_level").get_to(p.mAbeLevel);
    j.at("return_path").get_to(p.mAbePath);
    j.at("return_camera").get_to(p.mAbeCamera);
    j.at("input").get_to(p.field_72_input);
    j.at("input_raw").get_to(p.mInputRaw);
    j.at("next_brain_ret").get_to(p.field_74_next_brain_ret);
    j.at("controlled").get_to(p.mControlled);
    j.at("running").get_to(p.mRunning);
    j.at("hissed_or_left_screen").get_to(p.mHissedOrLeftScreen);
    j.at("prevent_depossession").get_to(p.mPreventDepossession);
    j.at("spawned").get_to(p.mSpawned);
    j.at("alerted").get_to(p.mAlerted);
    j.at("can_be_possessed").get_to(p.mCanBePossessed);
    p.mGamespeakNextMotion = j.value("gamespeak_next_motion", -1);
    p.mParamiteXOffsetSaved = j.value("paramite_x_offset_saved", false);
    p.mParamiteXOffset = j.value("paramite_x_offset", static_cast<s16>(0));
    if (j.contains("previous_motion_saved") && j.at("previous_motion_saved").get<bool>())
    {
        p.mPreviousMotionSaved = true;
        j.at("previous_motion").get_to(p.mPreviousMotion);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(PortalStates, {
    {PortalStates::CreatePortal_0, "create_portal"},
    {PortalStates::IdlePortal_1, "idle_portal"},
    {PortalStates::JoinDovesInCenter_2, "join_doves_in_center"},
    {PortalStates::KillDoves_3, "kill_doves"},
    {PortalStates::CreateTerminators_4, "create_terminators"},
    {PortalStates::ExpandTerminators_5, "expand_terminators"},
    {PortalStates::ActivePortal_6, "active_portal"},
    {PortalStates::ShrykullGetDoves_7, "shrykull_get_doves"},
    {PortalStates::Unused_8, "unused_8"},
    {PortalStates::GetShrykull_9, "get_shrykull"},
    {PortalStates::CollapseTerminators_10, "collapse_terminators"},
    {PortalStates::StopSound_11, "stop_sound"},
    {PortalStates::CreateFlash1_12, "create_flash1"},
    {PortalStates::CreateFlash2_13, "create_flash2"},
    {PortalStates::CreateFlash3_14, "create_flash3"},
    {PortalStates::KillPortal_15, "kill_portal"},
    {PortalStates::AbeInsidePortal_16, "abe_inside_portal"},
    {PortalStates::PortalExit_SetPosition_17, "portal_exit_set_position"},
    {PortalStates::PortalExit_CreateTerminators_18, "portal_exit_create_terminators"},
    {PortalStates::PortalExit_ExpandTerminators_19, "portal_exit_expand_terminators"},
    {PortalStates::PortalExit_AbeExitting_20, "portal_exit_abe_exitting"},
    {PortalStates::KillPortalClipper_21, "kill_portal_clipper"},
    {PortalStates::FadeoutTerminators_22, "fadeout_terminators"},
})

inline void to_json(nlohmann::json& j, const BirdPortalBaseState& p)
{
    nlohmann::json doveIds = nlohmann::json::array();
    for (s32 i = 0; i < 6; i++)
    {
        doveIds.push_back(p.mDoveIds[i]);
    }
    j = nlohmann::json{
        {"doves_exist", p.mDovesExist},
        {"state", p.mState},
        {"timer", p.mTimer},
        {"received_doves_count", p.mReceivedDovesCount},
        {"mud_count_for_shrykull", p.mMudCountForShrykull},
        {"hit_y", p.mHitY},
        {"throwable_total_indicator", p.mThrowableTotalIndicator},
        {"dove_ids", doveIds},
        {"terminator_id1", p.mTerminatorId1},
        {"terminator_id2", p.mTerminatorId2},
        {"screen_clipper_id1", p.mScreenClipperId1},
        {"screen_clipper_id2", p.mScreenClipperId2},
        {"orb_whirl_wind_id", p.mOrbWhirlWindId},
    };
}

inline void from_json(const nlohmann::json& j, BirdPortalBaseState& p)
{
    p.mFullState = true;
    j.at("doves_exist").get_to(p.mDovesExist);
    j.at("state").get_to(p.mState);
    j.at("timer").get_to(p.mTimer);
    j.at("received_doves_count").get_to(p.mReceivedDovesCount);
    j.at("mud_count_for_shrykull").get_to(p.mMudCountForShrykull);
    j.at("hit_y").get_to(p.mHitY);
    j.at("throwable_total_indicator").get_to(p.mThrowableTotalIndicator);
    const nlohmann::json& doveIds = j.at("dove_ids");
    for (s32 i = 0; i < 6 && i < static_cast<s32>(doveIds.size()); i++)
    {
        doveIds.at(i).get_to(p.mDoveIds[i]);
    }
    j.at("terminator_id1").get_to(p.mTerminatorId1);
    j.at("terminator_id2").get_to(p.mTerminatorId2);
    j.at("screen_clipper_id1").get_to(p.mScreenClipperId1);
    j.at("screen_clipper_id2").get_to(p.mScreenClipperId2);
    j.at("orb_whirl_wind_id").get_to(p.mOrbWhirlWindId);
}

inline void to_json(nlohmann::json& j, const BirdPortalSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"state", p.mState},
        {"mud_count_for_shrykull", p.mMudCountForShrykull},
        {"tlv_info", p.mTlvInfo},
        {"idle_saved", p.mIdleSaved},
        {"doves_exist", p.mDovesExist},
        {"timer", p.mTimer},
        {"dove_ids", p.mDoveIds},
        {"base", p.mBase},
    };
}

inline void from_json(const nlohmann::json& j, BirdPortalSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("state").get_to(p.mState);
    j.at("mud_count_for_shrykull").get_to(p.mMudCountForShrykull);
    j.at("tlv_info").get_to(p.mTlvInfo);
    // Saves from before the idle portal was saved don't have these
    p.mIdleSaved = j.value("idle_saved", false);
    if (p.mIdleSaved)
    {
        j.at("doves_exist").get_to(p.mDovesExist);
        j.at("timer").get_to(p.mTimer);
        j.at("dove_ids").get_to(p.mDoveIds);
    }
    if (j.contains("base"))
    {
        j.at("base").get_to(p.mBase);
    }
}

inline void to_json(nlohmann::json& j, const ThrowableArraySaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"count", p.mCount},
    };
}

inline void from_json(const nlohmann::json& j, ThrowableArraySaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("count").get_to(p.mCount);
}

NLOHMANN_JSON_SERIALIZE_ENUM(RingTypes, {
    {RingTypes::eExplosive_Pulse_0, "explosive_pulse"},
    {RingTypes::eExplosive_Emit_1, "explosive_emit"},
    {RingTypes::eExplosive_Emit_Effect_2, "explosive_emit_effect"},
    {RingTypes::eExplosive_Give_3, "explosive_give"},
    {RingTypes::eShrykull_Pulse_Small_4, "shrykull_pulse_small"},
    {RingTypes::eShrykull_Pulse_Large_5, "shrykull_pulse_large"},
    {RingTypes::eShrykull_Pulse_Orange_6, "shrykull_pulse_orange"},
    {RingTypes::eInvisible_Pulse_Small_7, "invisible_pulse_small"},
    {RingTypes::eInvisible_Pulse_Large_8, "invisible_pulse_large"},
    {RingTypes::eInvisible_Pulse_Emit_9, "invisible_pulse_emit"},
    {RingTypes::eInvisible_Pulse_Give_10, "invisible_pulse_give"},
    {RingTypes::eHealing_Emit_Effect_11, "healing_emit_effect"},
    {RingTypes::eHealing_Emit_12, "healing_emit"},
    {RingTypes::eHealing_Give_13, "healing_give"},
    {RingTypes::eHealing_Pulse_14, "healing_pulse"},
})

inline void to_json(nlohmann::json& j, const AbilityRingSaveState& p)
{
    nlohmann::json rects = nlohmann::json::array();
    for (s32 i = 0; i < 64; i++)
    {
        rects.push_back(nlohmann::json::array({p.mRingCollideRects[i][0], p.mRingCollideRects[i][1], p.mRingCollideRects[i][2], p.mRingCollideRects[i][3]}));
    }
    j = nlohmann::json{
        {"type", p.mType},
        {"ring_xpos", p.mRingXPos},
        {"ring_ypos", p.mRingYPos},
        {"ring_type", p.mRingType},
        {"ring_scale", p.mRingScale},
        {"ring_tlv_info", p.mRingTlvInfo},
        {"ring_right", p.mRingRight},
        {"ring_count", p.mRingCount},
        {"ring_red", p.mRingRed},
        {"ring_green", p.mRingGreen},
        {"ring_blue", p.mRingBlue},
        {"full_state", p.mFullState},
        {"refresh_target_obj_id", p.mRefreshTargetObjId},
        {"ring_layer", p.mRingLayer},
        {"ring_left", p.mRingLeft},
        {"ring_speed", p.mRingSpeed},
        {"ring_scale_x", p.mRingScaleX},
        {"ring_scale_y", p.mRingScaleY},
        {"ring_thickness", p.mRingThickness},
        {"ring_fadeout_distance", p.mRingFadeoutDistance},
        {"ring_screen_x", p.mRingScreenX},
        {"ring_screen_y", p.mRingScreenY},
        {"ring_screen_x_pos", p.mRingScreenXPos},
        {"ring_screen_y_pos", p.mRingScreenYPos},
        {"ring_semi_trans", p.mRingSemiTrans},
        {"ring_level", p.mRingLevel},
        {"ring_path", p.mRingPath},
        {"ring_target_obj_id", p.mRingTargetObjId},
        {"ring_collide_rects", rects},
    };
}

inline void from_json(const nlohmann::json& j, AbilityRingSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("ring_xpos").get_to(p.mRingXPos);
    j.at("ring_ypos").get_to(p.mRingYPos);
    j.at("ring_type").get_to(p.mRingType);
    j.at("ring_scale").get_to(p.mRingScale);
    j.at("ring_tlv_info").get_to(p.mRingTlvInfo);
    j.at("ring_right").get_to(p.mRingRight);
    j.at("ring_count").get_to(p.mRingCount);
    j.at("ring_red").get_to(p.mRingRed);
    j.at("ring_green").get_to(p.mRingGreen);
    // Older saves have the red twice
    j.at(j.contains("ring_blue") ? "ring_blue" : "ring_red").get_to(p.mRingBlue);
    if (j.contains("full_state") && j.at("full_state").get<bool>())
    {
        p.mFullState = true;
        j.at("refresh_target_obj_id").get_to(p.mRefreshTargetObjId);
        j.at("ring_layer").get_to(p.mRingLayer);
        j.at("ring_left").get_to(p.mRingLeft);
        j.at("ring_speed").get_to(p.mRingSpeed);
        j.at("ring_scale_x").get_to(p.mRingScaleX);
        j.at("ring_scale_y").get_to(p.mRingScaleY);
        j.at("ring_thickness").get_to(p.mRingThickness);
        j.at("ring_fadeout_distance").get_to(p.mRingFadeoutDistance);
        j.at("ring_screen_x").get_to(p.mRingScreenX);
        j.at("ring_screen_y").get_to(p.mRingScreenY);
        j.at("ring_screen_x_pos").get_to(p.mRingScreenXPos);
        j.at("ring_screen_y_pos").get_to(p.mRingScreenYPos);
        j.at("ring_semi_trans").get_to(p.mRingSemiTrans);
        j.at("ring_level").get_to(p.mRingLevel);
        j.at("ring_path").get_to(p.mRingPath);
        j.at("ring_target_obj_id").get_to(p.mRingTargetObjId);
        const nlohmann::json& rects = j.at("ring_collide_rects");
        for (s32 i = 0; i < 64 && i < static_cast<s32>(rects.size()); i++)
        {
            for (s32 k = 0; k < 4; k++)
            {
                rects.at(i).at(k).get_to(p.mRingCollideRects[i][k]);
            }
        }
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(RockStates, {
    {RockStates::eNone_0, "none"},
    {RockStates::eFallingOutOfRockSack_1, "falling_out_of_rock_sack"},
    {RockStates::eRolling_2, "rolling"},
    {RockStates::eOnGround_3, "on_ground"},
    {RockStates::eBouncing_4, "bouncing"},
    {RockStates::eFallingOutOfWorld_5, "falling_out_of_world"},
})

inline void to_json(nlohmann::json& j, const RockSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"obj_id", p.mTlvId},
        {"field_8_xpos", p.mXPos},
        {"field_c_ypos", p.mYPos},
        {"velx", p.mVelX},
        {"vely", p.mVelY},
        {"sprite_scale", p.mSpriteScale},
        {"path_number", p.mCurrentPath},
        {"lvl_number", p.mCurrentLevel},
        {"render", p.mRender},
        {"drawable", p.mDrawable},
        {"loop", p.mLoop},
        {"interactive", p.mInteractive},
        {"id", p.mPlatformId},
        {"line_type", p.mCollisionLineType},
        {"count", p.mThrowableCount},
        {"state", p.mState},
        {"volume", p.mBounceCount},
        {"field_30_xpos", p.mPreviousXPos},
        {"field_34_ypos", p.mPreviousYPos},
        {"shimmer_timer_saved", p.mShimmerTimerSaved},
        {"shimmer_timer", p.mShimmerTimer},
    };
}

inline void from_json(const nlohmann::json& j, RockSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("obj_id").get_to(p.mTlvId);
    j.at("field_8_xpos").get_to(p.mXPos);
    j.at("field_c_ypos").get_to(p.mYPos);
    j.at("velx").get_to(p.mVelX);
    j.at("vely").get_to(p.mVelY);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("path_number").get_to(p.mCurrentPath);
    j.at("lvl_number").get_to(p.mCurrentLevel);
    j.at("render").get_to(p.mRender);
    j.at("drawable").get_to(p.mDrawable);
    j.at("loop").get_to(p.mLoop);
    j.at("interactive").get_to(p.mInteractive);
    j.at("id").get_to(p.mPlatformId);
    j.at("line_type").get_to(p.mCollisionLineType);
    j.at("count").get_to(p.mThrowableCount);
    j.at("state").get_to(p.mState);
    j.at("volume").get_to(p.mBounceCount);
    j.at("field_30_xpos").get_to(p.mPreviousXPos);
    j.at("field_34_ypos").get_to(p.mPreviousYPos);
    if (j.contains("shimmer_timer_saved") && j.at("shimmer_timer_saved").get<bool>())
    {
        p.mShimmerTimerSaved = true;
        j.at("shimmer_timer").get_to(p.mShimmerTimer);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(eScrabMotions, {
    {eScrabMotions::eNone_m1, "none"},
    {eScrabMotions::Motion_0_Stand, "stand"},
    {eScrabMotions::Motion_1_Walk, "walk"},
    {eScrabMotions::Motion_2_Run, "run"},
    {eScrabMotions::Motion_3_Turn, "turn"},
    {eScrabMotions::Motion_4_RunToStand, "run_to_stand"},
    {eScrabMotions::Motion_5_HopBegin, "hop_begin"},
    {eScrabMotions::Motion_6_HopMidair, "hop_midair"},
    {eScrabMotions::Motion_7_HopLand, "hop_land"},
    {eScrabMotions::Motion_8_JumpToFall, "jump_to_fall"},
    {eScrabMotions::Motion_9_StandToWalk, "stand_to_walk"},
    {eScrabMotions::Motion_10_StandToRun, "stand_to_run"},
    {eScrabMotions::Motion_11_WalkToStand, "walk_to_stand"},
    {eScrabMotions::Motion_12_RunJumpBegin, "run_jump_begin"},
    {eScrabMotions::Motion_13_RunJumpEnd, "run_jump_end"},
    {eScrabMotions::Motion_14_WalkToFall, "walk_to_fall"},
    {eScrabMotions::Motion_15_RunToFall, "run_to_fall"},
    {eScrabMotions::Motion_16_WalkToRun , "walk_to_run"},
    {eScrabMotions::Motion_17_RunToWalk, "run_to_walk"},
    {eScrabMotions::Motion_18_Knockback, "knockback"},
    {eScrabMotions::Motion_19_GetEaten, "get_eaten"},
    {eScrabMotions::Motion_20_Fall, "fall"},
    {eScrabMotions::Motion_21_Stamp, "stamp"},
    {eScrabMotions::Motion_22_GetPossessed, "get_possessed"},
    {eScrabMotions::Motion_23_Empty, "motion_23_empty"},
    {eScrabMotions::Motion_24_DeathEnd, "death_end"},
    {eScrabMotions::Motion_25_Empty, "motion_25_empty"},
    {eScrabMotions::Motion_26_HowlBegin, "howl_begin"},
    {eScrabMotions::Motion_27_HowlEnd, "howl_end"},
    {eScrabMotions::Motion_28_GetDepossessedBegin, "get_depossessed_begin"},
    {eScrabMotions::Motion_29_GetDepossessedEnd, "get_depossessed_end"},
    {eScrabMotions::Motion_30_Shriek, "shriek"},
    {eScrabMotions::Motion_31_ScrabBattleAnim, "scrab_battle_anim"},
    {eScrabMotions::Motion_32_AttackSpin, "attack_spin"},
    {eScrabMotions::Motion_33_FeedToGulp, "feed_to_gulp"},
    {eScrabMotions::Motion_34_GulpToStand, "gulp_to_stand"},
    {eScrabMotions::Motion_35_StandToFeed, "stand_to_feed"},
    {eScrabMotions::Motion_36_Feed, "feed"},
    {eScrabMotions::Motion_37_AttackLunge, "attack_lunge"},
    {eScrabMotions::Motion_38_LegKick, "leg_kick"},
    {eScrabMotions::Motion_39_DeathBegin, "death_begin"},
})

inline void to_json(nlohmann::json& j, const ScrabSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"obj_id", p.field_4_obj_id},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"velx", p.mVelX},
        {"vely", p.mVelY},
        {"path_number", p.mCurrentPath},
        {"lvl_number", p.mCurrentLevel},
        {"sprite_scale", p.mSpriteScale},
        {"ring_red", p.mRingRed},
        {"ring_green", p.mRingGreen},
        {"ring_blue", p.mRingBlue},
        {"anim_flipX", p.mFlipX},
        {"field_28_current_motion", p.field_28_current_motion},
        {"current_frame", p.mCurrentFrame},
        {"frame_change_counter", p.mFrameChangeCounter},
        {"anim_render", p.mAnimRender},
        {"drawable", p.mDrawable},
        {"health", p.mHealth},
        {"field_34_current_motion", p.field_34_current_motion},
        {"next_motion", p.field_36_next_motion},
        {"last_line_ypos", p.field_38_last_line_ypos},
        {"line_type", p.mLineType},
        {"is_controlled", p.mIsControlled},
        {"tlv_info", p.field_44_tlvInfo},
        {"brain_idx", p.field_48_brain_idx},
        {"sub_state", p.mBrainSubState},
        {"brain_obj_id", p.field_54_obj_id},
        {"target_obj_id", p.field_58_target_obj_id},
        {"timer", p.field_5C_timer},
        {"depossession_timer", p.field_60_depossession_timer},
        {"falling_velx_scale_factor", p.field_64_falling_velx_scale_factor},
        {"motion_resource_block_index", p.field_68_motion_resource_block_index},
        {"spotting_abe_timer", p.field_6C_spotting_abe_timer},
        {"attack_delay_timer", p.field_70_attack_delay_timer},
        {"movement_timer", p.field_74_movement_timer},
        {"sfx_bitmask", p.field_78_sfx_bitmask},
        {"prevent_depossession", p.mPreventDepossession},
        {"return_level", p.mAbeLevel},
        {"return_path", p.mAbePath},
        {"return_camera", p.mAbeCamera},
        {"input", p.field_84_input},
        {"input_raw", p.mInputRaw},
        {"shred_power_active", p.field_8C_shred_power_active},
        {"speak", p.field_8E_speak},
        {"max_xpos", p.field_90_max_xpos},
        {"max_ypos", p.field_94_max_ypos},
        {"speak_counter", p.field_98_speak_counter},
        {"attacking", p.mAttacking},
        {"force_update_animation", p.mForceUpdateAnimation},
        {"roar_randomly", p.mRoarRandomly},
        {"persistant", p.mPersistant},
        {"last_ypos", p.field_13C_last_ypos},
        {"previous_motion_saved", p.mPreviousMotionSaved},
        {"previous_motion", p.mPreviousMotion},
    };
}

inline void from_json(const nlohmann::json& j, ScrabSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("obj_id").get_to(p.field_4_obj_id);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("velx").get_to(p.mVelX);
    j.at("vely").get_to(p.mVelY);
    j.at("path_number").get_to(p.mCurrentPath);
    j.at("lvl_number").get_to(p.mCurrentLevel);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("ring_red").get_to(p.mRingRed);
    j.at("ring_green").get_to(p.mRingGreen);
    j.at("ring_blue").get_to(p.mRingBlue);
    j.at("anim_flipX").get_to(p.mFlipX);
    j.at("field_28_current_motion").get_to(p.field_28_current_motion);
    j.at("current_frame").get_to(p.mCurrentFrame);
    j.at("frame_change_counter").get_to(p.mFrameChangeCounter);
    j.at("anim_render").get_to(p.mAnimRender);
    j.at("drawable").get_to(p.mDrawable);
    j.at("health").get_to(p.mHealth);
    j.at("field_34_current_motion").get_to(p.field_34_current_motion);
    j.at("next_motion").get_to(p.field_36_next_motion);
    j.at("last_line_ypos").get_to(p.field_38_last_line_ypos);
    j.at("line_type").get_to(p.mLineType);
    j.at("is_controlled").get_to(p.mIsControlled);
    j.at("tlv_info").get_to(p.field_44_tlvInfo);
    j.at("brain_idx").get_to(p.field_48_brain_idx);
    j.at("sub_state").get_to(p.mBrainSubState);
    j.at(j.contains("brain_obj_id") ? "brain_obj_id" : "obj_id").get_to(p.field_54_obj_id);
    j.at("target_obj_id").get_to(p.field_58_target_obj_id);
    j.at("timer").get_to(p.field_5C_timer);
    j.at("depossession_timer").get_to(p.field_60_depossession_timer);
    j.at("falling_velx_scale_factor").get_to(p.field_64_falling_velx_scale_factor);
    j.at("motion_resource_block_index").get_to(p.field_68_motion_resource_block_index);
    j.at("spotting_abe_timer").get_to(p.field_6C_spotting_abe_timer);
    j.at("attack_delay_timer").get_to(p.field_70_attack_delay_timer);
    j.at("movement_timer").get_to(p.field_74_movement_timer);
    j.at("sfx_bitmask").get_to(p.field_78_sfx_bitmask);
    j.at("prevent_depossession").get_to(p.mPreventDepossession);
    j.at("return_level").get_to(p.mAbeLevel);
    j.at("return_path").get_to(p.mAbePath);
    j.at("return_camera").get_to(p.mAbeCamera);
    j.at("input").get_to(p.field_84_input);
    j.at("input_raw").get_to(p.mInputRaw);
    j.at("shred_power_active").get_to(p.field_8C_shred_power_active);
    j.at("speak").get_to(p.field_8E_speak);
    j.at("max_xpos").get_to(p.field_90_max_xpos);
    j.at("max_ypos").get_to(p.field_94_max_ypos);
    j.at("speak_counter").get_to(p.field_98_speak_counter);
    j.at("attacking").get_to(p.mAttacking);
    j.at("force_update_animation").get_to(p.mForceUpdateAnimation);
    j.at("roar_randomly").get_to(p.mRoarRandomly);
    j.at("persistant").get_to(p.mPersistant);
    if (j.contains("last_ypos"))
    {
        j.at("last_ypos").get_to(p.field_13C_last_ypos);
    }
    if (j.contains("previous_motion_saved") && j.at("previous_motion_saved").get<bool>())
    {
        p.mPreviousMotionSaved = true;
        j.at("previous_motion").get_to(p.mPreviousMotion);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(ScrabSpawnerStates, {
    {ScrabSpawnerStates::eInactive_0, "inactive"},
    {ScrabSpawnerStates::eScrabSpawned_1, "scrab_spawned"},
})

inline void to_json(nlohmann::json& j, const ScrabSpawnerSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"tlv_info", p.field_4_tlvInfo},
        {"state", p.field_8_state},
        {"spawned_scrab_id", p.field_C_spawned_scrab_id},
    };
}

inline void from_json(const nlohmann::json& j, ScrabSpawnerSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("tlv_info").get_to(p.field_4_tlvInfo);
    j.at("state").get_to(p.field_8_state);
    j.at("spawned_scrab_id").get_to(p.field_C_spawned_scrab_id);
}

inline void to_json(nlohmann::json& j, const SlamDoorSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"tlv_info", p.mTlvInfo},
        {"anim", p.mAnim},
    };
}

inline void from_json(const nlohmann::json& j, SlamDoorSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("tlv_info").get_to(p.mTlvInfo);
    if (j.contains("anim"))
    {
        j.at("anim").get_to(p.mAnim);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(eSligMotions, {
    {eSligMotions::eNone_m1, "none"},
    {eSligMotions::Motion_0_StandIdle, "stand_idle"},
    {eSligMotions::Motion_1_StandToWalk, "stand_to_walk"},
    {eSligMotions::Motion_2_Walking, "walking"},
    {eSligMotions::Motion_3_StandToRun, "stand_to_run"},
    {eSligMotions::Motion_4_Running, "running"},
    {eSligMotions::Motion_5_TurnAroundStanding, "turn_around_standing"},
    {eSligMotions::Motion_6_Shoot, "shoot"},
    {eSligMotions::Motion_7_Falling, "falling"},
    {eSligMotions::Motion_8_SlidingToStand, "sliding_to_stand"},
    {eSligMotions::Motion_9_SlidingTurn, "sliding_turn"},
    {eSligMotions::Motion_10_SlidingTurnToWalk, "sliding_turn_to_walk"},
    {eSligMotions::Motion_11_SlidingTurnToRun, "sliding_turn_to_run"},
    {eSligMotions::Motion_12_ReloadGun, "reload_gun"},
    {eSligMotions::Motion_13_ShootToStand, "shoot_to_stand"},
    {eSligMotions::Motion_14_SteppingToStand, "stepping_to_stand"},
    {eSligMotions::Motion_15_StandingToStep, "standing_to_step"},
    {eSligMotions::Motion_16_DepossessingAbort, "depossessing_abort"},
    {eSligMotions::Motion_17_GameSpeak, "gamespeak"},
    {eSligMotions::Motion_18_WalkToStand, "walk_to_stand"},
    {eSligMotions::Motion_19_Recoil, "recoil"},
    {eSligMotions::Motion_20_SpeakHereBoy, "speak_here_boy"},
    {eSligMotions::Motion_21_SpeakHi, "speak_hi"},
    {eSligMotions::Motion_22_SpeakFreeze, "speak_freeze"},
    {eSligMotions::Motion_23_SpeakGetHim, "speak_get_him"},
    {eSligMotions::Motion_24_SpeakLaugh, "speak_laugh"},
    {eSligMotions::Motion_25_SpeakBullshit1, "speak_bullshit1"},
    {eSligMotions::Motion_26_SpeakLookOut, "speak_look_out"},
    {eSligMotions::Motion_27_SpeakBullshit2, "speak_bullshit2"},
    {eSligMotions::Motion_28_SpeakPanic, "speak_panic"},
    {eSligMotions::Motion_29_SpeakWhat, "speak_what"},
    {eSligMotions::Motion_30_SpeakAIFreeze, "speak_ai_freeze"},
    {eSligMotions::Motion_31_Blurgh, "blurgh"},
    {eSligMotions::Motion_32_Sleeping, "sleeping"},
    {eSligMotions::Motion_33_SleepingToStand, "sleeping_to_stand"},
    {eSligMotions::Motion_34_Knockback, "knockback"},
    {eSligMotions::Motion_35_KnockbackToStand, "knockback_to_stand"},
    {eSligMotions::Motion_36_Depossessing, ""},
    {eSligMotions::Motion_37_Possess, "possess"},
    {eSligMotions::Motion_38_OutToFall, "out_to_fall"},
    {eSligMotions::Motion_39_FallingInitiate, "falling_initiate"},
    {eSligMotions::Motion_40_LandingSoft, "landing_soft"},
    {eSligMotions::Motion_41_LandingFatal, "landing_fatal"},
    {eSligMotions::Motion_42_ShootZ, "shoot_z"},
    {eSligMotions::Motion_43_ShootZtoStand, "shoot_z_to_stand"},
    {eSligMotions::Motion_44_Smash, "smash"},
    {eSligMotions::Motion_45_PullLever, "pull_lever"},
    {eSligMotions::Motion_46_LiftGrip, "lift_grip"},
    {eSligMotions::Motion_47_LiftUngrip, "lift_ungrip"},
    {eSligMotions::Motion_48_LiftGripping, "lift_gripping"},
    {eSligMotions::Motion_49_LiftUp, "lift_up"},
    {eSligMotions::Motion_50_LiftDown, "lift_down"},
    {eSligMotions::Motion_51_Beat, "beat"},
})

inline void to_json(nlohmann::json& j, const SligSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"xpos", p.field_4_xpos},
        {"ypos", p.field_8_ypos},
        {"velx", p.field_C_velx},
        {"vely", p.field_10_vely},
        {"path_number", p.field_14_path_number},
        {"lvl_number", p.field_16_lvl_number},
        {"sprite_scale", p.field_18_sprite_scale},
        {"scale", p.field_1C_scale},
        {"r", p.field_1E_r},
        {"g", p.field_20_g},
        {"b", p.field_22_b},
        {"tint_saved", p.mTintSaved},
        {"tint", p.mTint},
        {"listener_last_event_idx", p.mListenerLastEventIdx},
        {"flipx", p.field_24_bFlipX},
        {"field_26_current_motion", p.field_26_current_motion},
        {"current_frame ", p.field_28_current_frame },
        {"frame_change_counter", p.field_2A_frame_change_counter},
        {"render", p.field_2C_bRender},
        {"drawable", p.field_2D_bDrawable},
        {"health", p.field_30_health},
        {"field_34_current_motion", p.field_34_current_motion},
        {"next_motion", p.field_36_next_motion},
        {"last_line_ypos", p.field_38_last_line_ypos},
        {"collision_line_type", p.field_3A_collision_line_type},
        {"active_char", p.field_40_bActiveChar},
        {"brain_sub_state", p.field_42_brain_sub_state},
        {"pitch_min", p.field_44_pitch_min},
        {"field_48_timer", p.field_48_timer},
        {"return_to_previous_motion", p.mReturnToPreviousMotion},
        {"checked_if_off_screen", p.mCheckedIfOffScreen},
        {"input", p.field_50_input},
        {"field_54_timer", p.field_54_timer},
        {"falling_velx_scale_factor", p.mFallingVelxScaleFactor},
        {"tlv_info", p.field_5C_tlvInfo},
        {"res_idx", p.field_60_res_idx},
        {"shot_motion", p.field_62_shot_motion},
        {"zone_rect", p.field_64_zone_rect},
        {"return_level", p.field_72_return_level},
        {"return_path", p.field_74_return_path},
        {"return_camera", p.field_76_return_camera},
        {"death_by_being_shot_timer", p.field_78_death_by_being_shot_timer},
        {"explode_timer", p.field_7C_explode_timer},
        {"brain_state_idx", p.field_80_brain_state_idx},
        {"num_times_to_shoot", p.field_8C_num_times_to_shoot},
        {"force_alive_state", p.field_90_force_alive_state},
        {"spotted_possessed_slig", p.field_92_spotted_possessed_slig},
        {"glukkon_id", p.field_94_glukkon_id},
        {"state_after_speak", p.field_98_state_after_speak},
        {"attention_timeout", p.field_9A_attention_timeout},
        {"next_command_arg1", p.field_9E_next_command_arg1},
        {"cmd_idx", p.field_A0_cmd_idx},
        {"follow_glukkon", p.mFollowGlukkon},
        {"stopped_for_lever_or_lift", p.mStoppedForLeverOrLift},
        {"glukkon_called_alloya", p.mGlukkonCalledAllOYa},
        {"heard_glukkon", p.mHeardGlukkon},
        {"previous_motion_saved", p.mPreviousMotionSaved},
        {"previous_motion", p.mPreviousMotion},
        {"motion_changed", p.mbMotionChanged},
        {"prevent_depossession", p.mPreventDepossession},
        {"next_gamespeak_motion", p.mNextGameSpeakMotion},
        {"input_raw", p.mInput},
        {"points_count", p.mPointsCount},
        {"points_x", p.mPointsX},
        {"points_y", p.mPointsY},
    };
}

inline void from_json(const nlohmann::json& j, SligSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("xpos").get_to(p.field_4_xpos);
    j.at("ypos").get_to(p.field_8_ypos);
    j.at("velx").get_to(p.field_C_velx);
    j.at("vely").get_to(p.field_10_vely);
    j.at("path_number").get_to(p.field_14_path_number);
    j.at("lvl_number").get_to(p.field_16_lvl_number);
    j.at("sprite_scale").get_to(p.field_18_sprite_scale);
    j.at("scale").get_to(p.field_1C_scale);
    j.at("r").get_to(p.field_1E_r);
    j.at("g").get_to(p.field_20_g);
    j.at("b").get_to(p.field_22_b);
    // Not in a save from before they were kept
    if (j.contains("tint_saved"))
    {
        j.at("tint_saved").get_to(p.mTintSaved);
        j.at("tint").get_to(p.mTint);
        j.at("listener_last_event_idx").get_to(p.mListenerLastEventIdx);
    }
    j.at("flipx").get_to(p.field_24_bFlipX);
    j.at("field_26_current_motion").get_to(p.field_26_current_motion);
    j.at("current_frame ").get_to(p.field_28_current_frame);
    j.at("frame_change_counter").get_to(p.field_2A_frame_change_counter);
    j.at("render").get_to(p.field_2C_bRender);
    j.at("drawable").get_to(p.field_2D_bDrawable);
    j.at("health").get_to(p.field_30_health);
    j.at("field_34_current_motion").get_to(p.field_34_current_motion);
    j.at("next_motion").get_to(p.field_36_next_motion);
    j.at("last_line_ypos").get_to(p.field_38_last_line_ypos);
    j.at("collision_line_type").get_to(p.field_3A_collision_line_type);
    j.at("active_char").get_to(p.field_40_bActiveChar);
    j.at("brain_sub_state").get_to(p.field_42_brain_sub_state);
    j.at("pitch_min").get_to(p.field_44_pitch_min);
    j.at("field_48_timer").get_to(p.field_48_timer);
    j.at("return_to_previous_motion").get_to(p.mReturnToPreviousMotion);
    j.at("checked_if_off_screen").get_to(p.mCheckedIfOffScreen);
    j.at("input").get_to(p.field_50_input);
    j.at("field_54_timer").get_to(p.field_54_timer);
    j.at("falling_velx_scale_factor").get_to(p.mFallingVelxScaleFactor);
    j.at("tlv_info").get_to(p.field_5C_tlvInfo);
    j.at("res_idx").get_to(p.field_60_res_idx);
    j.at("shot_motion").get_to(p.field_62_shot_motion);
    j.at("zone_rect").get_to(p.field_64_zone_rect);
    j.at("return_level").get_to(p.field_72_return_level);
    j.at("return_path").get_to(p.field_74_return_path);
    j.at("return_camera").get_to(p.field_76_return_camera);
    j.at("death_by_being_shot_timer").get_to(p.field_78_death_by_being_shot_timer);
    j.at("explode_timer").get_to(p.field_7C_explode_timer);
    j.at("brain_state_idx").get_to(p.field_80_brain_state_idx);
    j.at("num_times_to_shoot").get_to(p.field_8C_num_times_to_shoot);
    j.at("force_alive_state").get_to(p.field_90_force_alive_state);
    j.at("spotted_possessed_slig").get_to(p.field_92_spotted_possessed_slig);
    j.at("glukkon_id").get_to(p.field_94_glukkon_id);
    j.at("state_after_speak").get_to(p.field_98_state_after_speak);
    j.at("attention_timeout").get_to(p.field_9A_attention_timeout);
    j.at("next_command_arg1").get_to(p.field_9E_next_command_arg1);
    j.at("cmd_idx").get_to(p.field_A0_cmd_idx);
    j.at("follow_glukkon").get_to(p.mFollowGlukkon);
    j.at("stopped_for_lever_or_lift").get_to(p.mStoppedForLeverOrLift);
    j.at("glukkon_called_alloya").get_to(p.mGlukkonCalledAllOYa);
    j.at("heard_glukkon").get_to(p.mHeardGlukkon);
    if (j.contains("previous_motion_saved") && j.at("previous_motion_saved").get<bool>())
    {
        p.mPreviousMotionSaved = true;
        j.at("previous_motion").get_to(p.mPreviousMotion);
        j.at("motion_changed").get_to(p.mbMotionChanged);
        j.at("prevent_depossession").get_to(p.mPreventDepossession);
        j.at("next_gamespeak_motion").get_to(p.mNextGameSpeakMotion);
        j.at("input_raw").get_to(p.mInput);
        j.at("points_count").get_to(p.mPointsCount);
        const nlohmann::json& pointsX = j.at("points_x");
        const nlohmann::json& pointsY = j.at("points_y");
        for (s32 i = 0; i < 10 && i < static_cast<s32>(pointsX.size()); i++)
        {
            pointsX.at(i).get_to(p.mPointsX[i]);
            pointsY.at(i).get_to(p.mPointsY[i]);
        }
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(eSlogMotions, {
    {eSlogMotions::m2, "m2"},
    {eSlogMotions::m1, "m1"},
    {eSlogMotions::Motion_0_Idle, "idle"},
    {eSlogMotions::Motion_1_Walk, "walk"},
    {eSlogMotions::Motion_2_Run, "run"},
    {eSlogMotions::Motion_3_TurnAround, "turn_around"},
    {eSlogMotions::Motion_4_Fall, "fall"},
    {eSlogMotions::Motion_5_MoveHeadUpwards, "move_head_upwards"},
    {eSlogMotions::Motion_6_StopRunning, "stop_running"},
    {eSlogMotions::Motion_7_SlideTurn, "slide_turn"},
    {eSlogMotions::Motion_8_StartWalking, "start_walking"},
    {eSlogMotions::Motion_9_EndWalking, "end_walking"},
    {eSlogMotions::Motion_10_Land, "land"},
    {eSlogMotions::Motion_11_Unused, "motion_11_unused"},
    {eSlogMotions::Motion_12_StartFastBarking, "start_fast_barking"},
    {eSlogMotions::Motion_13_EndFastBarking, "end_fast_barking"},
    {eSlogMotions::Motion_14_AngryBark, "angry_bark"},
    {eSlogMotions::Motion_15_Sleeping, "sleeping"},
    {eSlogMotions::Motion_16_MoveHeadDownwards, "move_head_downwards"},
    {eSlogMotions::Motion_17_WakeUp, "wake_up"},
    {eSlogMotions::Motion_18_JumpForwards, "jump_forwards"},
    {eSlogMotions::Motion_19_JumpUpwards, "jump_upwards"},
    {eSlogMotions::Motion_20_Eating, "eating"},
    {eSlogMotions::Motion_21_Dying, "dying"},
    {eSlogMotions::Motion_22_Scratch, "scratch"},
    {eSlogMotions::Motion_23_Growl, "growl"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(eSlogBrains, {
    {eSlogBrains::Brain_0_ListeningToSlig, "listening_to_slig"},
    {eSlogBrains::Brain_1_Idle, "idle"},
    {eSlogBrains::Brain_2_ChasingAbe, "chasing_abe"},
    {eSlogBrains::Brain_3_Death, "death"},
})

inline void to_json(nlohmann::json& j, const SlogSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"base_tlv_id", p.mBaseTlvId},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"velx", p.mVelX},
        {"vely", p.mVelY},
        {"current_path", p.mCurrentPath},
        {"current_level", p.mCurrentLevel},
        {"sprite_scale", p.mSpriteScale},
        {"r", p.mR},
        {"g", p.mG},
        {"b", p.mB},
        {"flipx", p.mFlipX},
        {"current_motion", p.mCurrentMotion},
        {"current_frame", p.mCurrentFrame},
        {"frame_change_counter", p.mFrameChangeCounter},
        {"render", p.mRender},
        {"drawable", p.mDrawable},
        {"health", p.mHealth},
        {"current_motion2", p.mCurrentMotion2},
        {"next_motion", p.mNextMotion},
        {"last_line_ypos", p.mLastLineYPos},
        {"collision_line_type", p.mCollisionLineType},
        {"platform_id", p.mPlatformId},
        {"slog_tlv_id", p.mSlogTlvId},
        {"target_id", p.mTargetId},
        {"brain_state", p.mBrainState},
        {"brain_sub_state", p.mBrainSubState},
        {"multi_use_timer", p.mMultiUseTimer},
        {"falling_velx_scale_factor", p.mFallingVelxScaleFactor},
        {"listen_to_slig_id", p.mListeningToSligId},
        {"has_woofed", p.mHasWoofed},
        {"waiting_counter", p.mWaitingCounter},
        {"response_idx", p.mResponseIdx},
        {"response_part", p.mResponsePart},
        {"anger_level", p.mAngerLevel},
        {"jump_counter", p.mJumpCounter},
        {"scratch_timer", p.mScratchTimer},
        {"growl_timer", p.mGrowlTimer},
        {"bone_id", p.mBoneId},
        {"chase_delay", p.mChaseDelay},
        {"slog_random_idx", p.mSlogRandomIdx},
        {"biting_target", p.mBitingTarget},
        {"eBit3_Asleep", p.eBit3_Asleep},
        {"moved_off_screen", p.mMovedOffScreen},
        {"stop_running", p.mStopRunning},
        {"shot", p.mShot},
        {"hungry", p.mHungry},
        {"commanded_to_attack", p.mCommandedToAttack},
        {"hit_by_ability_ring", p.mHitByAbilityRing},
        {"listen_to_sligs", p.mListenToSligs},
    };
}

inline void from_json(const nlohmann::json& j, SlogSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("base_tlv_id").get_to(p.mBaseTlvId);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("velx").get_to(p.mVelX);
    j.at("vely").get_to(p.mVelY);
    j.at("current_path").get_to(p.mCurrentPath);
    j.at("current_level").get_to(p.mCurrentLevel);
    j.at("sprite_scale").get_to(p.mSpriteScale);
    j.at("r").get_to(p.mR);
    j.at("g").get_to(p.mG);
    j.at("b").get_to(p.mB);
    j.at("flipx").get_to(p.mFlipX);
    j.at("current_motion").get_to(p.mCurrentMotion);
    j.at("current_frame").get_to(p.mCurrentFrame);
    j.at("frame_change_counter").get_to(p.mFrameChangeCounter);
    j.at("render").get_to(p.mRender);
    j.at("drawable").get_to(p.mDrawable);
    j.at("health").get_to(p.mHealth);
    j.at("current_motion2").get_to(p.mCurrentMotion2);
    j.at("next_motion").get_to(p.mNextMotion);
    j.at("last_line_ypos").get_to(p.mLastLineYPos);
    j.at("collision_line_type").get_to(p.mCollisionLineType);
    j.at("platform_id").get_to(p.mPlatformId);
    j.at("slog_tlv_id").get_to(p.mSlogTlvId);
    j.at("target_id").get_to(p.mTargetId);
    j.at("brain_state").get_to(p.mBrainState);
    j.at("brain_sub_state").get_to(p.mBrainSubState);
    j.at("multi_use_timer").get_to(p.mMultiUseTimer);
    j.at("falling_velx_scale_factor").get_to(p.mFallingVelxScaleFactor);
    j.at("listen_to_slig_id").get_to(p.mListeningToSligId);
    j.at("has_woofed").get_to(p.mHasWoofed);
    j.at("waiting_counter").get_to(p.mWaitingCounter);
    j.at("response_idx").get_to(p.mResponseIdx);
    j.at("response_part").get_to(p.mResponsePart);
    j.at("anger_level").get_to(p.mAngerLevel);
    j.at("jump_counter").get_to(p.mJumpCounter);
    j.at("scratch_timer").get_to(p.mScratchTimer);
    j.at("growl_timer").get_to(p.mGrowlTimer);
    j.at("bone_id").get_to(p.mBoneId);
    j.at("chase_delay").get_to(p.mChaseDelay);
    j.at("slog_random_idx").get_to(p.mSlogRandomIdx);
    j.at("biting_target").get_to(p.mBitingTarget);
    j.at("eBit3_Asleep").get_to(p.eBit3_Asleep);
    j.at("moved_off_screen").get_to(p.mMovedOffScreen);
    j.at("stop_running").get_to(p.mStopRunning);
    j.at("shot").get_to(p.mShot);
    j.at("hungry").get_to(p.mHungry);
    j.at("commanded_to_attack").get_to(p.mCommandedToAttack);
    j.at("hit_by_ability_ring").get_to(p.mHitByAbilityRing);
    j.at("listen_to_sligs").get_to(p.mListenToSligs);
}

NLOHMANN_JSON_SERIALIZE_ENUM(SlurgStates, {
    {SlurgStates::eMoving_0, "moving"},
    {SlurgStates::ePausing_1, "pausing"},
    {SlurgStates::eBurst_2, "burst"},
})

inline void to_json(nlohmann::json& j, const SlurgSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"xpos", p.mXPos},
        {"ypos", p.mYPos},
        {"velx", p.mVelX},
        {"slurg_sprite_scale", p.mSlurgSpriteScale},
        {"flipx", p.mFlipX},
        {"anim_current_frame", p.mAnimCurrentFrame},
        {"frame_change_counter", p.mFrameChangeCounter},
        {"drawable", p.mDrawable},
        {"render", p.mRender},
        {"tlv_id", p.mTlvId},
        {"slurg_state", p.mSlurgState},
        {"going_right", p.mGoingRight},
        {"moving", p.mMoving},
        {"moving_timer", p.mMovingTimer},
        {"rng_for_moving_timer", p.mRngForMovingTimer},
    };
}

inline void from_json(const nlohmann::json& j, SlurgSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("xpos").get_to(p.mXPos);
    j.at("ypos").get_to(p.mYPos);
    j.at("velx").get_to(p.mVelX);
    j.at("slurg_sprite_scale").get_to(p.mSlurgSpriteScale);
    j.at("flipx").get_to(p.mFlipX);
    j.at("anim_current_frame").get_to(p.mAnimCurrentFrame);
    j.at("frame_change_counter").get_to(p.mFrameChangeCounter);
    j.at("drawable").get_to(p.mDrawable);
    j.at("render").get_to(p.mRender);
    j.at("tlv_id").get_to(p.mTlvId);
    j.at("slurg_state").get_to(p.mSlurgState);
    j.at("going_right").get_to(p.mGoingRight);
    j.at("moving").get_to(p.mMoving);
    if (j.contains("moving_timer"))
    {
        j.at("moving_timer").get_to(p.mMovingTimer);
        j.at("rng_for_moving_timer").get_to(p.mRngForMovingTimer);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(TimerTrigger::TimerTriggerStates, {
    {TimerTrigger::TimerTriggerStates::eWaitForEnabled_0, "wait_for_enabled"},
    {TimerTrigger::TimerTriggerStates::eWaitForFirstTrigger_1, "wait_for_first_trigger"},
    {TimerTrigger::TimerTriggerStates::eCheckForStartAgain_2, "check_for_start_again"},
    {TimerTrigger::TimerTriggerStates::eWaitForSecondTrigger_3, "wait_for_second_trigger"},
})

inline void to_json(nlohmann::json& j, const TimerTriggerSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"tlv_id", p.mTlvId},
        {"activation_delay_timer", p.mActivationDelayTimer},
        {"state", p.mState},
        {"starting_switch_state", p.mStartingSwitchState},
    };
}

inline void from_json(const nlohmann::json& j, TimerTriggerSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("tlv_id").get_to(p.mTlvId);
    j.at("activation_delay_timer").get_to(p.mActivationDelayTimer);
    j.at("state").get_to(p.mState);
    j.at("starting_switch_state").get_to(p.mStartingSwitchState);
}

NLOHMANN_JSON_SERIALIZE_ENUM(TrapDoorState, {
    {TrapDoorState::eClosed, "closed"},
    {TrapDoorState::eOpening, "opening"},
    {TrapDoorState::eOpen, "open"},
    {TrapDoorState::eClosing, "closing"},
})

inline void to_json(nlohmann::json& j, const TrapDoorSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"state", p.mState},
        {"open_time", p.mOpenTime},
        {"tlv_id", p.mTlvId},
        {"platform_base_saved", p.mPlatformBaseSaved},
        {"platform_base_count", p.mPlatformBaseCount},
        {"platform_base_x_offset", p.mPlatformBaseXOffset},
        {"platform_base_width_offset", p.mPlatformBaseWidthOffset},
        {"platform_base_y_offset", p.mPlatformBaseYOffset},
        {"platform_base_height_offset", p.mPlatformBaseHeightOffset},
    };
}

inline void from_json(const nlohmann::json& j, TrapDoorSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("state").get_to(p.mState);
    j.at("open_time").get_to(p.mOpenTime);
    j.at("tlv_id").get_to(p.mTlvId);
    if (j.contains("platform_base_saved") && j.at("platform_base_saved").get<bool>())
    {
        p.mPlatformBaseSaved = true;
        j.at("platform_base_count").get_to(p.mPlatformBaseCount);
        j.at("platform_base_x_offset").get_to(p.mPlatformBaseXOffset);
        j.at("platform_base_width_offset").get_to(p.mPlatformBaseWidthOffset);
        j.at("platform_base_y_offset").get_to(p.mPlatformBaseYOffset);
        j.at("platform_base_height_offset").get_to(p.mPlatformBaseHeightOffset);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(UXBState, {
    {UXBState::eDelay, "delay"},
    {UXBState::eActive, "active"},
    {UXBState::eExploding, "exploding"},
    {UXBState::eDeactivated, "deactivated"},
})

inline void to_json(nlohmann::json& j, const UXBSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"tlv_info", p.mTlvInfo},
        {"next_state_timer", p.mNextStateTimer},
        {"current_state", p.mCurrentState},
        {"starting_state", p.mStartingState},
        {"pattern_index", p.mPatternIndex},
        {"red_blink_count", p.mRedBlinkCount},
        {"is_red", p.mIsRed},
        {"anim", p.mAnim},
        {"flash_anim", p.mFlashAnim},
        {"flash_anim_id", p.mFlashAnimId},
        {"flash_pal_green", p.mFlashPalGreen},
    };
}

inline void from_json(const nlohmann::json& j, UXBSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("tlv_info").get_to(p.mTlvInfo);
    j.at("next_state_timer").get_to(p.mNextStateTimer);
    j.at("current_state").get_to(p.mCurrentState);
    j.at("starting_state").get_to(p.mStartingState);
    j.at("pattern_index").get_to(p.mPatternIndex);
    j.at("red_blink_count").get_to(p.mRedBlinkCount);
    j.at("is_red").get_to(p.mIsRed);
    if (j.contains("anim"))
    {
        j.at("anim").get_to(p.mAnim);
    }
    if (j.contains("flash_anim"))
    {
        j.at("flash_anim").get_to(p.mFlashAnim);
        j.at("flash_anim_id").get_to(p.mFlashAnimId);
        j.at("flash_pal_green").get_to(p.mFlashPalGreen);
    }
}

NLOHMANN_JSON_SERIALIZE_ENUM(WheelStates, {
    {WheelStates::eIdle_0, "idle"},
    {WheelStates::eTurning_1, "turning"},
})

inline void to_json(nlohmann::json& j, const WorkWheelSaveState& p)
{
    j = nlohmann::json{
        {"type", p.mType},
        {"tlv_id", p.mTlvId},
        {"turning_time", p.mTurningTime},
        {"state", p.mState},
    };
}

inline void from_json(const nlohmann::json& j, WorkWheelSaveState& p)
{
    j.at("type").get_to(p.mType);
    j.at("tlv_id").get_to(p.mTlvId);
    j.at("turning_time").get_to(p.mTurningTime);
    j.at("state").get_to(p.mState);
}

inline void to_json(nlohmann::json& j, const Quicksave_WorldInfo& p)
{
    j = nlohmann::json{
        {"gn_frame", p.mGnFrame},
        {"level", p.mLevel},
        {"path", p.mPath},
        {"cam", p.mCam},
        {"save_file_id", p.mSaveFileId},
        {"controlled_x", p.mControlledCharX},
        {"controlled_y", p.mControlledCharY},
        {"controlled_scale", p.mControlledCharAtFullScale},
        {"saved_muds", p.mRescuedMudokons},
        {"killed_muds", p.mKilledMudokons},
        {"muds_in_area", p.field_16_muds_in_area},
        {"last_saved_killed_muds_per_path", p.field_17_last_saved_killed_muds_per_path},
        {"saved_killed_muds_per_zulag", p.field_18_saved_killed_muds_per_zulag}, // array
        {"current_zulag_number", p.field_2C_current_zulag_number},
        {"total_meter_bars", p.mTotalMeterBars},
        {"draw_meter_count_down", p.field_30_bDrawMeterCountDown},
        {"visited_bonewerkz", p.mVisitedBonewerkz},
        {"visited_barracks", p.mVisitedBarracks},
        {"visited_feeco_ender", p.mVisitedFeecoEnder},
        {"gas_timer", p.mGasTimer},
        {"random_seed", p.mRandomSeed},
        {"leaf_rand_idx", p.mLeafRandIdx},
        {"guid_counter", p.mGuidCounter},
        {"abe_invincible", p.mAbeInvincible},
    };
}

inline void from_json(const nlohmann::json& j, Quicksave_WorldInfo& p)
{
    j.at("gn_frame").get_to(p.mGnFrame);
    j.at("level").get_to(p.mLevel);
    j.at("path").get_to(p.mPath);
    j.at("cam").get_to(p.mCam);
    j.at("save_file_id").get_to(p.mSaveFileId);
    j.at("controlled_x").get_to(p.mControlledCharX);
    j.at("controlled_y").get_to(p.mControlledCharY);
    j.at("controlled_scale").get_to(p.mControlledCharAtFullScale);
    j.at("saved_muds").get_to(p.mRescuedMudokons);
    j.at("killed_muds").get_to(p.mKilledMudokons);
    j.at("muds_in_area").get_to(p.field_16_muds_in_area);
    j.at("last_saved_killed_muds_per_path").get_to(p.field_17_last_saved_killed_muds_per_path);
    j.at("saved_killed_muds_per_zulag").get_to(p.field_18_saved_killed_muds_per_zulag); // array
    j.at("current_zulag_number").get_to(p.field_2C_current_zulag_number);
    j.at("total_meter_bars").get_to(p.mTotalMeterBars);
    j.at("draw_meter_count_down").get_to(p.field_30_bDrawMeterCountDown);
    j.at("visited_bonewerkz").get_to(p.mVisitedBonewerkz);
    j.at("visited_barracks").get_to(p.mVisitedBarracks);
    j.at("visited_feeco_ender").get_to(p.mVisitedFeecoEnder);
    j.at("gas_timer").get_to(p.mGasTimer);
    // Saves from before the seed was kept don't have it
    p.mRandomSeed = j.value("random_seed", static_cast<u8>(0));
    p.mLeafRandIdx = j.value("leaf_rand_idx", static_cast<u8>(8));
    p.mGuidCounter = j.value("guid_counter", static_cast<u32>(0));
    j.at("abe_invincible").get_to(p.mAbeInvincible);
}

inline void to_json(nlohmann::json& j, const SwitchStates& p)
{
    j = nlohmann::json{
        {"data", p.mData},
    };
}

inline void from_json(const nlohmann::json& j, SwitchStates& p)
{
    j.at("data").get_to(p.mData);
}

// TODO: put forward declare in the correct location
void ConvertObjectsStatesToJson(nlohmann::json& j, const SerializedObjectData& pData);

// TODO: Not AE specific move out of here
static inline nlohmann::json WriteObjectStateJson(const SerializedObjectData& object_states)
{
    nlohmann::json j;
    ConvertObjectsStatesToJson(j, object_states);
    return j;
}

// The Quicksave (top level) to_json/from_json are declared here and defined in AESaveSerialization.cpp.
#include "AEQuicksaveJson.hpp"

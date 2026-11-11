#pragma once

#include "FixedPoint.hpp"
#include "SaveStateBase.hpp"
#include "GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"

// Note, names taken from Abe's Exoddus PC Demo
enum class Event : s16
{
    kEventNoise,
    kEventSpeaking,
    kEventShooting,
    kEventHeroDying,
    kEventAbeDead, // only at the point when the abe birds appear
    kScrabOrParamiteDied,
    kEventDeathReset,
    kEventDeathResetEnd,
    kEventResetting,
    kEventAbeOhm,
    kEventSuspiciousNoise,
    kEventLoudNoise,
    kEventMudokonDied,
    kEventMudokonLaugh,
    kEventMudokonAbuse,
    kEventMudokonComfort,
    kEventGlukkonUnknown,
    kEventAlarm,
    kEventPortalOpen,
    kEventOtherPortalOpen,
    kEventScreenShake,
    kEventMudokonDead,
    kEventMax, // Must be last, do not use
};

class BaseGameObject;

// The events broadcast that the next frame looks at (they are looked at a frame after they are made) and which of
// the two buffers that is: part of the world between two frames. The objects are known by their ids.
struct EventsSaveState final : public SaveStateBase
{
    EventsSaveState()
        : SaveStateBase(ReliveTypes::eEventsState, sizeof(*this))
    { }
    s16 mEventsToUse;
    Guid mEventObjectIds[2][static_cast<u32>(Event::kEventMax)];
};

EventsSaveState EventsGetSaveState();
// Once the objects the events name are made
void EventsRestoreFromSaveState(const EventsSaveState& state);

void EventBroadcast(Event event, BaseGameObject* pObject);
BaseGameObject* EventGet(Event eventType);
void EventsReset();
void EventsResetActive();

enum class EventScale
{
    Full,
    Half,
    Both
};

inline constexpr EventScale AsEventScale(Scale scale)
{
    if (scale == Scale::Fg)
    {
        return EventScale::Full;
    }
    return EventScale::Half;
}

BaseAnimatedWithPhysicsGameObject* IsEventInRange(Event eventType, FP xpos, FP ypos, EventScale scale);

// TODO: Get AO to stop using the FP scale and merge both copies
BaseAnimatedWithPhysicsGameObject* IsEventInRange(Event eventType, FP xpos, FP ypos, FP scale);

void EventCancelForObj(BaseGameObject* pObj);

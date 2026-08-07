// SentinelAI/Patrol: the flare_reactor experiment's AI state + steering (docs/rfc/0001-flare-
// reactor-pipeline-experiment.md, Phase 6) -- first real use of the engine-ai-behavior skill in
// this project. UpdateSentinel is the actual "brain" (FSM + Seek steering, skill §§1/3);
// ApplyBeaconPerception is the perception rule (skill §4 -- reaction gated by kHearingRadius, not
// unconditional "always know where the beacon is"). Both are free functions operating on an
// entity's own components, not methods on some view or controller object -- see ai_view.h's own
// header comment for why (GameCode4's AITeapotView is a near-empty stub; the real brain lives
// per-actor in Lua state machines, not fused into the C++ view -- this project has no scripting
// layer, so the brain lives here instead, still decoupled from the view either way).
//
// Header-only (like beacon_pulse_process.h) and deliberately raylib-link-free -- Vector3Lerp-style
// raymath.h functions are RMAPI/header-only, so this stays test-build-safe
// (tests/sentinel_ai_test.cpp exercises it directly, no window/GL/audio context needed).
#ifndef FLARE_REACTOR_SENTINEL_AI_H
#define FLARE_REACTOR_SENTINEL_AI_H

#include <cstddef>
#include <vector>

#include <entt/entt.hpp>
#include <raylib.h>
#include <raymath.h>

#include "app/scene/transform.h"

enum class SentinelState { Patrol, Investigate };

struct SentinelAI {
    SentinelState state = SentinelState::Patrol;
    Vector3 velocity{0.0f, 0.0f, 0.0f};
    Vector3 investigateTarget{0.0f, 0.0f, 0.0f};
};

struct Patrol {
    std::vector<Vector3> waypoints;
    std::size_t current = 0;
};

namespace flare_reactor_sentinel {
    inline constexpr float kMaxSpeed = 3.0f;
    inline constexpr float kMaxAccel = 6.0f;
    inline constexpr float kArriveRadius = 0.5f;

    // Seek toward target with Arrive-style slowdown near it -- engine-ai-behavior skill §3's own
    // sketch, copied as-is rather than factored into a shared app/ header: this is the first real
    // AI in the project, one consumer, nothing to share yet (same "second real consumer" discipline
    // this project applies elsewhere).
    inline Vector3 Seek(Vector3 from, Vector3 to, float maxSpeed, float slowRadius = 0.0f) {
        Vector3 toTarget = Vector3Subtract(to, from);
        float dist = Vector3Length(toTarget);
        if (dist < 0.0001f) return Vector3Zero();
        float speed = maxSpeed;
        if (slowRadius > 0.0f && dist < slowRadius) speed = maxSpeed * (dist / slowRadius);
        return Vector3Scale(Vector3Normalize(toTarget), speed);
    }
}

// The FSM + steering tick for one sentinel entity, once per frame -- called from AIView::VOnUpdate
// (ai_view.h), the one entity that view possesses. Patrol: Seeks the current waypoint, advances to
// the next one on arrival. Investigate: Seeks investigateTarget and stays there once arrived -- no
// third "return to patrol" state yet (RFC-0001's own note on this).
inline void UpdateSentinel(entt::registry &registry, entt::entity entity, float dt) {
    using namespace flare_reactor_sentinel;

    SentinelAI *ai = registry.try_get<SentinelAI>(entity);
    LocalTransform *transform = registry.try_get<LocalTransform>(entity);
    if (ai == nullptr || transform == nullptr) return;

    Vector3 target;
    if (ai->state == SentinelState::Patrol) {
        Patrol *patrol = registry.try_get<Patrol>(entity);
        if (patrol == nullptr || patrol->waypoints.empty()) return;   // nothing to patrol toward
        target = patrol->waypoints[patrol->current];
    } else {
        target = ai->investigateTarget;
    }

    Vector3 desired = Seek(transform->position, target, kMaxSpeed, kArriveRadius);
    Vector3 accel = Vector3Subtract(desired, ai->velocity);
    if (Vector3Length(accel) > kMaxAccel) accel = Vector3Scale(Vector3Normalize(accel), kMaxAccel);
    ai->velocity = Vector3Add(ai->velocity, Vector3Scale(accel, dt));
    transform->position = Vector3Add(transform->position, Vector3Scale(ai->velocity, dt));

    if (Vector3Distance(transform->position, target) < kArriveRadius && ai->state == SentinelState::Patrol) {
        Patrol *patrol = registry.try_get<Patrol>(entity);
        if (patrol != nullptr && !patrol->waypoints.empty()) {
            patrol->current = (patrol->current + 1) % patrol->waypoints.size();
        }
    }
}

// Perception (skill §4): reacts to a beacon trigger broadcast to every sentinel, but the reaction
// is gated by hearingRadius on the subscriber side, not by the emitter -- "hearing an alarm" (no
// line-of-sight needed, unlike "seeing the player", which would need a real raycast). Still
// sensory omniscience in the sense that the exact position arrives with no occlusion check --
// acceptable for an alarm, per the skill's own note on that distinction.
inline void ApplyBeaconPerception(entt::registry &registry, Vector3 beaconPosition, float hearingRadius) {
    auto view = registry.view<SentinelAI, LocalTransform>();
    for (auto entity : view) {
        auto &ai = view.get<SentinelAI>(entity);
        auto &transform = view.get<LocalTransform>(entity);
        if (Vector3Distance(transform.position, beaconPosition) > hearingRadius) continue;

        ai.state = SentinelState::Investigate;
        ai.investigateTarget = beaconPosition;
    }
}

#endif // FLARE_REACTOR_SENTINEL_AI_H

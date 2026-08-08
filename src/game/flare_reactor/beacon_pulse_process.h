// BeaconPulseProcess: the flare_reactor experiment's first Process (docs/rfc/0001-flare-reactor-
// pipeline-experiment.md, Phase 4) -- and this whole project's first concrete Process subclass
// (engine-architecture skill §2 named a future CameraShakeProcess as the canonical example; this
// is that "first real one" instead). Animates how a reactor's activation *looks* (scale/rotation/
// color) over kDurationSeconds -- FlareReactorGameLogic::OnActivateBeacon already decided *if* the
// activation is valid and flipped Reactor::active true; this Process only animates the appearance,
// the same Logic/View split the rest of the project applies, just inside a single Process instead
// of between BaseGameLogic/IGameView (docs/rfc/0001 §5). Ends by flipping Reactor::active back to
// false itself, right before Succeed() -- no separate per-frame decrement system needed for that
// (see reactor.h's own header comment).
//
// Header-only (like transform.h/hierarchy.h/renderable.h) and deliberately avoids raylib's own
// ColorLerp (RLAPI in raylib.h, a real linked symbol, unlike raymath.h's RMAPI/header-only
// functions) so this stays link-safe for the test build -- same reasoning
// app/input/input_bindings.h's IsDown/IsPressed split already established. EaseOutQuad isn't in
// vendored raylib either (only in its examples/ tree, not on this project's include path) --
// written inline below instead of vendoring reasings.h for one line of math.
#ifndef FLARE_REACTOR_BEACON_PULSE_PROCESS_H
#define FLARE_REACTOR_BEACON_PULSE_PROCESS_H

#include <algorithm>

#include <entt/entt.hpp>
#include <raylib.h>
#include <raymath.h>

#include "app/process/process.h"
#include "app/scene/renderable.h"
#include "app/scene/transform.h"
#include "reactor.h"

namespace flare_reactor_beacon_pulse {
    inline constexpr float kDurationSeconds = 2.0f;
    inline constexpr float kSpinRadiansPerSecond = 3.0f;
    inline constexpr Vector3 kBaseScale{1.0f, 1.0f, 1.0f};
    inline constexpr Vector3 kPulsedScale{1.35f, 1.35f, 1.35f};

    inline float EaseOutQuad(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }

    // Colors interpolate as a raw per-channel byte lerp -- see the file header comment for why
    // this doesn't call raylib's own ColorLerp.
    inline Color LerpColor(Color a, Color b, float t) {
        return Color{
            static_cast<unsigned char>(a.r + (b.r - a.r) * t),
            static_cast<unsigned char>(a.g + (b.g - a.g) * t),
            static_cast<unsigned char>(a.b + (b.b - a.b) * t),
            static_cast<unsigned char>(a.a + (b.a - a.a) * t),
        };
    }
}

class BeaconPulseProcess : public Process {
public:
    // Snapshots reactor's current Renderable::color (if any) as the lerp's starting point, instead
    // of assuming GRAY -- stays correct if a future reactor variant has a different base color.
    BeaconPulseProcess(entt::registry &registry, entt::entity reactor)
        : registry_(registry), reactor_(reactor) {
        if (auto *renderable = registry_.try_get<Renderable>(reactor_)) baseColor_ = renderable->color;
    }

    void Update(float dt) override {
        using namespace flare_reactor_beacon_pulse;

        elapsed_ += dt;
        float t = std::min(elapsed_ / kDurationSeconds, 1.0f);
        float eased = EaseOutQuad(t);

        if (auto *local = registry_.try_get<LocalTransform>(reactor_)) {
            local->scale = Vector3Lerp(kBaseScale, kPulsedScale, eased);
            local->rotation =
                QuaternionFromAxisAngle(Vector3{0.0f, 1.0f, 0.0f}, elapsed_ * kSpinRadiansPerSecond);
        }

        if (auto *renderable = registry_.try_get<Renderable>(reactor_)) {
            renderable->color = LerpColor(baseColor_, RED, eased);
        }

        if (t >= 1.0f) {
            if (auto *reactor = registry_.try_get<Reactor>(reactor_)) reactor->active = false;
            Succeed();
        }
    }

private:
    entt::registry &registry_;
    entt::entity reactor_;
    Color baseColor_ = GRAY;
    float elapsed_ = 0.0f;
};

#endif // FLARE_REACTOR_BEACON_PULSE_PROCESS_H

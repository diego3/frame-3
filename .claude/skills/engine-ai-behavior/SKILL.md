---
name: engine-ai-behavior
description: Design guidance for enemy/actor AI and behavior in frame-3 — finite state machines, decision/utility scoring, steering behaviors, perception, and pathfinding — based on "Game Coding Complete, 4th Edition" (McShaffry & Graham) Ch. 11-13, adapted to raylib's Vector3/raymath.h and C++. Use this skill whenever an enemy or NPC needs more than one behavior, needs to move naturally (approach, flee, avoid) instead of teleporting/snapping toward a target, needs to choose between competing actions, or needs to react only within some sensing range instead of always knowing where the player is. This is forward-looking design guidance, not a description of existing code — frame-3 has no entities, no gameplay, and no AI of any kind yet.
---

# Engine AI & Behavior (Game Coding Complete Ch. 11-13, adapted to raylib/C++)

frame-3 has no enemies, no NPCs, not even a settled entity model (Actor vs. ECS is still an open
question — see `engine-architecture`'s Open Questions) at the time this skill was written. Nothing
here describes existing code. It exists so that when the first enemy/NPC actually gets built, it
reaches for the right Game Coding Complete Ch. 11-13 pattern instead of a pile of booleans in an
`Update()` — and so the patterns are grounded in raylib's actual 3D math API from day one instead
of translated awkwardly later.

Unlike the book (which assumes a full 3D engine with navmeshes and a general-purpose scene graph)
and unlike a hypothetical from-scratch vector library, **raylib already ships the 3D math this
needs**: `Vector3` plus `raymath.h`'s `Vector3Add`/`Vector3Subtract`/`Vector3Scale`/
`Vector3Normalize`/`Vector3Length`/`Vector3Lerp`, and `Quaternion` for rotation. Use those — don't
write a second vector math layer.

## When to Use This Skill

- An enemy/NPC needs more than a single hardcoded behavior
- An enemy should move naturally toward/away from something (the player, a point, another actor)
  instead of snapping directly onto a target position or velocity
- An enemy must choose among more than one candidate action based on the situation
- An enemy should only react once it's actually within some sensing range — not track the player
  from anywhere in the level regardless of distance or obstruction
- An enemy needs to navigate around level geometry instead of moving in a straight line

## Core Concepts

### 1. Finite state machines — explicit once there are 3+ states

A single enemy behavior (idle, only) needs no framework at all — a plain `Update()` is fine. Once
a second and third state exist (idle → chase → attack), use an explicit state enum and a
transition table rather than a growing pile of `bool isChasing`/`bool isAttacking` flags that can
end up contradicting each other:

```cpp
// Sketch.
enum class EnemyState { Idle, Chase, Attack };

EnemyState UpdateIdle(Enemy& e, float dt);
EnemyState UpdateChase(Enemy& e, float dt);
EnemyState UpdateAttack(Enemy& e, float dt);

void Enemy::Update(float dt) {
    switch (state) {
        case EnemyState::Idle:   state = UpdateIdle(*this, dt);   break;
        case EnemyState::Chase:  state = UpdateChase(*this, dt);  break;
        case EnemyState::Attack: state = UpdateAttack(*this, dt); break;
    }
}
```

### 2. Utility/decision scoring — for choosing between actions, not just transitioning

When an actor has several candidate actions available at once (attack vs. retreat vs.
reposition), Ch. 12's Utility Theory means scoring each candidate and picking the best, instead of
a nested `if`/`else` chain that gets unreadable as more actions are added. Keep tunable weights as
named constants (or later, data-driven from a level/entity definition file) rather than buried
magic numbers, so behavior can be retuned without recompiling logic:

```cpp
// Sketch.
float ScoreAttack(float distanceToPlayer, float health);
float ScoreRetreat(float distanceToPlayer, float health);

Action ChooseAction(const Enemy& e) {
    float attackScore = ScoreAttack(e.distanceToPlayer, e.health);
    float retreatScore = ScoreRetreat(e.distanceToPlayer, e.health);
    return attackScore >= retreatScore ? Action::Attack : Action::Retreat;
}
```

Don't build this for an enemy with only one real action — a single-behavior enemy just needs §1.

### 3. Steering behaviors — natural movement, raylib `Vector3`-native

Craig Reynolds' Seek/Flee/Arrive/Pursue/Evade are all the same shape: compute a desired velocity,
steer the current velocity toward it under an acceleration limit. This is pure `Vector3` math,
directly on top of `raymath.h`:

```cpp
// Sketch: Seek toward target, with Arrive-style slowdown near it.
Vector3 Seek(Vector3 from, Vector3 to, float maxSpeed, float slowRadius = 0.0f) {
    Vector3 toTarget = Vector3Subtract(to, from);
    float dist = Vector3Length(toTarget);
    if (dist < 0.0001f) return Vector3Zero();
    float speed = maxSpeed;
    if (slowRadius > 0.0f && dist < slowRadius) speed = maxSpeed * (dist / slowRadius);
    return Vector3Scale(Vector3Normalize(toTarget), speed);
}

// In Update(): steer current velocity toward the desired one, clamped by max acceleration —
// snapping straight to the desired vector looks robotic, which is the whole point of steering.
Vector3 desired = Seek(enemy.position, player.position, kMaxSpeed, kArriveRadius);
Vector3 accel = Vector3Subtract(desired, enemy.velocity);
if (Vector3Length(accel) > kMaxAccel) accel = Vector3Scale(Vector3Normalize(accel), kMaxAccel);
enemy.velocity = Vector3Add(enemy.velocity, Vector3Scale(accel, dt));
enemy.position = Vector3Add(enemy.position, Vector3Scale(enemy.velocity, dt));
```

This is usually the better first move over an FSM or pathfinding when the actual complaint is
"enemy movement feels flat" — it's cheaper than either and often what's really needed.

### 4. Perception & sensory gating — don't let AI cheat

Don't let an enemy react to the player's exact position unconditionally, regardless of distance —
that's the "sensory omnipotence" anti-pattern the book explicitly warns against (Ch. 11). Gate
reactions behind a sensing range check at minimum:

```cpp
// Sketch.
bool CanSense(const Enemy& e, Vector3 playerPos, float sightRadius) {
    return Vector3Distance(e.position, playerPos) <= sightRadius;
}
```

For true line-of-sight (not just distance) once level geometry exists, raylib's own
`GetRayCollisionMesh`/`GetRayCollisionBox` give a real raycast against the world — no separate
collision system needed for this. Skip line-of-sight entirely for enemies where "always aware" is
the intended design (a turret, an alarm-triggered spawn); it's a per-enemy design choice, not a
rule to apply everywhere.

### 5. Pathfinding — coarse grid or navmesh, not built speculatively

Don't build this before there's level geometry to navigate around — straight-line movement (§3)
covers open areas fine. When it's actually needed: A* over a coarse grid derived from level
geometry (not a fine per-vertex mesh) is enough at a small-demo scale; a full navmesh is Ch. 13's
territory and is a substantially bigger undertaking than the grid — don't reach for it until a
grid genuinely proves insufficient.

## Deliberately Out of Scope (for now)

Matching the same discipline the 2D `frame` project's AI skill settled on, for the same
reason — a small-scale learning project doesn't need infrastructure sized for a much bigger game:

- **Hierarchical state machines / GOAP / composite goal trees** — earn their cost once behaviors
  have real sub-structure or need to plan novel action sequences. A flat FSM (§1) or utility
  scoring (§2) covers a first enemy.
- **Full navmesh / influence maps / territory analysis** — built for open, contestable terrain at
  a scale this project doesn't have yet.
- **Fuzzy logic** — smooths hard thresholds into degrees; genuine polish, not a capability gap.

## Related Skills

- `engine-architecture` — event bus, process manager, prototype spawning, resource cache. AI
  code's timers/spawning/communication should go through those, not reinvent them locally.

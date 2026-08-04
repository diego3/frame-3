---
name: engine-architecture
description: Design guidance for frame-3's core systems (event manager, process manager, ECS entity/prototype spawning via EnTT, resource cache) based on "Game Coding Complete, 4th Edition" (McShaffry & Graham) Ch. 4, 6-8, modernized for raylib + C++ + 3D instead of the book's 2004-era Win32/DirectX target. Use this skill whenever adding cross-system communication, timed/multi-frame behavior (cooldowns, animations, camera effects), spawning game entities, or loading/caching 3D models, textures, or shaders. Also use it when the user asks "how should this be structured", mentions object pooling, ECS, EnTT, event manager, event bus, process manager, resource cache, or references Game Coding Complete directly. This is forward-looking design guidance, not a description of existing code — frame-3 has an `Engine` class (Ch. 5, `src/app/engine.h`/`.cpp`) owning window/audio lifecycle, loop driving, the `entt::registry`, an `EventManager` (`src/app/event_manager.h`), a `ProcessManager` (`src/app/process_manager.h`/`.cpp`, ticked once per frame from `Engine::Run`), and a `ResourceCache<T>` (`src/app/resource_cache.h`, per ADR-0004, backing `Engine::Fonts()`/`Sounds()`); no real components exist yet, and nothing subscribes to the event manager or attaches a process yet either.
---

# Engine Architecture (Game Coding Complete Ch. 4, 6-8 — modernized)

frame-3 is still close to the stock
[raylib-game-template](https://github.com/raysan5/raylib-game-template), reorganized into
`src/app/`, `src/game/`, `src/platform/`, with a simple logo/title/gameplay/ending/options screen
state machine (`src/game/screens.h`, still plain C) driven by `src/app/raylib_game.cpp`. The
`src/app/` layer: **`Engine`** (`src/app/engine.h`/`.cpp`, Ch. 5 Application layer) owns
window/audio lifecycle, drives the main loop via a function-pointer callback, owns an
`entt::registry`, and now also owns and drives §§1-2's systems (`EventManager`, `ProcessManager`)
plus §4's `ResourceCache<T>` — see §3 for what it does (and deliberately doesn't) touch re: ECS.
EnTT itself is wired into the raw Makefile (`CXX`/`CXXFLAGS`/`ENTT_PATH`, no CMake in this repo).
No real components exist yet (§3). This skill exists so that when each of these systems gets
built, it follows a considered design instead of whatever's fastest to type at the time — and so
the *book's* patterns get adapted deliberately, not copied verbatim from a codebase that assumed
Windows, DirectX, and C++03.

**Update this skill with real file references once each system actually lands.** Until then, treat
every code sketch below as a starting shape to adjust, not a spec to match exactly.

## When to Use This Skill

- Adding any kind of cross-system "something happened, who needs to know" communication
- Adding a cooldown, timer, or multi-frame effect (camera shake, a weapon's fire rate, an
  animation sequence) that isn't naturally owned by a single object's own state
- Spawning game entities (enemies, projectiles, pickups) — deciding whether/how to avoid
  hand-building each one from scratch at every spawn site
- Loading a model, texture, or shader — deciding whether/how to avoid loading the same asset
  twice
- Reviewing a design that reaches for one of Game Coding Complete's Ch. 4/6-8 patterns, to check
  it's using the modernized shape below and not the book's literal 2004 C++/Win32 code

## Core Concepts

### 1. Event manager (Ch. 4) — typed, not GUID+enum

The book calls this the Event Manager, and multicasts by a 32-bit GUID and a monolithic event-type
enum, dispatched through hand-rolled delegate objects — idiomatic C++ for a pre-`std::function`,
pre-templates-were-trusted-for-this era. **Don't port that literally.** A modern equivalent, and —
not coincidentally — the same shape already proven out in this project's sibling 2D engine
(`frame`'s Go `event.Subscribe[T]`/`Emit`): an `EventManager` keyed by `std::type_index`, with
`std::function<void(const T&)>` handlers, keeping the book's name rather than relabeling it "event
bus".

```cpp
// Sketch — matches src/app/event_manager.h; adjust here if that file's shape changes.
class EventManager {
public:
    template <typename T>
    void Subscribe(std::function<void(const T&)> handler) {
        handlers_[std::type_index(typeid(T))].push_back(
            [handler](const void* ev) { handler(*static_cast<const T*>(ev)); });
    }

    template <typename T>
    void Emit(const T& event) {
        auto it = handlers_.find(std::type_index(typeid(T)));
        if (it == handlers_.end()) return;
        for (auto& h : it->second) h(&event);
    }

private:
    std::unordered_map<std::type_index, std::vector<std::function<void(const void*)>>> handlers_;
};
```

No GUIDs, no enum to keep in sync across translation units, and adding a new event type is just
defining a new struct — the same win the book's Ch. 4 was actually chasing (avoid the
"terrible monolithic enumeration" compile-time trap), achieved with templates instead of an
integer registry.

**Current state**: `EventManager` (`src/app/event_manager.h`, header-only) exists and
`Engine::Events()` (`src/app/engine.h`) owns the one instance — but nothing subscribes or emits
yet. No event struct types exist yet either; define one per event kind as gameplay code needs to
announce something (e.g. a future `EvtData_EnemyDied`), don't pre-build a taxonomy of events
speculatively.

### 2. Process manager (Ch. 4) — cooperative multitasking for timed behavior

Genuinely not Windows/DirectX-specific in the book — this ports close to as-is. A `Process` runs
across multiple frames and reports its own completion; a `ProcessManager` owns a list and calls
`Update(dt)` on each once per frame, removing finished ones.

```cpp
// Sketch — matches src/app/process.h + src/app/process_manager.h/.cpp; adjust here if those
// files' shape changes.
class Process {
public:
    virtual ~Process() = default;
    virtual void Update(float dt) = 0;
    bool IsDead() const { return state_ != State::Running; }
    void Succeed() { state_ = State::Succeeded; }
    void Fail() { state_ = State::Failed; }
private:
    enum class State { Running, Succeeded, Failed } state_ = State::Running;
};

class ProcessManager {
public:
    void Attach(std::unique_ptr<Process> p) { processes_.push_back(std::move(p)); }
    void Update(float dt) {
        for (auto& p : processes_) p->Update(dt);
        std::erase_if(processes_, [](auto& p) { return p->IsDead(); });
    }
private:
    std::vector<std::unique_ptr<Process>> processes_;
};
```

Use this for behavior that isn't naturally one object's own state — camera shake is the canonical
example (there's no single "camera actor" to hang a cooldown field off of). Don't reach for this
for a single entity's own cooldown; that's just a member variable on the entity.

**Current state**: `Process`/`ProcessManager` exist and `Engine::Processes()` (`src/app/engine.h`)
owns the one `ProcessManager` instance, ticked once per frame from `Engine::Run` (both the desktop
`while` loop and the `PLATFORM_WEB` `emscripten_set_main_loop` path go through the same
`TickAndUpdateDraw` trampoline in `engine.cpp`, so process ticking isn't duplicated per-platform)
— but nothing attaches a `Process` yet. No concrete `Process` subclass exists yet either; write one
per timed behavior as it's needed (e.g. a future `CameraShakeProcess`), don't pre-build a library
of process types speculatively.

### 3. Prototype / archetype spawning (Ch. 6-7) — ECS via EnTT, decided

**Decided**: entities are ECS entities (EnTT), not an Actor class hierarchy. `entt::registry`
owns all component storage; an "entity" is just an `entt::entity` handle (a lightweight
generation-checked ID), not a heap object. This also resolves the ownership-model question that
used to be listed under Open Questions below — there's no separate `unique_ptr`-per-entity
scheme to design, the registry's own sparse-set storage *is* the pool/arena.

The book's underlying point still applies exactly: don't hand-construct an entity's full
component set at every spawn site. Define what an "enemy" or "projectile" *is* once — a small
factory function that creates an entity and emplaces its component set — and spawn by calling
that, not by repeating the same `emplace<Position>(...); emplace<Health>(...); ...` block
everywhere something needs to exist:

```cpp
// Sketch — the "prototype" is this function, not a cloneable object.
entt::entity SpawnEnemy(entt::registry& registry, Vector3 position) {
    const auto entity = registry.create();
    registry.emplace<Position>(entity, position);
    registry.emplace<Health>(entity, kEnemyMaxHealth);
    registry.emplace<EnemyTag>(entity); // empty struct — a marker component for queries
    return entity;
}
```

If enemy *variants* are needed later (a designer-tunable set of stats per enemy type, not just
one hardcoded factory function), that becomes a small data table (enemy type → component values)
read by one generic factory — still not a reason to reach for EnTT's snapshot/prototype
utilities prematurely; a plain data table is enough until it visibly isn't.

**Current state**: EnTT compiles/links/runs as part of the build, and `Engine::Registry()`
(`src/app/engine.h`) owns the one `entt::registry` instance — but nothing populates it yet. No
`Position`, `Health`, or any other real component exists, and no factory functions like
`SpawnEnemy` above exist yet either — this section describes the shape to use once they're
written, not something already in the codebase. (The earlier `ecs_smoke_test.cpp` proof-of-build
file has been deleted now that `Engine` gives EnTT a real, permanent home in the codebase.)

### 4. Resource cache (Ch. 8) — a thin layer over raylib, not a new loader

The book builds a full resource cache with its own Windows file I/O and a custom ZIP-based asset
bundle format (`Resource.zip`) — that whole layer exists because Win32 didn't hand you asset
loading for free. **raylib already does the hard part** (`LoadModel`, `LoadTexture`, `LoadShader`,
with GPU upload included). What's still worth having is a thin cache keyed by resolved path, so
loading `"models/enemy.glb"` twice doesn't upload it to the GPU twice — plus, per ADR-0004, the
book's `ResHandle`/`shared_ptr` lifetime-safety piece pulled forward alongside it, so a resource
can't be freed out from under a caller still holding a handle to it:

```cpp
// Sketch — matches src/app/resource_cache.h; adjust here if that file's shape changes.
template <typename T>
class ResourceCache {
public:
    using Loader = std::function<T(const char* path)>;
    using Unloader = std::function<void(T& resource)>;

    ResourceCache(Loader loader, Unloader unloader);

    // Loads `path` on first request (or once every prior handle to it has been released);
    // returns a shared_ptr whose deleter calls `unloader` the moment its last holder releases it.
    std::shared_ptr<T> GetHandle(const std::string& path);

    void Clear(); // drops the cache's own bookkeeping; doesn't force-unload a still-live handle

private:
    Loader loader_;
    Unloader unloader_;
    std::unordered_map<std::string, std::weak_ptr<T>> resources_; // weak: doesn't keep alive alone
};
```

`resources_` holds a `weak_ptr`, not a `shared_ptr` — the cache never keeps a resource loaded on
its own; a resource frees itself as soon as every caller's `shared_ptr<T>` handle to it is
released. That's simpler than the book's byte-budgeted LRU (still not built — see below) while
still not leaking anything nobody references anymore.

Don't build asset bundling/packing (the book's ZIP format), a pluggable per-type loader registry,
or LRU+budget eviction until there's an actual reason to (many small files, custom decode logic
beyond a single raylib call, or a measured memory-pressure problem) — that's premature for a
learning project's early demos; see ADR-0004 for the full comparison and reasoning.

**Current state**: `ResourceCache<T>` (`src/app/resource_cache.h`, header-only, templated) exists.
`Engine` (`src/app/engine.h`/`.cpp`) owns one `ResourceCache<Font>` and one `ResourceCache<Sound>`,
exposed via `Fonts()`/`Sounds()`; `Engine::Init()` loads the shared font and coin sound through
them (`fontHandle_`/`soundHandle_`, kept alive as `Engine` members since `screens.h`'s plain-C
screen code reads `font`/`fxCoin` as `extern` globals, not through a `shared_ptr`). `Engine::
Shutdown()` resets those handles and calls `Clear()` on both caches *before*
`CloseAudioDevice()`/`CloseWindow()` — required, not incidental: a handle's deleter calls
`UnloadFont`/`UnloadSound`, which need a still-open GL/audio context. No other resource type
(`Texture2D`, `Model`, ...) is cached yet; add a `ResourceCache<T>` instance the first time
something actually loads one, don't pre-instantiate for types nothing uses yet.

## Decisions Made

- **Actor (OOP class hierarchy) vs. ECS: ECS, via [EnTT](https://github.com/skypjack/entt)**
  (v4.0.0+, requires C++20). Chosen over hand-rolling a sparse-set ECS to prioritize reaching
  rendering/graphics work sooner; hand-rolling one's own ECS remains a valid *future*, deliberate
  learning project once graphics/gameplay momentum exists, not something being deferred silently.
- **Not wrapped behind a swappable abstraction, on purpose.** An ECS isn't a swappable backend
  service (unlike, say, a `Renderer` interface over GL/Vulkan) — it's the data layout and
  iteration paradigm gameplay code gets written directly against. A future move to a hand-rolled
  ECS would be a deliberate migration regardless of any indirection layer (this is how real
  engines have made this jump, e.g. Unity's GameObject → DOTS rewrote gameplay code, it didn't
  flip a config flag), so a "just in case" wrapper here would cost real clarity/performance for a
  swap unlikely to ever be exercised as designed. What's used instead: our own domain-vocabulary
  helper functions on top of EnTT as they get written (`SpawnEnemy(...)`, per §3) — reduces the
  blast radius of a future change without pretending the backend is drop-in replaceable.
- **Ownership model** — resolved as a consequence of the ECS decision, not separately: EnTT's
  `entt::registry` owns all component storage itself (sparse sets internally); an entity is a
  lightweight `entt::entity` handle, not a heap-allocated object needing its own
  `unique_ptr`/pool scheme.

## Open Questions

- **Hierarchy (scene graph fused into ECS)** — not built yet, and not documented in any skill
  file so far (only discussed in conversation). The plan: `Parent`/`Children` relationship
  components plus a topological transform-propagation system, not a separate tree-of-node-objects
  structure — the GCC `SceneNode` idea, reimplemented as ECS components + a system instead of an
  object tree. EnTT's `registry.sort<Parent>(...)` is the likely mechanism for keeping iteration
  in topological (parent-before-child) order cheaply; worth using rather than re-deriving a
  custom sort. Give this its own section in this skill (or a new skill) once it actually lands.

## Related Skills

- `engine-ai-behavior` — FSM/utility/steering/perception/pathfinding (Game Coding Complete Ch.
  11-13), for once entities exist and need behavior beyond static geometry.

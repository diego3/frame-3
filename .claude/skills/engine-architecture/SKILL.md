---
name: engine-architecture
description: Design guidance for frame-3's core systems (event manager, process manager, ECS entity/prototype spawning via EnTT, resource cache) based on "Game Coding Complete, 4th Edition" (McShaffry & Graham) Ch. 4, 6-8, modernized for raylib + C++ + 3D instead of the book's 2004-era Win32/DirectX target. Use this skill whenever adding cross-system communication, timed/multi-frame behavior (cooldowns, animations, camera effects), spawning game entities, or loading/caching 3D models, textures, or shaders. Also use it when the user asks "how should this be structured", mentions object pooling, ECS, EnTT, event manager, event bus, process manager, resource cache, or references Game Coding Complete directly. This is forward-looking design guidance, not a description of existing code — frame-3 has an `Engine` class (Ch. 5, `src/app/core/engine.h`/`.cpp`) owning window/audio lifecycle, loop driving, the `entt::registry`, an `EventManager` (`src/app/events/event_manager.h`), a `ProcessManager` (`src/app/process/process_manager.h`/`.cpp`, ticked once per frame from `Engine::Run`), and a `ResourceCache<T>` (`src/app/resource/resource_cache.h`, per ADR-0004, backing `Engine::Fonts()`/`Sounds()`/`Models()`/`Textures()`/`GetShader()`); a few real components (`LocalTransform`/`WorldTransform` via a `"Position"` `EntityFactory` loader, `BoxRenderable` via `"BoxRenderable"`, and `game/flare_reactor`'s own `Renderable`) and a `BaseGameLogic`/`HumanView` Logic/View split (§9, ADR-0010) exist and are wired into the gameplay screen, and `HumanView` composes an `IScreenElement` stack (§10, ADR-0016) instead of rendering directly; a second and third concrete game module, `game/camera_fps` (ADR-0017) and `game/flare_reactor` (RFC-0001), and the `HumanViewBase` (`app/view/`) all three share landed alongside them. `game/flare_reactor` is also this project's first real `EventManager` subscriber/emitter and first real `Process` subclass (`BeaconPulseProcess`) — the "nothing subscribes... or attaches a process yet" state this description used to describe is no longer accurate; see §§1-2's own "Current state" notes, which still need a pass to catch up.
---

# Engine Architecture (Game Coding Complete Ch. 4, 6-8 — modernized)

frame-3 is still close to the stock
[raylib-game-template](https://github.com/raysan5/raylib-game-template), reorganized into
`src/app/` (game-agnostic engine) and `src/game/<game-id>/` (a concrete game; `src/platform/` holds
shared packaging), per [ADR-0014](../../../docs/adr/0014-game-module-boundary-and-template-migration.md)
(Accepted). Three game modules exist, selected via the Makefile's `GAME` variable (ADR-0017):
`src/game/sandbox/`, the migrated raylib template itself -- now C++, not C -- with a simple
logo/title/gameplay/ending/options screen state machine (`src/game/sandbox/screens.h`) driven by
`src/game/sandbox/main.cpp`; `src/game/camera_fps/`, raylib's own "3d camera fps" example ported in
as a single-scene module with no screen state machine (see §10); and `src/game/flare_reactor/`
(RFC-0001), a single-scene module built to exercise the full event/process/render/audio/AI
pipeline end to end. The
`src/app/` layer: **`Engine`** (`src/app/core/engine.h`/`.cpp`, Ch. 5 Application layer) owns
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
// Sketch — matches src/app/events/event_manager.h; adjust here if that file's shape changes.
class EventManager {
public:
    using RawHandler = std::function<void(const void*)>;    // type-erased, one per subscriber
    using PendingAction = std::function<void()>;             // a Queue<T>'d Emit<T>, replayed later

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
    std::unordered_map<std::type_index, std::vector<RawHandler>> handlers_;
    std::vector<PendingAction> pending_;
};
```

No GUIDs, no enum to keep in sync across translation units, and adding a new event type is just
defining a new struct — the same win the book's Ch. 4 was actually chasing (avoid the
"terrible monolithic enumeration" compile-time trap), achieved with templates instead of an
integer registry.

**Current state**: `EventManager` (`src/app/events/event_manager.h`, header-only) exists and
`Engine::Events()` (`src/app/core/engine.h`) owns the one instance. `game/flare_reactor` (RFC-0001)
is the first real subscriber/emitter, both directions: `FlareReactorView`'s
`PlayerInteractElement` `Emit`s `EvtData_ActivateBeacon`, which `FlareReactorGameLogic` subscribes
to (`OnActivateBeacon`, validates proximity/state) and, once valid, `Queue`s
`EvtData_BeaconTriggered` — consumed by `FlareReactorView`'s own `OnBeaconTriggered` (pan/volume
audio, §4/ADR-0004). `app/entity/level_loader.h`'s `EvtData_EntitySpawned` (ADR-0009) is queued by
every `LevelLoader::Load` call but still has no subscriber. `game/camera_fps`'s
`EvtData_ActorJumped` (ADR-0017 follow-up) is queued but also has no subscriber yet — define one
struct per event kind as gameplay code needs to announce something, don't pre-build a taxonomy of
events speculatively.

Per [ADR-0005](../../../docs/adr/0005-event-manager-queued-dispatch-idata-lua-proposal.md)
(Partially Accepted), `EventManager` also has `Queue<T>`/`DispatchQueued()` — a deferred-dispatch
path that avoids the reentrancy hazard of a handler `Emit`-ing the same type it's currently
handling — ticked once per frame from `Engine::Run`'s `TickAndUpdateDraw`
(`src/app/core/engine.cpp`), right alongside `ProcessManager::Update`. `Emit<T>` is unchanged. Separately,
for events that need to cross a process or time boundary (networking, an event journal — neither
exists yet), there's now an **opt-in** serialization path: `ISerializableEvent`
(`src/app/events/serializable_event.h`), a stable compile-time type ID via `Fnv1aHash`
(`src/app/events/event_type_id.h`), a factory table to reconstruct a concrete type from raw bytes
(`EventTypeRegistry`, `src/app/events/event_type_registry.h`), and a minimal in-memory
`ByteWriter`/`ByteReader` (`src/app/events/byte_stream.h`). Purely local events (everything today) never
touch any of this — it's paid for only by an event type that explicitly implements
`ISerializableEvent`, and no real event type does yet (`src/tests/serializable_event_test.cpp`
exercises it with a stand-in event). `EventJournal` and the actual network transport are still just
designed, not implemented — see the ADR's Implementation status note.

### 2. Process manager (Ch. 4) — cooperative multitasking for timed behavior

Genuinely not Windows/DirectX-specific in the book — this ports close to as-is. A `Process` runs
across multiple frames and reports its own completion; a `ProcessManager` owns a list and calls
`Update(dt)` on each once per frame, removing finished ones.

```cpp
// Sketch — matches src/app/process/process.h + src/app/process/process_manager.h/.cpp; adjust here if those
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

**Current state**: `Process`/`ProcessManager` exist and `Engine::Processes()` (`src/app/core/engine.h`)
owns the one `ProcessManager` instance, ticked once per frame from `Engine::Run` (both the desktop
`while` loop and the `PLATFORM_WEB` `emscripten_set_main_loop` path go through the same
`TickAndUpdateDraw` trampoline in `engine.cpp`, so process ticking isn't duplicated per-platform).
The first real, non-hypothetical `Process` subclass now exists:
`game/flare_reactor/beacon_pulse_process.h`'s `BeaconPulseProcess` (RFC-0001 Phase 4), attached via
`processes_.Attach(...)` from `FlareReactorGameLogic::OnActivateBeacon` once a reactor activation
is validated -- animates scale/rotation/color over a fixed duration, then flips a game-specific
`Reactor::active` flag back off and calls `Succeed()`. Deliberately avoids any raylib symbol that
isn't header-only (writes its own byte-lerp `Color` helper instead of raylib's linked `ColorLerp`)
so it stays unit-testable (`tests/beacon_pulse_process_test.cpp`) the same way
`app/input/input_bindings.h`'s `IsDown`/`IsPressed` split does. Write one concrete `Process` per
timed behavior as it's needed (e.g. a future engine-level `CameraShakeProcess`), don't pre-build a
library of process types speculatively.

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
(`src/app/core/engine.h`) owns the one `entt::registry` instance. `SpawnEnemy`-style hardcoded factory
functions still don't exist — but the "small data table read by one generic factory" extension
mentioned above is now real (§6), and a few real component pairings have landed through it:
`"Position"` → `LocalTransform`/`WorldTransform` (§9's `game/sandbox/screen_gameplay.cpp` and
`game/camera_fps/main.cpp` both register this loader, identically), and `"BoxRenderable"` →
`app/scene/render_components.h`'s `BoxRenderable` (§10/ADR-0017, `game/camera_fps` only so far).
`Health`/`EnemyTag` and any other component beyond that still don't exist — write one per real
need, same discipline as always. (The earlier `ecs_smoke_test.cpp` proof-of-build file has been
deleted now that `Engine` gives EnTT a real, permanent home in the codebase.)

### 4. Resource cache (Ch. 8) — a thin layer over raylib, not a new loader

The book builds a full resource cache with its own Windows file I/O and a custom ZIP-based asset
bundle format (`Resource.zip`) — that whole layer exists because Win32 didn't hand you asset
loading for free. **raylib already does the hard part** (`LoadModel`, `LoadTexture`, `LoadShader`,
with GPU upload included). What's still worth having is a thin cache keyed by resolved path, so
loading `"models/enemy.glb"` twice doesn't upload it to the GPU twice — plus, per ADR-0004, the
book's `ResHandle`/`shared_ptr` lifetime-safety piece pulled forward alongside it, so a resource
can't be freed out from under a caller still holding a handle to it:

```cpp
// Sketch — matches src/app/resource/resource_cache.h; adjust here if that file's shape changes.
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

**Current state**: `ResourceCache<T>` (`src/app/resource/resource_cache.h`, header-only, templated) exists.
`Engine` (`src/app/core/engine.h`/`.cpp`) owns `ResourceCache<Font>`, `ResourceCache<Sound>`,
`ResourceCache<Model>`, `ResourceCache<Texture2D>`, and `ResourceCache<Shader>`, exposed via
`Fonts()`/`Sounds()`/`Models()`/`Textures()`/`GetShader(vsPath, fsPath)`. `LoadShader` takes two
file paths, not one, so the `Shader` cache is keyed via `ResourceCacheKeys::Combine()`/`Split()`
(`resource_cache.h`) rather than a bare path — generic, reusable by any future multi-argument
loader, not raylib- or `Shader`-specific. `Engine::Init()` itself loads no asset at all
([ADR-0014](../../../docs/adr/0014-game-module-boundary-and-template-migration.md) moved that out
of `Engine` -- which specific font/sound to load is a game concern, not an engine one);
`game/sandbox/main.cpp` loads the shared font and coin sound through `engine.Fonts()`/
`engine.Sounds()` itself, right after `engine.Init()` returns, keeping the handles alive as
`main()` locals for as long as the app runs (`sandbox/screens.h`'s screen code still reads
`font`/`fxCoin` as plain `extern` globals, not through a `shared_ptr`); nothing loads a
`Model`/`Texture2D`/`Shader` at `Init()` time — those three exist for gameplay code to call once it
needs to.

**Handle lifetime is a real hazard, not a theoretical one** — reproduced as an actual `SIGSEGV`
while implementing this (see ADR-0004's "A real crash this surfaced"): any handle from any of the
five caches (`Fonts()`/`Sounds()`/`Models()`/`Textures()`/`GetShader()`) **must** be released
before `Engine::Shutdown()` runs, or its `Unload*` call fires into an already-closed GL/audio
context. `Engine` itself holds none of these handles (ADR-0014 moved the one former exception,
font/sound, out to `main()`) — every handle is whichever caller's responsibility to release, on the
same terms. A real component now exists (§3, §9) but doesn't hold one of these resource-cache
handles (`LocalTransform`/`WorldTransform` are plain data, not `Model`/`Texture2D`/`Shader`
handles) — once one does, whatever clears the `entt::registry` needs to run before `Shutdown()`,
not after.

Not cached: raylib's standalone `LoadMaterials(fileName, &count)` (an out-param array + count,
not a single `T` — doesn't fit `ResourceCache<T>`'s shape). `Model`'s own bundled materials
(populated automatically by `LoadModel`) cover the common case; this is only relevant for a bare
`.mtl` file used independent of a full model, which nothing needs yet.

### 5. Entity hierarchy (Ch. 9-10) — `Relationship` + `LocalTransform`/`WorldTransform`, via ADR-0002

The book's `SceneNode` tree doesn't fit an ECS directly — there's no per-node object to hang
parent/child pointers off of, just `entt::entity` handles. [ADR-0002](../../../docs/adr/0002-scene-graph-hierarchy-options.md)
(Accepted) evaluated adopting a ready-made scene graph (flecs' `ChildOf`, OpenSceneGraph,
VulkanSceneGraph, a raylib-specific community project) against building the small amount of code
this actually needs, fused directly into EnTT — every "adopt" option either meant abandoning EnTT
(already decided non-swappable, §3) or bringing a competing renderer (raylib is already the
renderer). The chosen shape is EnTT's own documented "Pattern A" for exactly this problem
(`docs/md/entity.md`, "Hierarchies and the like"): an intrusive doubly-linked list of children,
plus a topological transform-propagation system:

```cpp
// Sketch — matches src/app/scene/hierarchy.h + src/app/scene/transform.h; adjust here if those files' shape
// changes.
struct Relationship {
    std::size_t children{};
    entt::entity first{entt::null};
    entt::entity prev{entt::null};
    entt::entity next{entt::null};
    entt::entity parent{entt::null};
};

struct LocalTransform { Vector3 position; Quaternion rotation; Vector3 scale; };
struct WorldTransform  { Matrix matrix; };

void SetParent(entt::registry& registry, entt::entity child, entt::entity parent);
void RemoveParent(entt::registry& registry, entt::entity child);
void PropagateTransforms(entt::registry& registry);
```

**Not `registry.sort<Relationship>(...)`.** ADR-0002 left "sort for topological order vs. something
else" as an explicit open question. What actually shipped recurses down from every root instead
(an entity with no `Relationship`, or one whose `parent == entt::null`), computing each child's
`WorldTransform` only once its parent's has already been written by the same call — correct at any
hierarchy depth regardless of EnTT's internal storage order, with no sort step to keep synchronized
as parents change. Simpler to reason about at this project's scale (a handful of entities per
scene); worth revisiting only if profiling ever shows the recursion itself is hot.

**Current state**: `Relationship` (`src/app/scene/hierarchy.h`) and `LocalTransform`/`WorldTransform`
(`src/app/scene/transform.h`) exist, both header-only. `PropagateTransforms` is ticked once per frame from
`Engine::Run`'s `TickAndUpdateDraw` (`src/app/core/engine.cpp`), after `ProcessManager::Update` and
before `updateAndDraw()` — the same per-frame-tick spot §§1-2's systems use. One real gameplay
entity uses this now (§9's `"Position"` component loader emplaces `LocalTransform`+`WorldTransform`,
no `Relationship` — a standalone root); `src/tests/hierarchy_test.cpp` exercises
`SetParent`/`RemoveParent`/`PropagateTransforms` directly against bare `entt::registry` entities,
including a three-level hierarchy attached in scrambled (non-parent-before-child) creation order,
specifically to prove correctness doesn't depend on EnTT's storage order.

### 6. Data-driven entity/component loading (Ch. 6-7) — `EntityFactory` over a format-agnostic tree, via ADR-0008

The book's `ActorFactory` reads an `<Actor>` XML element and dispatches each child element to a
registered creation function by tag name. [ADR-0008](../../../docs/adr/0008-data-driven-entity-loading-yaml.md)
(Accepted) adapts this to a component-loader registry over a small, concrete value tree —
`EntityDefNode` — rather than a virtual per-format node interface, so the one thing that actually
varies between formats (file syntax) is fully consumed during parsing and nothing downstream needs
runtime polymorphism to stay format-agnostic:

```cpp
// Sketch — matches src/app/entity/entity_def.h, entity_file_parser.h/_yaml.h/.cpp, entity_factory.h;
// adjust here if those files' shape changes.
class EntityDefNode {
public:
    using List = std::vector<EntityDefNode>;
    using Map  = std::unordered_map<std::string, EntityDefNode>;
    bool HasKey(const std::string& key) const;
    const EntityDefNode& Get(const std::string& key) const;    // throws if absent
    const EntityDefNode* TryGet(const std::string& key) const; // nullptr if absent
    std::string AsString(const std::string& fallback = "") const;
    float AsFloat(float fallback = 0.0f) const;
    int AsInt(int fallback = 0) const;
    const List& AsList() const;
    const Map& AsMap() const;
};

class IEntityFileParser {
public:
    virtual EntityDefNode Parse(const std::string& fileContents) = 0;
};

class EntityFactory {
public:
    using ComponentLoader = std::function<void(entt::registry&, entt::entity, const EntityDefNode&)>;
    void RegisterComponentLoader(const std::string& componentName, ComponentLoader loader);
    entt::entity Create(entt::registry& registry, const EntityDefNode& def) const;
};
```

`YamlEntityFileParser` (backed by [mini-yaml](https://github.com/jimmiebergmann/mini-yaml),
vendored) is the one `IEntityFileParser` implementation that landed; swapping to JSON or another
format later means writing a new implementation and changing which one gets constructed, with no
change to `EntityDefNode`/`EntityFactory`/any `ComponentLoader`. `EntityFactory::Create` skips a
component name with no registered loader (forward-compatible with a definition file written for a
newer build) rather than failing the whole entity.

**A real mini-yaml limitation, confirmed while implementing this**: this checkout doesn't parse
flow-style maps/sequences (`{ x: 1 }`, `[a, b]`) — silently misparses them as an empty scalar
rather than throwing. Entity/level definition files must use block style (newline + indentation).
See ADR-0008's Implementation status note.

**Current state**: `EntityDefNode` (`src/app/entity/entity_def.h`), `IEntityFileParser`
(`src/app/entity/entity_file_parser.h`), `YamlEntityFileParser`
(`src/app/entity/entity_file_parser_yaml.h`/`.cpp`), and `EntityFactory` (`src/app/entity/entity_factory.h`) all
exist and are unit-tested against fake components (`src/tests/entity_def_test.cpp`,
`entity_file_parser_yaml_test.cpp`, `entity_factory_test.cpp`) — composed into a real
`LevelLoader` (§7), which a real gameplay screen now constructs and calls (§9's
`game/sandbox/screen_gameplay.cpp`), with one real (non-test-fake) component loader registered:
`"Position"` (§3).

### 7. Level loading (Ch. 6-7, "book's `VLoadGame`") — `LevelLoader`, without a `BaseGameLogic` yet, via ADR-0009

[ADR-0009](../../../docs/adr/0009-level-loading-actor-placement.md) (Accepted) composes §6's pieces
with placement + per-instance overrides, without needing a `BaseGameLogic` host object (`m_actors`
is just this project's `entt::registry`; process management is already `ProcessManager`, §2):

```cpp
// Sketch — matches src/app/entity/level_loader.h/.cpp, entity_def.h's MergeOverrides; adjust here if
// those files' shape changes.
struct EvtData_EntitySpawned { entt::entity entity; };  // this project's first real event type

EntityDefNode MergeOverrides(const EntityDefNode& base, const EntityDefNode& overrides);

class LevelLoader {
public:
    using FileReader = std::function<std::string(const std::string& path)>;
    LevelLoader(EntityFactory& entityFactory, IEntityFileParser& parser,
                FileReader readFile = ReadWholeFile);   // file_io.h
    std::vector<entt::entity> Load(entt::registry& registry, EventManager& events,
                                    const std::string& levelPath);
};
```

`Load` fires `EvtData_EntitySpawned` via `EventManager::Queue` (§1) for every entity it creates —
unconditionally, even though no `RemoteView`/`AIView`-equivalent subscriber exists yet (see
ADR-0009's "View-plurality seam, kept on purpose": the whole value is that `LevelLoader` never has
to change once one does — a deliberate exception to this project's usual defer-until-needed
discipline).

**Current state**: `MergeOverrides` (`src/app/entity/entity_def.h`), `LevelLoader`
(`src/app/entity/level_loader.h`/`.cpp`), and `EvtData_EntitySpawned` (`src/app/entity/level_loader.h`) all exist,
tested against in-memory fake files (`src/tests/level_loader_test.cpp`) via `LevelLoader`'s
injectable `FileReader`. Now wired into a real gameplay screen — see §9
([ADR-0010](../../../docs/adr/0010-base-game-logic-and-igameview.md), Accepted):
`game/sandbox/screen_gameplay.cpp` constructs one and calls `VLoadLevel` on the first real level
file (`assets/levels/level_01.yaml`).

### 8. Engine/game config (Ch. 5) — two-tier, writable, via ADR-0011

[ADR-0011](../../../docs/adr/0011-engine-and-game-config.md) (Accepted) splits config along the
same boundary ADR-0001 Decision 2 already drew: `Engine` owns exactly the parameters it already
takes/uses, a specific game owns whatever it needs beyond that:

```cpp
// Sketch — matches src/app/core/engine_config.h/.cpp, src/game/sandbox/game_config.h.
struct EngineConfig {
    int screenWidth = 800, screenHeight = 450;
    bool fullscreen = false;
    int targetFps = 60;
    float masterVolume = 1.0f;
};
EngineConfig LoadOrCreateEngineConfig(const std::string& path = "config/engine.yaml");

struct GameConfig { std::string characterTexturePath, coinSoundPath; /* add more as needed */ };
GameConfig LoadOrCreateGameConfig(const std::string& path = "config/game.yaml");
```

Both read via §6's `YamlEntityFileParser`; writing (new — §6's parser only ever needed to read) is
a minimal hand-written emitter scoped to each struct's own flat fields, not a general
`EntityDefNode`-to-YAML serializer. `config/` lives outside `assets/` (gitignored, generated at
runtime with defaults on first run) for the same reason the book keeps `PlayerOptions.xml` out of
its packaged resource bundle (§4's read-only, version-controlled content root).

**Current state**: `Engine::Init()` takes an `EngineConfig` (not raw `screenWidth`/`screenHeight`
ints); `Engine::Run()` uses `config.targetFps` for `SetTargetFPS`/`emscripten_set_main_loop`'s rate
argument and the frame-budget-SLO warning threshold. `game/sandbox/main.cpp`'s `main()` calls
`engine.Init(LoadOrCreateEngineConfig(), title)`. `fullscreen`/`masterVolume` are loaded/saved but
not yet applied to real window/audio state. First-run defaults (both `EngineConfig` and
`InputBindings`, ADR-0013) now seed from a shipped, versioned `defaultsPath` argument
(`assets/config/engine.yaml`/`assets/config/keybindings.yaml`, staged into `resources/config/`)
instead of a bare struct literal (ADR-0011's 2026-08-07 addendum) — the player-writable runtime
copy itself is unchanged. `GameConfig` got its first real fields and caller
(docs/adr/0014): `characterTexturePath`/`coinSoundPath`, read by `game/sandbox/main.cpp` right
after `engine.Init()` returns and passed to `engine.Fonts()`/`engine.Sounds()` instead of
hardcoding the paths there.

### 9. Logic/View split (Ch. 9-10) — `BaseGameLogic` + `IGameView`, via ADR-0010

[ADR-0010](../../../docs/adr/0010-base-game-logic-and-igameview.md) (Accepted) activates the split
ADR-0001 Decision 2 deferred and ADR-0009 (§7) reaffirmed "not yet," ahead of either of that ADR's
own named triggers actually being hit — a deliberate exception, same category as §7's
`EvtData_EntitySpawned` call, made because `HumanView`/`RemoteView`/`AIView` are explicitly the
next-planned work and retrofitting the seam after any get built independently costs more than
building it once now:

```cpp
// Sketch — matches src/app/view/game_view.h, base_game_logic.h/.cpp (game-agnostic, per ADR-0014);
// src/game/sandbox/human_view.h/.cpp (concrete, game-layer). Adjust here if those files' shape
// changes.
enum class GameViewType { Human, Remote, AI, Other };
using GameViewId = std::uint32_t;

class IGameView {
public:
    virtual void VOnAttach(GameViewId id, std::optional<entt::entity> actorId) = 0;
    virtual void VOnUpdate(float dt) = 0;   // input + view-local state, never rendering
    virtual void VOnRender(float dt) = 0;   // rendering only
    virtual GameViewType VGetType() const = 0;
};

class BaseGameLogic {
public:
    BaseGameLogic(entt::registry& registry, EventManager& events, ProcessManager& processes,
                  LevelLoader& levelLoader);   // takes what it needs, owns none of it (§§1-2, §7)
    GameViewId AttachView(std::unique_ptr<IGameView> view,
                          std::optional<entt::entity> actorId = std::nullopt);
    virtual void VLoadLevel(const std::string& levelPath);   // Loading -> Running via §7's LevelLoader
    void VOnUpdate(float dt);   // ticks every attached view's VOnUpdate -- never VOnRender
};
```

Dropped from the book's `IGameView`/`GameCodeApp`, with no raylib/OpenGL/Win32 analogue at this
project's scope: `VOnRestore()` (DirectX device-loss recovery), `VOnMsgProc(AppMsg)` (raylib
already hands out polled input directly — `HumanView::VOnUpdate` calls `IsKeyDown`/etc. itself, no
message-queue translation layer), and the book's fuller `MainMenu`/`WaitingForPlayers`/... state
machine (`screens.h`'s `GameScreen` enum already answers "what screen"; `BaseGameLogic`'s own
state only needs to cover the `GAMEPLAY` screen's inner simulation: `Loading`/`Running`/`Paused`).

**Who drives it**: not `Engine` (stays Ch. 5-only, ADR-0001 Decision 2 unchanged) —
`game/sandbox/screen_gameplay.cpp` itself, holding `BaseGameLogic`/`HumanView` as file-local
statics across separate `Init`/`Update`/`Draw`/`Unload` calls, the same pattern `main.cpp` already
uses for screen-transition state. This used to go through a small `extern "C"` bridge
(`gameplay_bridge.h`/`.cpp`) while the screen was still plain C; [ADR-0014](../../../docs/adr/0014-game-module-boundary-and-template-migration.md)
removed that indirection once the screen itself became a real C++ translation unit —
`BaseGameLogic`/`HumanView` are reachable directly, no bridge needed. `Engine::Current()`
(`src/app/core/engine.h`/`.cpp` — not anticipated by ADR-0010's own sketch) is how the screen reaches
`Engine`'s `registry_`/`eventManager_`/`processManager_` to construct one: a static accessor backed
by the same de facto singleton the `Run()` emscripten trampoline already relied on internally.

**Current state**: `IGameView`, `BaseGameLogic` (`src/app/view/`) exist; three concrete `HumanView`-
family views now subclass the shared `HumanViewBase` (`app/view/human_view_base.h`/`.cpp`, §10) --
`game/sandbox/human_view.h`, `game/camera_fps/human_view.h` (ADR-0017), and
`game/flare_reactor/human_view.h` (RFC-0001), the latter rebased onto `HumanViewBase` once ADR-0017
merged, dropping its own until-then-duplicated copy of the same plumbing. `BaseGameLogic` gained
`Pause()`/`Resume()` beyond the ADR's own sketch (freezes attached views' `VOnUpdate` only;
`ProcessManager` keeps running regardless) and `VLoadLevel` is now `virtual`, returning every
spawned entity in file order -- `game/camera_fps/game_logic.h`'s `CameraFpsLogic` is the first
subclass, running its own physics step before calling `BaseGameLogic::VOnUpdate`.
`game/sandbox/HumanView` still drives its possessed actor via hardcoded arrow-key movement in its
own `VOnUpdate`; `game/flare_reactor/FlareReactorView` moved off that onto ADR-0013's
`InputBindings` instead (`config/keybindings.yaml`, rebindable). The `ProcessManager&`/
`ResourceCache<Sound>&` every `HumanView`-family constructor takes (the "human" half of
`GCC4::HumanView`'s own dependencies) has a real caller now too: `FlareReactorView::
OnBeaconTriggered` (subscribed to `EvtData_BeaconTriggered`, §1) computes pan/volume from
distance/direction and plays a `ResourceCache<Sound>` handle -- `game/sandbox`'s own `HumanView`
still doesn't call into either. `AIView` (`GameViewType::AI`) is implemented too now --
`game/flare_reactor/ai_view.h`, one instance per sentinel actor (identified by a `SentinelAI`
component, not a bare tag; see the `engine-ai-behavior` skill); `RemoteView` remains named, not
built. Verified end-to-end under
`xvfb-run`: `Engine::Init` → `InitGameplayScreen` (loads `assets/levels/level_01.yaml`, spawns one
entity) → several `Update`/`Draw` cycles → `UnloadGameplayScreen` → `Engine::Shutdown`, no crash.

### 10. `IScreenElement` stack (Ch. 10) — `HumanView` composes layered elements, via ADR-0016

[ADR-0016](../../../docs/adr/0016-screen-element-stack.md) (Accepted) is what §9's own "Current
state" pointed at: `HumanView` used to render directly in one `VOnRender` body, and
`screen_gameplay.cpp`'s `DrawGameplayScreen` bolted a raw `DrawText` call on top of that, entirely
outside `HumanView`. Neither could be reordered, hidden, or composed with anything else.

```cpp
// Matches src/app/view/screen_element.h; PushElement/RemoveElement live in
// app/view/human_view_base.h/.cpp, shared by every concrete HumanView-family view (game/sandbox,
// game/camera_fps, game/flare_reactor).
class IScreenElement {
public:
    virtual void VOnUpdate(float dt) = 0;
    virtual void VOnRender(float dt) = 0;
    virtual int VGetZOrder() const = 0;
    virtual void VSetZOrder(int zOrder) = 0;
    virtual bool VIsVisible() const = 0;
    virtual void VSetVisible(bool visible) = 0;
};
using ScreenElementId = std::uint32_t;
```

Dropped from the book's `IScreenElement` (`GameCode4/interfaces.h`), same category of cut §9 already
made for `IGameView`: `VOnRestore()`/`VOnLostDevice()` (DirectX device-loss recovery, no
raylib/OpenGL analogue) and `VOnMsgProc(AppMsg)` (no Win32 message queue -- input is already polled
state). The book's own `operator<` on the interface is replaced by whoever owns the elements sorting
on `VGetZOrder()` itself.

**Current state**: `IScreenElement`/`ScreenElementId` live in `app/` (game-agnostic, same reasoning
as `IGameView` -- ADR-0014). The stack plumbing itself -- `PushElement`/`RemoveElement`, owning a
`vector<pair<ScreenElementId, unique_ptr<IScreenElement>>>` (paired with an id rather than the
book's `list<shared_ptr<...>>`, because `IScreenElement` unlike `IGameView` has no attach step to
hang a self-known id on, and nothing here needs shared ownership), `VOnRender`'s `stable_sort`-by-
`VGetZOrder()`-then-dispatch (mirroring the book's own `m_ScreenElements.sort()` pass), and
`VOnAttach` -- now lives in `app/view/human_view_base.h`/`.cpp`'s `HumanViewBase` (**not** `HumanView`
itself), promoted there once a second concrete `IGameView` needed the exact same plumbing (see
below and [ADR-0017](../../../docs/adr/0017-camera-fps-second-game-module.md)). Two real elements
exist in `game/sandbox/human_view.cpp`: `GameplayScene` (the 3D camera/entity render pass that used
to be `VOnRender`'s direct body) and `GameplayHud` (the "PRESS ENTER..." prompt, promoted out of
`screen_gameplay.cpp`, z-ordered above `GameplayScene` so it visually layers on top -- mirrors
`TeapotWarsHumanView` pushing `StandardHUD` alongside the base scene). No `BaseUI`-equivalent
convenience base, no console, no modal input priority (`VOnMsgProc`'s reverse-order dispatch was
dropped) -- see ADR-0016's own Tradeoffs.

**`HumanViewBase` and the second game module (ADR-0017)**: `game/camera_fps/` (raylib's own "3d
camera fps" example) is frame-3's second concrete game module, and its `CameraFpsView` is the
second `IGameView` to need this stack -- with a completely different per-frame behavior (WASD+mouse
FPS body movement vs. sandbox's arrow-key box nudging). That confirmed exactly what ADR-0015 said
to wait for a second data point before generalizing: `HumanViewBase : public IGameView` now holds
the stack plumbing and a protected `UpdateElements(dt)` helper. `VOnUpdate` has a default body now
(just `UpdateElements(dt)`), not pure virtual -- `game/camera_fps`'s `CameraFpsView` doesn't
override it at all (its own per-frame work fully lives in a pushed `PlayerMovementElement`, see
below), while `game/sandbox`'s `HumanView` still does (it moves its possessed actor directly, not
through a pushed element). Both subclass `HumanViewBase` now, keeping only what's actually
game-specific.

`camera_fps` also uses the same data-driven wiring §6-7 already describe, not a special case: its
player and 4 towers are real actors, spawned via `BaseGameLogic`/`EntityFactory`/`LevelLoader` off
`assets/levels/camera_fps.yaml` (reusing `assets/entities/player.yaml`, plus a new
`assets/entities/tower.yaml`). Three new components came out of this, all in
`game/camera_fps/components.h` except where noted: `PlayerBody` (velocity/dir/isGrounded -- kept
game-local since its tuning is specific to this movement scheme, not a generic physics body
ADR-0012 hasn't designed yet), `FirstPersonCameraRig` (the `Camera3D` itself plus its look/head-bob
easing state -- seeded onto the actor by a `CameraFpsView::VOnAttach` override, since it's
view/presentation setup, unlike `PlayerBody` which `main.cpp` emplaces as Logic-owned simulation
state), and `BoxRenderable` (size/color -- `app/scene/render_components.h`, the first real render
component, ADR-0010's own Open Questions flagged this as undecided; game-agnostic by nature but not
yet applied to `game/sandbox`'s own hardcoded-cube `GameplayScene`). `CameraFpsView` itself holds
**no** per-frame state at all now, not even the camera -- `UpdateBody`/`UpdateCameraFPS` became free
functions taking components by reference. Important gotcha this surfaced: entt can relocate a
component pool's storage on any create/destroy of that same component type, so `FpsScene` (which
used to hold a `const Camera3D&` safely, back when it was a stable view member) now re-fetches
`FirstPersonCameraRig` via `registry.try_get<...>(actor)` inside every `VOnRender` call instead of
caching a pointer across frames.

Two more pieces got pulled out of `game/camera_fps/human_view.cpp` into `app/`, both because
wrapping an already-generic thing is itself generic, not because a second game needed them (unlike
`HumanViewBase` above): `app/scene/scene_renderer.h`'s `DrawBoxRenderables(registry)` -- a free function
that draws every `BoxRenderable` entity at its already-computed `WorldTransform` (the scene graph's
own output, ADR-0002), so `FpsScene::VOnRender` doesn't hand-roll that loop itself; any view can
call it from inside its own `BeginMode3D`/`EndMode3D` (it doesn't touch the camera, which differs
per game). And `app/view/debug_overlay_screen_element.h`'s `DebugOverlayScreenElement`, an `IScreenElement`
wrapping `DebugOverlay` (F3 HUD) -- `camera_fps` folds it into its own stack (safe there because it
has exactly one view alive for the whole run) unlike `game/sandbox`, where ADR-0016's reasoning for
keeping `DebugOverlay` *outside* any view's stack (it must survive `LOGO`/`TITLE`/`OPTIONS`/
`ENDING`, screens with no `HumanView` at all) still applies unchanged -- `game/sandbox/main.cpp`
still calls `UpdateDebugOverlay`/`DrawDebugOverlay` directly, not through this. Still no
`GameConfig` for `camera_fps` -- this module has no assets to configure.

Landing a level with more than one entity for the first time also surfaced a real gap:
`BaseGameLogic::VLoadLevel` used to return `void`, so both games guessed "the player" from
registry-iteration order -- safe only because `game/sandbox`'s level had exactly one entity.
`VLoadLevel` now returns `std::vector<entt::entity>` (forwarding `LevelLoader::Load`'s own return
value), so a caller takes `spawned[0]` per the level file's own `actors[]` order instead --
`game/sandbox/screen_gameplay.cpp` was updated to match. The `src/Makefile` gained a
`GAME ?= sandbox` build selector (first time two game modules needed choosing between) to make all
of this buildable.

**`CameraFpsLogic` -- the first real `BaseGameLogic` subclass.** The FPS movement/physics
integration (`UpdateBody`) initially ran straight out of `CameraFpsView::VOnUpdate`, which read
raw input *and* wrote `PlayerBody`/`LocalTransform` in the same method -- exactly the coupling this
section's own Logic/View split exists to prevent, and `BaseGameLogic::VLoadLevel`'s own doc comment
had already left "a future game-specific `BaseGameLogic` subclass" as an explicit, unused seam.
`game/camera_fps/game_logic.h`/`.cpp` is that subclass now: `CameraFpsLogic::VOnUpdate` (overriding
`BaseGameLogic::VOnUpdate`, made `virtual` for this) advances every actor with a `MovementIntent`/
`PlayerBody`/`LocalTransform` one physics step -- `registry_.view<...>()` here *is* the ECS-native
form of "`GameLogic` walks its actors, updating their components," no separate actor list to walk
by hand -- *then* calls the base class to tick views, so a view renders this frame's
already-integrated position. It also queues a new `EvtData_ActorJumped` via `events_`
(`EventManager`, §1) whenever the physics step triggers a jump -- the ported example's own
commented-out "Sound can be played at this moment" hook, done the way `app/entity/level_loader.h`'s
`EvtData_EntitySpawned` already established (fire it even with no subscriber yet).

`MovementIntent` (`facingYaw`/`side`/`forward`/`jumpPressed`/`crouchHold`, not `PlayerInput`) is
the seam that crosses the Logic/View boundary -- named and shaped so nothing about it is
human-specific: `CameraFpsLogic` only ever reads whatever `MovementIntent` an actor has, with no
idea whether a human or a future `AIView` produced it. `facingYaw` (not `lookYaw`) is the same
reasoning applied to one field -- "which way this actor faces," a fact about the actor's movement,
not the rendering camera, even though its only source today is `FirstPersonCameraRig`.

Reading input and publishing `MovementIntent` -- plus easing the camera to follow the actor -- used
to be `CameraFpsView::VOnUpdate`'s own ~40-line body even after physics moved out. That became its
own `IScreenElement`, `PlayerMovementElement` (mirrors `FpsScene`/`FpsHud`/
`DebugOverlayScreenElement`, just for `VOnUpdate` instead of `VOnRender`) -- at which point
`CameraFpsView` stopped overriding `VOnUpdate` at all, since its whole job was `UpdateElements(dt)`
once nothing else needed doing directly. `HumanViewBase::VOnUpdate` (§10) gained a default body
(`UpdateElements(dt)`) for exactly this -- `game/sandbox`'s `HumanView` still overrides it, since it
moves its possessed actor directly rather than through a pushed element.

## Conventions

- **Alias a dense or repeated `std::function`/container type as a `using` inside the class that
  owns it**, instead of inlining the raw type at every member/parameter site. Every system sketched
  above that stores one already does this: `ResourceCache<T>::Loader`/`Unloader` (§4),
  `EntityDefNode::List`/`Map` (§6), `EntityFactory::ComponentLoader` (§6), `LevelLoader::FileReader`
  (§7), `EventManager::RawHandler`/`PendingAction` (§1), `EventTypeRegistry::Factory` (§1's
  serialization path), `InputBindings::BindingMap` (ADR-0013, `src/app/input/input_bindings.h`).
  Apply it once a type is used more than once (especially across a header/`.cpp` pair, e.g.
  `BindingMap` shared by `InputBindings` and `input_bindings.cpp`'s free functions) or its inline
  form makes a declaration hard to scan at a glance — not for a type spelled out exactly once with
  a short signature.

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

- **How hierarchy (§5) interacts with `ModelSkeleton`'s bone hierarchy**, for an entity whose
  `Model` is itself skinned/animated — likely orthogonal (bone hierarchy stays internal to one
  `Model`'s skinning; entity-to-entity hierarchy via `Relationship` is a separate concern) but not
  yet worked through in code, per ADR-0002's own Open Questions.

## Related Skills

- `engine-ai-behavior` — FSM/utility/steering/perception/pathfinding (Game Coding Complete Ch.
  11-13), for once entities exist and need behavior beyond static geometry.

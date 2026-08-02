---
name: engine-architecture
description: Design guidance for frame-3's core systems (event bus, process manager, actor/prototype spawning, resource cache) based on "Game Coding Complete, 4th Edition" (McShaffry & Graham) Ch. 4, 6-8, modernized for raylib + C++ + 3D instead of the book's 2004-era Win32/DirectX target. Use this skill whenever adding cross-system communication, timed/multi-frame behavior (cooldowns, animations, camera effects), spawning game entities, or loading/caching 3D models, textures, or shaders. Also use it when the user asks "how should this be structured", mentions object pooling, event bus, process manager, resource cache, or references Game Coding Complete directly. This is forward-looking design guidance, not a description of existing code — frame-3 is a fresh raylib-game-template checkout with none of these systems built yet.
---

# Engine Architecture (Game Coding Complete Ch. 4, 6-8 — modernized)

frame-3 doesn't have any of these systems yet — it's the stock
[raylib-game-template](https://github.com/raysan5/raylib-game-template) (currently plain C, not
C++), with a simple logo/title/gameplay/ending/options screen state machine
(`src/screens.h`) and nothing else. This skill exists so that when each of these systems gets
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

### 1. Event bus (Ch. 4) — typed, not GUID+enum

The book's Event Manager multicasts by a 32-bit GUID and a monolithic event-type enum, dispatched
through hand-rolled delegate objects — idiomatic C++ for a pre-`std::function`, pre-templates-were-
trusted-for-this era. **Don't port that literally.** A modern equivalent, and — not coincidentally
— the same shape already proven out in this project's sibling 2D engine (`frame`'s Go
`event.Subscribe[T]`/`Emit`): a bus keyed by `std::type_index`, with `std::function<void(const T&)>`
handlers.

```cpp
// Sketch — adjust once this actually gets built.
class EventBus {
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

### 2. Process manager (Ch. 4) — cooperative multitasking for timed behavior

Genuinely not Windows/DirectX-specific in the book — this ports close to as-is. A `Process` runs
across multiple frames and reports its own completion; a `ProcessManager` owns a list and calls
`Update(dt)` on each once per frame, removing finished ones.

```cpp
// Sketch.
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

### 3. Prototype / object pool spawning (Ch. 6-7) — contingent on the Actor/ECS decision

The book spawns actors by cloning a prototype defined once (originally from an XML template) —
avoids hand-building every enemy/projectile field-by-field at each spawn call site, and centralizes
"what does an enemy look like" in one place a designer can tune.

**This one is genuinely contingent on the still-open scene-graph/ECS question** — whether
"spawning" means constructing an Actor-hierarchy object or creating an ECS entity + attaching an
archetype's component set is a different mechanism either way. What doesn't change regardless of
that answer: **don't hand-construct an entity's full field set at every spawn site.** Define what
an "enemy" or "projectile" is once (an ECS archetype, or a prototype object to clone — whichever
the ECS decision lands on), and spawn by instantiating that one definition. Revisit this section
once that decision is made; the principle stays, the mechanism doesn't.

### 4. Resource cache (Ch. 8) — a thin layer over raylib, not a new loader

The book builds a full resource cache with its own Windows file I/O and a custom ZIP-based asset
bundle format (`Resource.zip`) — that whole layer exists because Win32 didn't hand you asset
loading for free. **raylib already does the hard part** (`LoadModel`, `LoadTexture`, `LoadShader`,
with GPU upload included). What's still worth having is a thin cache keyed by resolved path, so
loading `"models/enemy.glb"` twice doesn't upload it to the GPU twice:

```cpp
// Sketch.
class ResourceCache {
public:
    Model& GetModel(const std::string& path) {
        auto it = models_.find(path);
        if (it != models_.end()) return it->second;
        return models_.emplace(path, LoadModel(path.c_str())).first->second;
    }
    ~ResourceCache() { for (auto& [_, m] : models_) UnloadModel(m); }
private:
    std::unordered_map<std::string, Model> models_;
};
```

Don't build asset bundling/packing (the book's ZIP format) until there's an actual reason to (many
small files, or a distribution/build-size concern) — that's premature for a learning project's
early demos.

## Open Questions (don't let this skill presuppose an answer)

- **Actor (OOP class hierarchy) vs. ECS** — unresolved; affects §3 directly and touches how
  Update/Render get driven for entities in general. See the scene-graph/ECS discussion for
  context once it's written down somewhere durable (currently just conversation history).
- **Ownership model** — `std::unique_ptr` in a central manager (closest analog to `frame`'s Go
  `object.Manager` owning every `*GameObject`) vs. pools/arenas (more raylib-idiomatic, closer to
  what a real AAA engine would do). Not decided; don't assume one in code that doesn't need to.

## Related Skills

- `engine-ai-behavior` — FSM/utility/steering/perception/pathfinding (Game Coding Complete Ch.
  11-13), for once entities exist and need behavior beyond static geometry.

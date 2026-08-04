# 4. Event manager: queued dispatch, plus a serialization contract for networking and an event journal

- Status: Proposed
- Date: 2026-08-03

## Context

[ADR-0002](0002-event-manager-and-process-manager-game-loop.md) already shipped `EventManager`
(`src/app/event_manager.h`): pub/sub keyed by `std::type_index`, handlers are
`std::function<void(const T&)>`, and `Emit<T>` dispatches **synchronously and immediately** — no
queue, no per-frame budget. That ADR explicitly chose this over the book's GUID+monolithic-enum
multicast mechanism, reasoning that `std::type_index` dispatch doesn't carry the lookup/dispatch
cost the book's `VTick(20ms)` budget was defending against. `Engine` (`src/app/engine.h`) owns the
one instance, exposed via `Events()`. As of ADR-0002, nothing in the codebase calls `Subscribe`
or `Emit` yet — no event struct types exist either.

A follow-up proposal asks for "more detail" on the event manager, laying out *Game Coding
Complete* Ch. 4's fuller design: an `IEventData` interface (event type, timestamp, serialization)
with a `BaseEventData` helper base, GUID-based type identification, two dispatch modes —
`VTriggerEvent` (immediate) and `VQueueEvent` (deferred to next frame, via two alternating
queues) — and a Lua scripting bridge.

### Revision note: this ADR's scope changed mid-review

The first draft of this ADR rejected GUID typing and `IEventData`/`BaseEventData` outright,
reasoning that `std::type_index` already gives free, boilerplate-free type identity for local,
in-process dispatch. That reasoning still holds **for local dispatch** — but it implicitly assumed
the `EventManager` would only ever need to identify event types *within one running process*.

Follow-up direction changed that assumption on purpose: the `EventManager` is meant to be the
central decoupling point between engine subsystems, and it should be designed **from the start**
to support two things that cross a process or a time boundary, not bolted on after the fact:

1. **Multiplayer networking** — an event (or a summary of one) serialized, sent to another
   process, and reconstructed there.
2. **Event journal (save-file versioning / replay)** — events serialized to disk so a session can
   be replayed or a save can be loaded by a binary built later than the one that wrote it.

Both of these need something `std::type_index` cannot provide: a **stable identity that survives
outside a single running process** (`std::type_index`'s ordering/hash is implementation-defined
and not guaranteed to match between two separately-launched processes, let alone two builds
compiled months apart), plus an actual byte representation of the event's data. This section
below replaces §§1-2 of the original evaluation with a revised design that adds that, while
keeping local pub/sub exactly as lightweight as ADR-0002 shipped it for events that never leave
the process.

## Evaluation, piece by piece

### 1. A stable wire/journal type ID — needed, but neither `std::type_index` nor the book's hand-picked GUID literal is the right shape

Three candidate mechanisms, and why none of the obvious ones is quite right on its own:

| Mechanism | Stable across processes? | Stable across rebuilds/patches? | Boilerplate per event |
|---|---|---|---|
| `std::type_index(typeid(T))` (today, local-only) | No — implementation-defined, not a wire format | No | None |
| Book's hand-picked hex literal (`0x77dd2b3a`) | Yes, by construction | Yes, by construction | One manually-authored constant per event, generated with an external tool the book doesn't specify in detail; collisions are a human process problem (nothing catches two events picking the same literal by accident) |
| `entt::type_hash<T>::value()` (already vendored via EnTT, ADR-0001) | Yes, if client/server run binaries built by the *same* compiler/version | **No** — the hash is derived from a stringified type name (`__PRETTY_FUNCTION__`/equivalent), which can differ between compiler vendors or major versions; a save/journal file read years later by a different toolchain isn't guaranteed to match | None |

Networking (client and server are, in practice, the same binary or built by the same CI, launched
fresh each session) can tolerate `entt::type_hash`'s compiler-dependence — a mismatched hash there
just means "incompatible build," which any multiplayer protocol needs a version handshake to catch
anyway. An **event journal is a stricter case**: it's a file that can outlive the compiler that
wrote it. Rather than maintaining two different ID schemes for the two use cases, this ADR proposes
one mechanism that satisfies the stricter requirement, so networking gets it for free:

**Decision: a `constexpr` FNV-1a hash of a human-authored, `constexpr` name string, computed at
compile time.** Stable forever, because both the algorithm and the input string are fixed by us —
nothing compiler-internal. Unlike the book's hand-picked hex literal, nobody manually assigns or
tracks a number; the ID is derived automatically from a name that's already self-documenting.

```cpp
// event_type_id.h (sketch)
constexpr uint32_t Fnv1aHash(std::string_view s) {
    uint32_t hash = 2166136261u;
    for (char c : s) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 16777619u;
    }
    return hash;
}
```

This ID is **only required for events that cross a boundary** (network or journal) — see §2. Local,
in-process pub/sub keeps using `std::type_index` exactly as ADR-0002 shipped it; nothing about this
changes for an event that never leaves the process.

### 2. Serialization contract — opt-in per event, not a mandatory base for every event

The book's `IEventData` is mandatory for *every* event, including ones that in practice never
leave the client (e.g. a purely cosmetic UI hover). Forcing every event through a virtual
serialization interface for the sake of the subset that actually needs it reintroduces the exact
boilerplate-per-event-type cost the original draft of this ADR objected to — just for a different
reason this time (multiplayer/journal support existing) rather than no reason at all.

**Decision: `ISerializableEvent` is implemented only by events that need to cross the network or
be written to the journal.** Purely local events stay plain structs, exactly as before.

```cpp
// Local-only event: unchanged from before, no interface, no ID needed.
struct EvtData_Camera_Shake {
    float intensity;
    float durationSeconds;
};

// Network- and journal-relevant event: opts into ISerializableEvent explicitly.
class ISerializableEvent {
public:
    virtual ~ISerializableEvent() = default;
    virtual uint32_t VTypeId() const = 0;
    virtual void VSerialize(ByteWriter &out) const = 0;
    virtual void VDeserialize(ByteReader &in, uint32_t recordVersion) = 0;
};

struct EvtData_Destroy_Actor : ISerializableEvent {
    static constexpr std::string_view kEventName = "EvtData_Destroy_Actor";
    static constexpr uint32_t kTypeId = Fnv1aHash(kEventName);   // compile-time, stable forever

    entt::entity id{entt::null};

    uint32_t VTypeId() const override { return kTypeId; }

    void VSerialize(ByteWriter &out) const override {
        out.WriteU32(static_cast<uint32_t>(id));
    }

    void VDeserialize(ByteReader &in, uint32_t recordVersion) override {
        id = static_cast<entt::entity>(in.ReadU32());   // recordVersion lets old journal records
                                                         // up-convert if the payload layout changes
    }
};
```

`EvtData_Destroy_Actor` still works with the local, unchanged `EventManager::Subscribe<T>`/
`Emit<T>`/`Queue<T>` exactly like any other event — `ISerializableEvent` is an *additional*
capability the struct opts into, not a replacement for how it participates in local dispatch.

`VGetTimeStamp()`/`BaseEventData` from the original book design is deliberately **not** part of
this interface — see §4: the journal captures timestamp as record metadata at the point of
recording, not as a field every serializable event must carry itself, so events that are networked
but never journaled don't pay for a field they don't use.

### 3. A registry, for the one case where virtual dispatch is genuinely required

Locally, `Subscribe<T>`/`Emit<T>` never need runtime type lookup — the compiler already knows `T`
at every call site. The receiving end of a network message or a journal replay is different: it
only has raw bytes plus a `uint32_t` type ID, and must reconstruct the right concrete C++ type
without knowing it at compile time. This is the one place in this design that legitimately needs a
runtime factory table — not a limitation of `std::type_index`, a genuine property of crossing a
serialization boundary at all, in any design.

```cpp
class EventTypeRegistry {
public:
    template <typename T>
    void Register() {
        factories_[T::kTypeId] = [] { return std::make_unique<T>(); };
    }

    std::unique_ptr<ISerializableEvent> Create(uint32_t typeId) const {
        auto it = factories_.find(typeId);
        return it != factories_.end() ? it->second() : nullptr;
    }

private:
    std::unordered_map<uint32_t, std::function<std::unique_ptr<ISerializableEvent>()>> factories_;
};
```

Registration happens once at startup for every serializable event type (`registry.Register<EvtData_Destroy_Actor>();`),
the same shape as `Subscribe` calls already are — not a monolithic enum, just an explicit call
per type that needs it.

### 4. Event journal (save-file versioning / replay)

A journal record is metadata (schema version, type ID, timestamp) plus the event's own serialized
payload:

```cpp
// One journal record on disk: [schemaVersion:u32][typeId:u32][timestampMs:u32][payloadSize:u32][payload bytes]
class EventJournal {
public:
    void Record(const ISerializableEvent &event, float timestampSeconds);

    // Reads every record back and re-`Queue`s the reconstructed event through the same
    // EventManager a live session would have used -- a subscriber can't tell a replayed event
    // from a live one, which is exactly the property a deterministic replay needs.
    void Replay(EventManager &events, const EventTypeRegistry &registry);

private:
    static constexpr uint32_t kCurrentSchemaVersion = 1;
};
```

- **Versioning**: each record stores the schema version it was written with (`kCurrentSchemaVersion`
  at write time). `VDeserialize(in, recordVersion)` receives that version so an event type can
  up-convert an old payload layout (e.g. a field added in version 2 defaults when reading a
  version-1 record) — this is the piece of "save file versionado" that the book's own
  serialization story doesn't cover in depth, since it targets same-session network sync, not
  data that has to remain readable across patches.
- **Replay re-enters through `Queue<T>`, not a separate code path** — a handler that reacts to
  `EvtData_Destroy_Actor` doesn't need to know or care whether the event came from live gameplay
  or `EventJournal::Replay`. This is the same "heart that decouples subsystems" property the
  original proposal asked to preserve: journaling and replay become just another producer/consumer
  of the existing `EventManager`, not a parallel system with its own dispatch rules.

### 5. `VTriggerEvent` (immediate) vs. `VQueueEvent` (deferred, double-buffered) — adopt the queued path as an addition

Unchanged from the original draft of this ADR. `Emit<T>` today iterates `handlers_[type]`
synchronously; nothing stops a handler from re-entering the same `EventManager` mid-dispatch —
e.g. a handler for `EvtData_Destroy_Actor` that itself calls `Subscribe`/`Emit` for the same type
while `handlers_[type]` is still being iterated, which can invalidate the iterator `Emit` is using.
Nothing has hit this yet only because nothing calls `Subscribe`/`Emit` at all yet — which makes now,
before real call sites exist, the right time to decide this.

```cpp
// Addition to EventManager (sketch).
template <typename T>
void Queue(T event) {
    pending_.push_back([this, event = std::move(event)] { Emit(event); });
}

// Called once per frame from Engine::Run(), the same place ProcessManager::Update() is already
// ticked (src/app/engine.cpp's TickAndUpdateDraw trampoline).
void DispatchQueued() {
    std::vector<std::function<void()>> active;
    std::swap(active, pending_);   // anything Queue()'d *during* this loop lands in the now-empty
    for (auto &dispatch : active) dispatch();   // pending_, not in `active` -- no re-entrant loop
}
```

- `Emit<T>` stays exactly as ADR-0002 shipped it — immediate, synchronous.
- `Queue<T>` becomes the default recommendation for cross-system events (matches the book's own
  usage pattern) and is also the natural point where a networking or journaling subsystem observes
  events (see §6) — queued events are already "this frame's batch," the same granularity a network
  tick or a journal record wants to work at.
- Still no time budget (`VTick(20ms)`) — ADR-0002's reasoning holds; the double-buffer swap solves
  a correctness problem (reentrancy), not a cost problem.

### 6. Wiring networking/journaling to `EventManager` — via ordinary `Subscribe`, not built into the core

`EventManager` itself stays free of any networking or file I/O dependency — no socket code, no
`ByteWriter`, no journal format baked into `event_manager.h`. Multiplayer sync and journaling are
just two more subscribers, wired explicitly per event type that needs them:

```cpp
// Only for event types that need multiplayer sync and/or journaling -- wiring is explicit per
// event type, not automatic for every event that happens to implement ISerializableEvent.
events.Subscribe<EvtData_Destroy_Actor>([&](const EvtData_Destroy_Actor &e) {
    journal.Record(e, GetTime());
    if (session.IsHost()) session.Broadcast(e);
});
```

This keeps `EventManager` a small, dependency-free core (matching ADR-0002's original framing of
it as "the heart" gluing subsystems together) while still making networking/journaling a first-class
concern *of the design*, not an afterthought bolted onto local-only dispatch later — the stable ID
(§1) and serialization contract (§2) exist now specifically so that retrofit never has to happen.

### 7. Lua scripting bridge — still not applicable

Unchanged: frame-3 has no scripting language integration at all (no Lua vendored/linked anywhere).
This piece of the original proposal has no target to attach to; revisit only if/when embedding a
scripting language is decided on its own merits.

## Decision

1. Extend `EventManager` with `Queue<T>`/`DispatchQueued()` (§5), ticked once per frame from
   `Engine::Run()` alongside `ProcessManager::Update()`. `Emit<T>` is unchanged.
2. Add `ISerializableEvent` (§2) as an **opt-in** interface — only event types that need to cross
   the network or be journaled implement it; purely local events remain plain structs using the
   existing `std::type_index`-based dispatch, unchanged.
3. Type identity for serializable events is a `constexpr` FNV-1a hash of a human-authored name
   string (§1) — not `std::type_index`, and not the book's hand-picked hex literal.
4. Add `EventTypeRegistry` (§3) as the factory table used only when reconstructing an event from
   raw bytes (network receive, journal replay).
5. Add `EventJournal` (§4) with a versioned record format (schema version, type ID, timestamp,
   payload), replaying through `EventManager::Queue<T>` so replayed events are indistinguishable
   from live ones to subscribers.
6. Networking/journaling attach to `EventManager` via ordinary `Subscribe` calls (§6) — no
   networking- or file-I/O-specific code lives inside `event_manager.h` itself.
7. Do not adopt Lua integration (§7) — no scripting layer exists to attach it to.

## Tradeoffs accepted

- More moving pieces than a single mandatory interface would be (`ISerializableEvent`,
  `EventTypeRegistry`, `EventJournal`, the FNV-1a helper) — accepted because each is paid for only
  by the events that actually need to cross a boundary; a purely local event (the common case
  today, since no networking/journal code exists yet) still costs exactly what it did in ADR-0002.
- FNV-1a over a 32-bit space has a theoretical collision risk as the number of serializable event
  types grows — mitigated by keeping the space small in practice (only network/journal-relevant
  events need an ID at all) and by a follow-up: a startup-time assertion/test in
  `EventTypeRegistry::Register<T>()` that fails loudly if two registered types hash to the same ID,
  rather than silently misrouting.
- No actual network transport (sockets, a networking library) is chosen here — this ADR only
  decides the event-level contract (stable ID + serialize/deserialize), not how bytes actually move
  between processes. That's a separate, larger decision left for a future ADR once multiplayer work
  actually starts.
- `VDeserialize` takes a `recordVersion` parameter every serializable event must be prepared to
  handle once the schema evolves — a real, ongoing maintenance cost of supporting long-lived save
  files (accepted because "save file versionado" was an explicit requirement, not a hypothetical).
- Two dispatch entry points (`Emit`/`Queue`) instead of one — same tradeoff as the original draft;
  each has a distinct, real use.
- Still no `Unsubscribe` — out of scope here, flagged as an open question below.

## Consequences / follow-ups

- Once any of `Queue<T>`/`DispatchQueued()`, `ISerializableEvent`, `EventTypeRegistry`, or
  `EventJournal` land in code, update `.claude/skills/engine-architecture` §1 and this ADR's Status
  to Accepted, per the precedent in ADR-0002's own "update once it lands" note.
- `Engine::Run()`'s `TickAndUpdateDraw` trampoline gains `Events().DispatchQueued()` alongside
  `Processes().Update(...)`.
- The actual network transport (sockets vs. a library, host/client vs. peer-to-peer topology, tick
  rate, reliability/ordering guarantees) is explicitly **not** decided here and needs its own ADR
  once multiplayer implementation actually starts — this ADR only guarantees the event layer won't
  need to be redesigned when that happens.
- `EventJournal`'s on-disk format (byte layout, endianness, compression) is sketched at the
  concept level only (§4) — a real implementation needs to pick concrete `ByteWriter`/`ByteReader`
  primitives, which don't exist in the codebase yet.
- First real serializable event type should also be the first test of whether per-event
  `VSerialize`/`VDeserialize` (hand-written, book-style) is worth it at frame-3's current scale, vs.
  a reflection/macro-based approach that generates them — hand-written is the simpler starting
  point and matches how `Subscribe`/`Emit` were introduced (concrete first, generalize only if
  repetition actually hurts).

## Open Questions

- **`Unsubscribe<T>`** — still undesigned; no code subscribes yet.
- **Should `Queue<T>` accept move-only event types?** Unchanged open question from the original
  draft.
- **Delta vs. full-state events for networking** — this ADR treats "serialize the event" as
  sending the whole event struct; real multiplayer engines often diff/compress state instead. Not
  decided here; revisit once there's real network traffic to measure.
- **Journal file rotation/size limits** — an unbounded append-only journal will grow forever for a
  long play session; not addressed here, worth deciding once `EventJournal` has a real
  implementation to test against.

## References

- *Game Coding Complete, 4th Edition* (McShaffry & Graham), Ch. 4 — `IEventData`, `BaseEventData`,
  GUID-based `EventType`, `IEventManager::VTriggerEvent`/`VQueueEvent`, the two-alternating-queue
  processing model, and the Lua `ScriptEvent` bridge.
- [ADR-0002](0002-event-manager-and-process-manager-game-loop.md) — the shipped `EventManager`
  (`std::type_index` dispatch, `Emit<T>`, no GUID/enum, no budget) this ADR builds on and extends.
- [ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md) — EnTT as a vendored dependency; `entt::type_hash`
  is discussed in §1 as a candidate mechanism, not adopted for journal-stability reasons.
- `.claude/skills/engine-architecture/SKILL.md` §1 — current `EventManager` documentation, to be
  updated once any piece of this ADR lands.
- `src/app/event_manager.h`, `src/app/engine.h`/`.cpp` — the current implementation and per-frame
  tick site this ADR's additions attach to.

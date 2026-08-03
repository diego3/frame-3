# 2. Event bus and process manager, ticked from `Engine::Run`

- Status: Accepted
- Date: 2026-08-03

## Context

*Game Coding Complete* Ch. 4 builds its main loop around three cooperating systems: capture input,
update game logic, present to the views — with an Event Manager (multicast by 32-bit GUID plus a
monolithic event-type enum, hand-rolled delegate objects) and a Process Manager (cooperative
multitasking for behavior that spans several frames) sitting in the middle of that loop. The book's
own `WinMain`/DirectX message pump is exactly what `Engine` (ADR-0001, Decision 2) already replaced
with raylib's `InitWindow`/`WindowShouldClose`. What that ADR left unaddressed was the two Ch. 4
systems themselves — `.claude/skills/engine-architecture` had design sketches for both, but neither
existed in code, and `Engine::Run()` called only the screen's fused update/draw callback each frame.

The book's Event Manager is a product of a pre-`std::function`, pre-templates-were-trusted-for-this
C++03 era; porting the GUID+enum mechanism literally would reintroduce exactly the "terrible
monolithic enumeration" problem the book itself was working around. The Process Manager, by
contrast, is genuinely not Windows/DirectX-specific in the book and ports close to as-is.

## Decision — `Engine` owns one `EventBus` and one `ProcessManager`, ticked from a shared trampoline

**`EventBus`** (`src/app/event_bus.h`, header-only): pub/sub keyed by `std::type_index`, handlers
are `std::function<void(const T&)>`. `Subscribe<T>`/`Emit<T>` dispatch synchronously, in the calling
frame — no queuing, no per-frame time budget (the book's `VTick(20ms)` existed to bound processing
under the GUID+enum dispatch cost; the typed dispatch here doesn't carry that cost). Adding a new
event kind is just defining a new struct, no shared enum to touch.

**`Process`/`ProcessManager`** (`src/app/process.h`, `src/app/process_manager.h`/`.cpp`): a
`Process` runs across frames via `Update(dt)` until it reports `Succeed()`/`Fail()`; `ProcessManager`
owns the list, calls `Update(dt)` on each once per frame, and drops the ones that finished
(`std::erase_if`). Ported close to the book's shape per the skill's guidance — this piece was never
Win32-specific.

`Engine` (`src/app/engine.h`/`.cpp`) owns one instance of each, alongside its existing
`entt::registry`, exposed via `Events()`/`Processes()`. `Engine::Run()` ticks the process manager
with `GetFrameTime()` once per frame, immediately before the screen's update/draw callback runs.
Both loop paths — the desktop `while (!WindowShouldClose())` loop and the `PLATFORM_WEB`
`emscripten_set_main_loop` callback — go through the same `TickAndUpdateDraw` trampoline in
`engine.cpp`, so the tick isn't duplicated (or allowed to drift) per platform. The trampoline needs
a plain function pointer for `emscripten_set_main_loop`, which can't capture `this`; a pair of file-
local globals (`g_runningEngine`, `g_updateAndDraw`) bridge that, valid because `Engine::Run()` is
only ever called once, from `main()`.

The event bus is not ticked or budgeted at all — `Emit` runs handlers immediately, so there's
nothing for `Engine::Run()` to drive on its behalf.

### Tradeoffs accepted

- No time-budgeted event processing (the book's `VTick(20)`). Accepted because the cost that budget
  was defending against (GUID lookup + enum dispatch over a possibly-large per-frame event queue)
  doesn't exist with immediate `std::type_index`-keyed dispatch; revisit only if a real workload
  shows event storms causing frame-time spikes.
- Two file-local globals in `engine.cpp` to work around `emscripten_set_main_loop`'s bare
  function-pointer requirement, rather than a cleaner captured-lambda API — accepted because
  `Engine` is a de facto singleton in practice (one instance, constructed in `main()`, `Run()`
  called once), and the alternative (a heavier callback-registration API on `Engine` itself) isn't
  justified until something other than raylib's own loop-driving needs to hook in.
- Neither system has a caller yet: nothing subscribes to the event bus or attaches a `Process`,
  because no gameplay code exists to need either yet. No event struct types and no concrete
  `Process` subclass were pre-built speculatively — both get added the first time real gameplay code
  needs to announce something or run a multi-frame effect.

## Consequences / follow-ups

- `.claude/skills/engine-architecture/SKILL.md` §§1-2 updated to point at these real files instead
  of only sketches, per the skill's own "update once it lands" note.
- The resource cache (Ch. 8) and real ECS components (Ch. 6-7) remain unbuilt — see ADR-0001 and
  the skill's Open Questions; this ADR only covers the event bus and process manager.
- First real usage of `EventBus`/`ProcessManager` should also be the first test of whether
  `Events()`/`Processes()` accessed straight off `Engine` (vs. threaded through as explicit
  parameters to game/view code) stays comfortable once `screens.h`'s C screen functions need to
  reach them — those functions currently only see `currentScreen`/`font`/`music`/`fxCoin` via
  `extern` (ADR-0001, Decision 2's tradeoffs), and neither `EventBus` nor `ProcessManager` are
  C-compatible types.

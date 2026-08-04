# 6. Test framework: doctest for `EventManager`/`ProcessManager` unit tests

- Status: Accepted
- Date: 2026-08-03

## Context

ADR-0003 landed `EventManager` and `ProcessManager`, ticked once per frame from `Engine::Run`'s
shared `TickAndUpdateDraw` trampoline. Neither has any test coverage today. Both are also, by
construction, pure C++ logic with no dependency on raylib, a window, or a GPU context:
`EventManager::Subscribe`/`Emit` key off `std::type_index` and `std::function`, and
`ProcessManager::Attach`/`Update` just owns a `vector<unique_ptr<Process>>` and drives each one's
`Update(dt)`/`IsDead()`. That combination — real logic, zero platform dependency — makes them the
first genuinely test-shaped code in the project, worth deciding a test framework for rather than
leaving to accumulate ad hoc.

Whatever gets picked has to fit the same constraint ADR-0001's Decision 3 already established when
EnTT was added: the project consolidated onto a single Makefile-based build on purpose, and rejected
reintroducing CMake even though CMake has native multi-language target support, specifically to
avoid maintaining two build systems. A test framework that assumes a CMake/FetchContent workflow
would reopen that same tradeoff just for tests.

This decision was also prompted by a direct question worth recording: the engine is expected to
grow quickly into much bigger demos (a GTA-style "vigilante" game, a Quake-style FPS), which will
add real subsystems (physics, AI, resource loading, maybe networking) — what does it cost to walk
back a framework choice made now, once test needs are less "does this dispatch call every handler"
and more "mock a raylib call" or "run the same test across a dozen weapon configs"?

## Options considered

| | **doctest** | **Catch2 v2** | **GoogleTest** | **Hand-rolled `assert()` binary** |
|---|---|---|---|---|
| Vendoring shape | Single header, MIT — clone the repo, point an include path at it. Same shape as EnTT (ADR-0001). | Single header (v2 only — v3 dropped this and requires building a library). | Distributed to be built as a library, almost always via CMake/FetchContent in every guide and example. | None — no new dependency at all. |
| Fits the Makefile-only constraint | Yes, no build step, mirrors EnTT exactly. | Yes, same shape as doctest. | No — would need either hand-writing a Makefile rule to compile gtest's own sources (fighting its intended build path) or reopening the CMake question ADR-0001 already closed. | Trivially yes. |
| Compile time | Fastest of the three — doctest's own explicit design goal is to be a faster-compiling alternative to Catch2. | Slower than doctest at scale; acceptable at today's project size. | N/A (compiled once as a library, not per test TU) but adds the library build itself. | Fastest possible — no framework to parse. |
| Macro surface | Minimal: `TEST_CASE`, `SUBCASE`, `CHECK`/`REQUIRE` with expression decomposition. | Nearly identical to doctest by lineage: `TEST_CASE`, `SECTION`, `CHECK`/`REQUIRE`. | Different shape: `TEST`/`TEST_F`, `EXPECT_EQ`/`ASSERT_EQ` typed comparisons, class-based fixtures. | None — plain functions and `assert()`. |
| Mocking | None built in. | None built in. | `gmock` included — real mocking support. | None. |
| Parameterized/data-driven tests | `DOCTEST_VALUE_PARAMETERIZED_DATA` — usable, clunkier than gtest's `TEST_P`. | `GENERATE()` — ergonomic, closer to property-based testing. | `TEST_P` with value/type parameterization — mature. | Hand-roll a loop. |
| Test discovery / readable failures / run-single-test | Yes. | Yes. | Yes. | No — you get whatever `assert()`'s abort message gives you. |

## Migration cost, given the engine is expected to grow

The blast radius of this choice is smaller than it looks, for a structural reason: tests only ever
depend on the *public interface* of the system under test (`EventManager::Subscribe`/`Emit`,
`ProcessManager::Attach`/`Update`) and include the test framework's header from `*_test.cpp` files
only. No production header — `event_manager.h`, `process_manager.h`, `engine.h`, or anything a
future physics/AI/resource system adds — ever includes a test framework header. Swapping frameworks
later is scoped to however many test files exist at that point; it does not grow with the engine
itself.

Within that bound, the two possible migrations cost differently:

- **doctest → Catch2**: close to mechanical. doctest's public API was deliberately modeled on
  Catch2's (that's the whole premise of doctest existing — "a faster-compiling, header-only
  Catch2-alike"), so `TEST_CASE`/`CHECK`/`REQUIRE` carry over as-is and `SUBCASE` renames to
  `SECTION`.
- **doctest → GoogleTest**: a real rewrite, but still bounded to test bodies. Assertion style changes
  (`CHECK(a == b)`'s expression decomposition vs. `EXPECT_EQ(a, b)`'s typed comparison) and the
  fixture model changes (`SUBCASE` nesting vs. `TEST_F` class fixtures) — every existing test's body
  needs touching, not just its includes. Bringing GoogleTest in at all also means finally taking on
  the CMake/FetchContent question ADR-0001 avoided, or hand-maintaining a Makefile build of gtest's
  own sources.

The gap that actually matters for the FPS/physics-scale future — no mocking — is accepted at both
doctest and Catch2 today, since neither has it built in. The mitigation isn't "pick GoogleTest now
just for gmock": it's that a later need for heavier integration-style tests (faking a raylib call, a
resource cache, a physics query) is better met by adding a second, purpose-built test target/binary
for that class of test when it's actually needed — same "revisit once there's a concrete trigger"
pattern ADR-0004 used for the resource cache's deferred pieces — rather than migrating the
already-working unit suite off doctest pre-emptively.

## Decision

**Adopt doctest** for the engine's unit test suite. Vendor it the same way EnTT already is: a git
clone (`vendor/doctest` locally via `build.sh`, a sibling checkout in CI mirroring the existing
`entt` cache step) exposing a single header, no library to build. Tests live under `src/tests/`, one
`*_test.cpp` per system under test, compiled by a new Makefile target into a standalone test binary
that CI runs as a step after the main build — a failing test blocks the same way a compile warning
already does under `ci_sanity.yml`'s `-Werror` gate.

## Consequences / follow-ups

- `build.sh` and `src/Makefile` gain doctest vendoring and a `tests` build target (this same change).
- `ci_sanity.yml` gains a step that builds and runs the test binary after the main build.
- If/when a class of test needs mocking, fixtures beyond `SUBCASE` nesting, or parameterized data
  sets doctest handles awkwardly, treat that as its own decision naming the concrete trigger — a new
  test target alongside doctest, not a framework migration — consistent with how ADR-0004 sequenced
  the resource cache's deferred pieces.
- No CI/build change needed for `EventManager`/`ProcessManager` themselves; this ADR only adds
  coverage for code ADR-0003 already shipped.

## References

- [ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md) — Decision 3: Makefile as the project's one
  build system, the constraint this decision had to fit.
- [ADR-0003](0003-event-manager-and-process-manager-game-loop.md) — `EventManager` and
  `ProcessManager`, the systems this decision adds coverage for.
- [ADR-0004](0004-resource-cache-thin-vs-full-book-rescache.md) — precedent for deferring a heavier
  piece (there: LRU/ZIP bundling/loader registry; here: mocking/fixtures) until a concrete trigger
  justifies it.
- [doctest](https://github.com/doctest/doctest) — single-header C++ testing framework.

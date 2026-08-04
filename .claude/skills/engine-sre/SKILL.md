---
name: engine-sre
description: SRE guidance for frame-3 specifically — CI build/test gates, frame budget as an SLO, an accepted-but-unquantified error budget, observability via the F3 debug overlay, and why SLA/tracing/Terraform don't fit yet. Use this skill whenever the user brings up SRE, reliability, SLI/SLO/SLA, error budgets, observability, monitoring, alerting, tracing, incident response, or infrastructure-as-code for this project, or asks "what SRE thing could we add next" / "is X worth doing here yet". Also use it before adding a CI workflow, a build/test gate, a runtime metric, a log, or any infrastructure (hosting, Terraform) — check here first for what already exists and what's already been deliberately deferred, so the same ground doesn't get re-litigated from scratch. This is a running log of decisions made in conversation, not upstream doctrine — update it whenever a new SRE-flavored decision is made (accepted, deferred, or reversed), the same way engine-architecture tracks engine decisions.
---

# SRE for frame-3

frame-3 is a single-player, local, from-scratch C++/raylib/EnTT game (desktop + WebAssembly
builds) with no server, no customers, and one developer. Classic SRE was written for teams
running always-on, multi-tenant services — most of the vocabulary (SLA, on-call, tracing,
capacity planning) doesn't map onto that directly. This skill exists to keep straight which
pieces *do* translate today, which are already implemented, and which are explicitly deferred (and
to what trigger) — so each one gets decided once, not re-argued from zero in a future
conversation.

**Update this skill whenever a new SRE-flavored decision lands** — a new metric, a new CI gate, or
a deferred piece getting pulled forward — the same "keep current state accurate" convention
`engine-architecture` uses.

## When to Use This Skill

- The user mentions SRE, reliability, SLI, SLO, SLA, error budget, observability, monitoring,
  alerting, tracing, incident response, postmortems, or infrastructure-as-code (Terraform, etc.)
- Before adding a new CI workflow or build/test gate — check `ci_sanity.yml` and the `build_*.yml`
  release workflows first, so a new one doesn't duplicate what's already gating pushes/PRs
- Before adding a runtime metric, counter, or log — check whether it's a natural extension of the
  frame budget SLO or the debug overlay below, rather than a new, disconnected mechanism
- Before reaching for infrastructure tooling (hosting, Terraform, Docker, k8s) for anything —
  check [ADR-0007](../../../docs/adr/0007-terraform-gated-on-authoritative-server.md)'s reasoning
  for why that's gated on a trigger that hasn't happened yet
- The user asks "what SRE thing could we add next given what we already have"

## Current state — what's already implemented

- **CI build gate** (`.github/workflows/ci_sanity.yml`): runs on every push and pull request,
  caches raylib/EnTT/doctest, builds with `-Werror` (one pre-existing warning downgraded via
  `-Wno-error=unused-function`, not silenced project-wide), then builds and runs the doctest unit
  suite under the same `-Werror` gate. This is the project's fail-fast/reliability floor — a build
  or test regression is caught in a couple of minutes, before it reaches `main`.
- **Release build reproducibility** (`build_linux.yml`, `build_macos.yml`, `build_windows.yml`,
  `build_webassembly.yml`): raylib and EnTT checkouts are pinned to specific tags (`6.0`,
  `v4.0.0`), not tracking a moving branch — an upstream change can't silently break a release build
  with no change on our side.
- **Unit test suite** (`src/tests/`, doctest — see
  [ADR-0006](../../../docs/adr/0006-doctest-for-unit-tests.md)): covers `EventManager` and
  `ProcessManager`, the two systems in the codebase that are pure logic with no raylib/window
  dependency. `test.sh` / `make -C src tests` run it locally; CI runs it on every push/PR.
- **Frame budget SLO** (`src/app/engine.cpp`, `TickAndUpdateDraw`): `kFrameBudgetMs = 1000/60`
  (~16.67ms), matching the existing `SetTargetFPS(60)`. Every frame over budget logs a
  `TraceLog(LOG_WARNING, ...)` via raylib's own logger — no new dependency. **Unthrottled by
  design for now**: a real stall logs one warning per frame, which is spammy but was accepted
  deliberately rather than adding throttling ahead of it mattering in practice.
- **Observability via the F3 debug overlay** (`src/app/debug_overlay.h`/`.cpp`): toggled with F3,
  Linux desktop only (`#if defined(__linux__) && !defined(PLATFORM_WEB)`; a no-op elsewhere).
  Samples `/proc/self/status` and `/proc/self/stat` at most every 0.5s (real syscall I/O, not done
  every frame) and draws FPS, CPU%, RSS, VMem, thread count, and open FD count on screen. This
  answers "why is the frame budget blown" (CPU pegged? RSS growing? leaking file handles?) that
  FPS alone can't. **Ephemeral by design so far**: nothing here is logged or persisted — it only
  exists on screen while F3 is held on, live.

## Core concepts, mapped onto what actually exists here

### SLI (Service Level Indicator) — have two already, no aggregation yet

Frame time (feeding the frame budget SLO) and the debug overlay's CPU%/RSS/VMem/threads/FDs are
real indicators already being sampled. Neither is aggregated or persisted past the current
frame/sample — see "Not yet done" below for the natural next step.

### SLO (Service Level Objective) — have one, a second is sitting unused in the Makefile

Frame budget (~16.67ms/frame) is the one SLO actually checked at runtime today. A second one is
effectively already *declared*, just never checked: `src/Makefile`'s `BUILD_WEB_HEAP_SIZE ?=
128MB` is a memory ceiling for the WebAssembly build, but nothing at runtime compares actual usage
against it.

### SLA (Service Level Agreement) — doesn't apply, on purpose

No customers, no contract, no support commitment — there's no counterparty to have an agreement
with. Don't force this to fit; revisit only if frame-3 ever ships with a real support/uptime
commitment attached (a different project phase than exists today).

### Error budget — the SLO exists, the budget doesn't yet

An error budget needs an aggregate ("what fraction of frames blew the budget this session"), and
today the frame budget SLO only logs each individual violation with no counting/threshold. Nothing
currently answers "was this session fine (a couple of blips) or a real regression (blown budget
constantly)". This is the most-called-out "not yet done" item below.

### Observability — real but ephemeral

The F3 overlay is genuine observability, just live-only: nothing it samples is written anywhere
that survives the overlay being toggled off or the process exiting. `Engine::Shutdown()`
(`src/app/engine.cpp`) already exists as a natural, already-called-exactly-once hook where a
session summary could be logged — nothing does that yet.

### Tracing — doesn't apply yet, and won't until there's a second process

Tracing (in the standard sense: spans correlated across a request/process boundary) needs more
than one process to trace across. frame-3 is one process, locally, today. The closest useful
in-process analog — per-frame phase timing (how long input/update/draw or a given system's
`Update` took) — isn't built either; it would be profiling/instrumentation, not tracing proper.
Real tracing becomes relevant once
[ADR-0005](../../../docs/adr/0005-event-manager-queued-dispatch-idata-lua-proposal.md)'s
authoritative-server design is actually implemented and there's a client + server to correlate
events across — not before.

### Infrastructure as code (Terraform) — decided, deferred

[ADR-0007](../../../docs/adr/0007-terraform-gated-on-authoritative-server.md) covers this in full:
not adopted now (no infrastructure exists to provision — no server, no cloud resource), explicitly
gated on a working authoritative multiplayer server binary needing real hosting. When that trigger
is met, that ADR also pre-names the hosting shape to lean toward (a small VPS provider, not
AWS/GCP, matching this project's actual scale) so that choice isn't made from scratch under
pressure later.

## Decisions made

- **`-Werror` on both the main build and the test suite, with per-warning-type exceptions rather
  than a blanket downgrade.** `ci_sanity.yml`'s one exception (`-Wno-error=unused-function`, for a
  single pre-existing dead function) was scoped to that warning class specifically, not to
  "warnings are fine" generally — keeps the gate meaningful for every future warning class.
- **Every heavier reliability mechanism gets gated on a concrete, named trigger, not built ahead of
  need.** Same pattern ADR-0004 (resource cache pieces), ADR-0006 (mocking/parameterized tests),
  and ADR-0007 (Terraform) all used independently for their own deferred pieces — worth stating
  once, here, as the general principle instead of re-deriving it per subsystem.
- **The frame budget SLO logs unthrottled, on purpose, for now.** A real stall spams one warning
  per frame; accepted deliberately rather than adding throttling speculatively before it's ever
  been a real annoyance.

## Not yet done — concrete, low-effort next steps

Roughly in the order they were identified as worth doing:

1. **Session summary log at `Engine::Shutdown()`** — average/max frame time, % of frames over
   budget, peak RSS. Turns the F3 overlay's live-only observability into something reviewable after
   a playtest or crash, and the "% of frames over budget" figure is also the missing piece needed
   to state an actual error budget (see below) rather than just an SLO.
2. **Error budget for the frame budget SLO** — once frame-over-budget counting exists (from #1),
   pick a threshold (e.g. "more than 1% of frames over budget this session = investigate") instead
   of leaving every violation as an equally-weighted, un-triaged warning.
3. **Runtime check against the WebAssembly heap SLO** — `BUILD_WEB_HEAP_SIZE`'s 128MB is currently
   just a build-time ceiling; nothing compares live usage against it. Likely lands as part of
   porting the debug-overlay/session-summary work to `PLATFORM_WEB` (today Linux-desktop-only,
   since it reads `/proc/self`, which doesn't exist there).
4. **Per-frame phase timing** (input/update/draw, or per-system `Update` cost) — the in-process
   analog of tracing described above; would feed either the debug overlay or the session summary.
   Worth doing before real cross-process tracing is even relevant, since it's useful for FPS
   debugging on its own.

## Related

- [ADR-0006](../../../docs/adr/0006-doctest-for-unit-tests.md) — the test-framework decision behind
  the CI test gate.
- [ADR-0007](../../../docs/adr/0007-terraform-gated-on-authoritative-server.md) — the
  infrastructure-as-code decision, and the fullest existing writeup of the "gate on a concrete
  trigger" reasoning this skill leans on repeatedly.
- [ADR-0005](../../../docs/adr/0005-event-manager-queued-dispatch-idata-lua-proposal.md) — the
  authoritative-server-adjacent design that both ADR-0007 and this skill's tracing section are
  gated on.
- `engine-architecture` — the sibling skill for non-reliability engine design (event manager,
  process manager, ECS, resource cache); this skill assumes those systems' shape as given rather
  than re-explaining them.

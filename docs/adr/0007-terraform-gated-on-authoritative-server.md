# 7. Infrastructure as code (Terraform), gated on an authoritative multiplayer server existing

- Status: Proposed
- Date: 2026-08-03

## Context

This ADR records a question asked directly, in two parts: does Terraform fit frame-3 today, and
does that answer change once there's an authoritative multiplayer server?

**Today: no.** Terraform provisions and manages infrastructure — compute, networking, databases,
DNS. frame-3 has none of that. It's a local desktop/web game: `build.sh` compiles a binary, GitHub
Actions builds and tests it (`ci_sanity.yml`, `build_*.yml`), and the output either runs on the
player's own machine or gets served as static WebAssembly. There is no server process, no cloud
resource, nothing running anywhere on frame-3's behalf. The one theoretical fit — the GitHub
Terraform provider, to version the repo's own settings (branch protection, Actions secrets,
webhooks) as code instead of clicking through the UI — is more ceremony than value for a solo repo
with a handful of settings, and isn't what's being asked here.

**Once there's an authoritative server: yes, genuinely.** [ADR-0005](0005-event-manager-queued-dispatch-idata-lua-proposal.md)
already designs the piece that makes multiplayer possible — a stable, compiler-independent wire
type ID (`Fnv1aHash` over a constexpr name string) and an opt-in `ISerializableEvent` contract, so
an event can cross a process boundary. That ADR is a design for `EventManager`, though; no server
binary exists, nothing is deployed, and there's nothing to provision yet. The moment that changes —
a real authoritative server binary that needs to run somewhere reachable by actual players — is the
moment real infrastructure enters the picture: compute to host the process, an open game port (and
TLS if anything sits in front of it, e.g. a matchmaking/lobby HTTP endpoint), and possibly durable
storage if ADR-0005's event-journal idea lands as more than local disk. That's exactly the class of
problem Terraform exists for: reproducible, versioned provisioning instead of manually clicking
through a cloud console and drifting out of sync with what's actually running.

## What Terraform would provision, once the trigger is met

- **Compute** to run the authoritative server binary — a VM or container, sized for whatever the
  server actually needs (starts small; this is a from-scratch project, not a live-service launch).
- **Networking** — the game port open to players, plus TLS if any HTTP surface (lobby, matchmaking,
  patch distribution) sits alongside the raw game socket.
- **Persistence**, only if ADR-0005's event journal grows past "write to local disk" into something
  that needs to survive the server instance being recreated.
- **Multiple instances/regions**, only if and when matchmaking or player count makes a single
  always-on box insufficient — not assumed from the start.

## Options considered for *how* (naming the shape now, not deciding it yet)

| | **Terraform + a small VPS provider** (Hetzner, Fly.io, DigitalOcean) | **Terraform + AWS/GCP** | **No IaC — provision by hand** |
|---|---|---|---|
| Fit for a single authoritative server | Good — matches the actual scale of a from-scratch project's first multiplayer server, same "don't build for a scale that doesn't exist yet" framing [ADR-0004](0004-resource-cache-thin-vs-full-book-rescache.md) and [ADR-0006](0006-doctest-for-unit-tests.md) already used for other systems. | Overbuilt at first — managed DBs, autoscaling groups, global regions solve problems a single game server doesn't have yet. | Fine for exactly one always-on box, until it needs to be rebuilt (disaster recovery) or a second environment (staging) appears — that's usually the point IaC starts paying for itself. |
| Cost | Low — VPS pricing, no premium for managed cloud services not being used. | Higher baseline even at minimal scale. | No tooling cost, but no reproducibility either. |
| Room to grow | Real, if it happens to be needed — most of these providers also offer managed Postgres/regions when the day comes. | Most room to grow, at a cost paid from day one. | None — every recreation is manual and undocumented. |

No decision between these is made here — naming them now so the choice isn't made from scratch
under deadline pressure once the trigger is actually met.

## Decision

**Not adopted now.** Explicitly deferred, same "gated on a concrete trigger" pattern
[ADR-0004](0004-resource-cache-thin-vs-full-book-rescache.md) and
[ADR-0006](0006-doctest-for-unit-tests.md) already used for their own deferred pieces. The trigger:
a working authoritative server binary — built on top of what ADR-0005 designs, once that design is
actually implemented — that needs to run somewhere reachable by real players. At that point, pick a
hosting provider sized to the project's actual scale (leaning toward the small-VPS shape above,
given how every other infrastructure decision in this project has been sized so far) and write the
Terraform configuration as part of that same change, not ahead of it.

## Consequences / follow-ups

- No code or infrastructure changes from this ADR — it exists to record the reasoning, not to ship
  anything.
- ADR-0005 needs to move from Proposed to actually implemented before this ADR's trigger can be met
  at all; there's no authoritative server to host without it.
- When the trigger is met, amend or supersede this ADR with the real provider decision and the
  actual Terraform configuration, rather than treating "we chose Terraform" as the whole decision.

## References

- [ADR-0005](0005-event-manager-queued-dispatch-idata-lua-proposal.md) — the networking-capable
  event design an authoritative server would be built on; not yet implemented.
- [ADR-0004](0004-resource-cache-thin-vs-full-book-rescache.md) and
  [ADR-0006](0006-doctest-for-unit-tests.md) — precedent for gating a heavier piece of machinery on
  a concrete trigger instead of building ahead of need.

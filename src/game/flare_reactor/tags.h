// Marker (tag) components for the flare_reactor experiment (docs/rfc/0001-flare-reactor-pipeline-
// experiment.md). Empty structs used only for registry.view<Tag>() lookups -- resolves the same
// "which entity is the player" ambiguity game/sandbox/screen_gameplay.cpp's own comment already
// flags ("first entity in the registry stands in for the player, until a real PlayerTag exists"),
// now that a level has more than one entity and that heuristic actually breaks.
//
// No ReactorTag here (Phase 1 had one) -- Phase 3 gave the reactor a real component (reactor.h's
// Reactor), and a marker saying "this entity is a reactor" alongside a component that already
// only reactors get is redundant. SentinelTag stays for now; it'll get the same treatment once
// Phase 6 gives the sentinel its own real component (SentinelAI).
#ifndef FLARE_REACTOR_TAGS_H
#define FLARE_REACTOR_TAGS_H

struct PlayerTag {};
struct SentinelTag {};

#endif // FLARE_REACTOR_TAGS_H

// Marker (tag) components for the flare_reactor experiment (docs/rfc/0001-flare-reactor-pipeline-
// experiment.md). Empty structs used only for registry.view<Tag>() lookups -- resolves the same
// "which entity is the player" ambiguity game/sandbox/screen_gameplay.cpp's own comment already
// flags ("first entity in the registry stands in for the player, until a real PlayerTag exists"),
// now that a level has more than one entity and that heuristic actually breaks.
//
// No ReactorTag or SentinelTag here (Phase 1 had both) -- Phase 3 gave the reactor a real
// component (reactor.h's Reactor) and Phase 6 gave the sentinel one too (sentinel_ai.h's
// SentinelAI); a marker saying "this entity is a reactor/sentinel" alongside a component that
// already only that kind of entity gets is redundant. PlayerTag stays -- the player has no other
// component that would uniquely identify it the same way.
#ifndef FLARE_REACTOR_TAGS_H
#define FLARE_REACTOR_TAGS_H

struct PlayerTag {};

#endif // FLARE_REACTOR_TAGS_H

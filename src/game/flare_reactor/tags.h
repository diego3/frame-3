// Marker (tag) components for the flare_reactor experiment (docs/rfc/0001-flare-reactor-pipeline-
// experiment.md). Empty structs used only for registry.view<Tag>() lookups -- resolves the same
// "which entity is the player" ambiguity game/sandbox/screen_gameplay.cpp's own comment already
// flags ("first entity in the registry stands in for the player, until a real PlayerTag exists"),
// now that a level has more than one entity and that heuristic actually breaks.
#ifndef FLARE_REACTOR_TAGS_H
#define FLARE_REACTOR_TAGS_H

struct PlayerTag {};
struct ReactorTag {};
struct SentinelTag {};

#endif // FLARE_REACTOR_TAGS_H

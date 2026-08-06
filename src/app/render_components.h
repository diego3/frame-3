// First real "how does an entity look" render component (ADR-0010's own Open Questions flagged
// render component design as undecided). Scoped to exactly what docs/adr/0017's game/camera_fps
// towers need: an axis-aligned box with a size and a solid color, drawn at its WorldTransform.
// Game-agnostic (any game could plausibly want "a box with a size and a color"), so it lives here
// rather than under a game/<game-id>/ tree -- but not yet applied to game/sandbox's own
// GameplayScene (still hardcodes 1x1x1 MAROON wireframe cubes for every WorldTransform entity);
// revisit that if/when it also wants entity-driven appearance, not guessed ahead of a second real
// need the way HumanViewBase was (see docs/adr/0017).
#ifndef RENDER_COMPONENTS_H
#define RENDER_COMPONENTS_H

#include <raylib.h>

struct BoxRenderable {
    Vector3 size{1.0f, 1.0f, 1.0f};
    Color color{WHITE};
};

#endif // RENDER_COMPONENTS_H

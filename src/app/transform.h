// Local/world transform components for entity hierarchy (Ch. 9-10, modernized -- see
// docs/adr/0002). LocalTransform is authored/edited data, relative to an entity's parent (or to
// the world, for a root entity with no Relationship/parent -- see hierarchy.h). WorldTransform is
// derived, computed once per frame by PropagateTransforms (hierarchy.h); nothing else should
// write to it directly.
#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <raylib.h>
#include <raymath.h>

struct LocalTransform {
    Vector3 position{0.0f, 0.0f, 0.0f};
    Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};   // identity
    Vector3 scale{1.0f, 1.0f, 1.0f};
};

struct WorldTransform {
    Matrix matrix = MatrixIdentity();
};

#endif // TRANSFORM_H

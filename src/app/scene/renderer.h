// The Renderer row of ADR-0019's Mesh/RenderMaterial/Renderer sketch: given a Mesh and a
// RenderMaterial (material.h), actually turn it into pixels. Two free functions, not a class --
// nothing here needs instance state, same "plain data plus free functions" shape
// transform.h/hierarchy.h/renderable.h already use in this directory.
//
// app/scene/renderable.h's DrawRenderables (solid Box/Sphere Renderables) and its Model submesh
// loop both go through DrawWithMaterial below -- the same bind step either way, resolving ADR-0019's
// own "does a Model pass through the same Renderer as primitives, or stay a separate path" open
// question: yes, same Renderer, one submesh at a time (raylib's own DrawModelEx already draws a
// Model as one DrawMesh call per submesh internally -- vendor/raylib/src/rmodels.c -- this just
// routes that same loop through BindMaterial instead of raylib's inline bind).
#ifndef RENDERER_H
#define RENDERER_H

#include <raylib.h>

#include "app/scene/material.h"

// Pushes `material`'s extras onto its own shader. Called every draw, not once at material-creation
// time -- a RenderMaterial's raylibMaterial fields (diffuse tint, in particular) are already
// mutated per-entity/per-frame today (BeaconPulseProcess's gray-to-RED lerp), so extras are
// re-pushed on the same cadence for consistency, even though today's only extra (rim glow) happens
// to be static per docs/flare_reactor/lighting.cpp. Cheap relative to the draw call itself at this
// project's scale -- same tradeoff ApplyExtras's own comment already accepts.
//
// Named BindMaterial, not the bare `Bind` ADR-0019's own sketch used -- a one-word global function
// name is too easy to collide with unrelated code later; every other draw-adjacent free function in
// this directory (DrawRenderables, DrawBoxRenderables) already pairs a verb with a noun.
inline void BindMaterial(const RenderMaterial &material) { ApplyExtras(material.shader, material.extras); }

// BindMaterial then DrawMesh, with material.shader forced onto the raylib Material actually passed
// to DrawMesh (so a RenderMaterial's raylibMaterial.shader field never has to be kept in sync by
// hand at every call site -- material.shader is the one source of truth).
inline void DrawWithMaterial(const Mesh &mesh, const RenderMaterial &material, Matrix transform) {
    BindMaterial(material);
    ::Material rl = material.raylibMaterial;
    rl.shader = material.shader;
    DrawMesh(mesh, rl, transform);
}

#endif // RENDERER_H

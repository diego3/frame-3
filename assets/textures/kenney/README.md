# Kenney texture packs (dev/test scenarios)

Three packs by [Kenney](https://kenney.nl), moved here from `assets/new-to-organize/` -- see
`game/dev_arena` (`src/game/dev_arena/`) for the first real consumer. Each subdirectory keeps its
own shipped `License.txt`/`Preview.png` unmodified.

- `development-essentials/` -- checkerboard, UV texture, gradients, solid-color pixels (1x1/4x4),
  a default normal map, Perlin noise. `Checkerboard/checkerboard.png` is `dev_arena.mat.yaml`'s
  diffuse texture (`assets/materials/dev_arena.mat.yaml`).
- `prototype-textures/` -- numbered grid textures in six color variants (Red/Purple/Orange/Light/
  Green/Dark). Not consumed yet -- kept for a future per-entity texture once one entity can carry
  its own diffuse map (today `Renderable`'s solid boxes only take a shared material per draw call,
  see `app/scene/renderable.h`).
- `pattern-pack/` -- assorted tileable patterns (`Default`/`Double` variants). Not consumed yet.

All three: **License: CC0** (`http://creativecommons.org/publicdomain/zero/1.0/`) -- free for
personal, educational, and commercial use; crediting Kenney/kenney.nl is appreciated, not required.

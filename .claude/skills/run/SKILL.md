---
name: run
description: How to build and run frame-3's game modules locally (vendor lib paths, GAME= selection) — and the project's visual-validation convention, don't launch Xvfb or take screenshots to self-check a graphics/UI change. Use this whenever asked to build/run/"see a change working" in flare_reactor, sandbox, or camera_fps, and especially before reaching for Xvfb/screenshot tooling to confirm a rendering change looks right — check here first, that step belongs to the user.
---

# Running frame-3 locally

## Build and run: use build.sh/run.sh, never call `make` directly

**Learned the hard way (2026-08-08):** calling `make GAME=... -j...` directly (bypassing
`build.sh`) relinks `build/desktop/raylib_game` but does **not** refresh `build/desktop/<GAME>` (a
separate `cp`, done by `build.sh`, not the Makefile — see the Makefile's own comment on why: it
must never touch what the release CI workflows expect). `run.sh` launches the `<GAME>`-named copy,
so a direct `make` call leaves it stale and silently running old code with no error. Always go
through the wrapper scripts at the repo root instead:

```sh
GAME=flare_reactor ./build.sh   # builds + refreshes build/desktop/flare_reactor + a versioned copy
./run.sh                        # launches whichever GAME was last built (reads .built_game)
# or, combined:
GAME=flare_reactor ./build-run.sh
```

`GAME` selects which `src/game/<GAME>/` module gets built (`sandbox` is the default if unset).
`build.sh` already points `RAYLIB_PATH`/`ENTT_PATH`/`MINI_YAML_PATH` at this repo's own `vendor/`
checkouts — no need to pass them manually (the Makefile's own defaults are Windows-style
placeholders, but that's `build.sh`'s problem to work around, not something to redo by hand).

Every `build.sh` run also produces a versioned, identifiable copy —
`build/desktop/<GAME>-v<VERSION>-<git-sha>[-dirty]` (`VERSION` is the repo-root semver file, bumped
by hand on deliberate milestones; the git SHA is what actually answers "is this the build with my
latest changes"). `run.sh` doesn't use this one — it's purely for telling builds apart after the
fact. Only the previous versioned copy for that `GAME` is kept, not an ever-growing pile.

## Visual validation belongs to the user — don't self-check

**Standing preference (2026-08-08):** when a change is graphics/rendering/UI-related and needs
eyeballing to confirm it "looks right" (a new shader effect, a camera change, a layout tweak) — do
**not** launch Xvfb, take a screenshot, or otherwise try to self-validate visually. Build it,
confirm it compiles and links cleanly, and hand it back. The user runs the app themselves and will
say if something needs debugging.

Still fine, unprompted:
- Compiling/linking (`make` succeeds, no warnings-as-errors tripped)
- Non-visual checks: the doctest unit suite, `TraceLog` output, anything that doesn't require
  looking at a rendered frame

Not fine, unprompted:
- Standing up Xvfb + launching the binary + screenshotting to "confirm the effect shows up"
- Launching the binary at all just to eyeball a frame

If the user later says something looks off and asks for help debugging, or explicitly asks for a
screenshot, Xvfb is back on the table — match its resolution to `assets/config/engine.yaml`'s
`screenWidth`/`screenHeight` (mismatched resolution makes screenshots miss on-screen content).

## Related

- `docs/learning/rendering.html` — running rendering-topics study doc; shader-effect
  implementations discussed there are the kind of change this applies to.

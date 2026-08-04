# 4. Resource cache: thin raylib-backed cache vs. the book's full `ResCache`

- Status: Proposed
- Date: 2026-08-03

## Context

*Game Coding Complete* Ch. 8 builds a full `ResCache`: a custom `IResourceFile` abstraction whose
concrete implementation reads a Win32-era ZIP asset bundle, a pluggable `IResourceLoader` registry
(one loader per resource type, matched by filename pattern), `ResHandle` objects wrapping the
loaded bits behind `shared_ptr`, and an LRU eviction policy that unloads the least-recently-used
handles once a fixed memory budget is exceeded. That whole layer exists in the book because Win32
doesn't hand you asset loading, decoding, or GPU upload for free — the `ResCache` is doing work the
OS/graphics API won't do for you.

`.claude/skills/engine-architecture/SKILL.md` §4 already covers this ground for frame-3 and takes
a deliberately narrower position: raylib already does the hard part (`LoadTexture`,
`LoadModel`, `LoadShader` — decode plus GPU upload in one call), so what's still worth having is
"a thin layer over raylib, not a new loader" — a cache keyed by resolved path, so loading the same
path twice doesn't re-upload to the GPU. The skill explicitly says not to build the book's
ZIP-bundling layer "until there's an actual reason to (many small files, or a
distribution/build-size concern) — that's premature for a learning project's early demos." No
resource cache exists in code yet either way (`ResourceCache` is a sketch in the skill file only).

This ADR was prompted by a proposal to port the book's full `ResCache` onto raylib close to its
original shape: a ZIP-backed `IResourceFile`, `LoadImageFromMemory`/`LoadTextureFromImage` inside a
`RaylibTextureLoader : IResourceLoader`, `ResHandle`-owned raylib types unloaded from the handle's
destructor, and byte-budgeted LRU eviction. It's recorded here — proposal and tradeoffs — as a
decision point between that full port and the skill's existing thin-cache direction, rather than
silently picking one.

## The proposal: full book `ResCache` ported onto raylib

```cpp
class RaylibTextureLoader : public IResourceLoader {
public:
    virtual std::string VGetPattern() { return "*.png"; }
    virtual bool VUseRawFile() { return false; }

    virtual bool VLoadResource(char *rawBuffer, unsigned int rawSize, shared_ptr<ResHandle> handle) {
        Image img = LoadImageFromMemory(".png", (unsigned char*)rawBuffer, rawSize);
        Texture2D texture = LoadTextureFromImage(img);
        UnloadImage(img); // free the CPU-side copy, texture stays resident in VRAM

        shared_ptr<RaylibTextureExtraData> extra(new RaylibTextureExtraData(texture));
        handle->SetExtra(extra);
        return true;
    }
};

// Registration, once, at startup:
pResCache->RegisterLoader(make_shared<RaylibTextureLoader>());

// Per use site:
Resource res("art/hero.png");
shared_ptr<ResHandle> textureHandle = pResCache->GetHandle(&res);
Texture2D* pTex = (Texture2D*)textureHandle->GetExtra()->VGet();
DrawTexture(*pTex, x, y, WHITE);
```

Pieces this brings, each carried over from the book close to as-is:

- **ZIP-backed `IResourceFile`**: assets shipped as one packed `Resource.zip` rather than loose
  files; `ResCache` opens the ZIP once and reads raw bytes for a given entry name on demand.
- **`IResourceLoader` registry**: one loader class per resource type/extension pattern
  (`RaylibTextureLoader` for `*.png`, a sound loader for `*.wav`, etc.), matched by `VGetPattern()`
  and dispatched to by `ResCache::GetHandle`.
- **`ResHandle` + `shared_ptr`**: a resource stays loaded as long as any `shared_ptr<ResHandle>` to
  it is alive — an explosion's sound won't be unloaded mid-playback just because another system
  released its own handle to the same clip.
- **LRU + byte budget**: once total cached size crosses a configured ceiling (e.g. 128MB), the
  least-recently-used handles are evicted first, calling `UnloadTexture()`/`UnloadSound()` etc. in
  `ResHandle`'s destructor to release VRAM/audio memory.
- **`Preload()`**: walk a manifest and force-load every resource a level needs during a loading
  screen, so gameplay never stalls on a first-use disk/decode hit mid-frame.

## Comparison

| | **Thin cache (skill §4, status quo)** | **Full book `ResCache` (this proposal)** |
|---|---|---|
| What raylib already does for you | `LoadTexture`/`LoadModel`/`LoadShader` do decode + GPU upload in one call — the cache just avoids calling them twice for the same path. | Same raylib calls, but reached through `LoadImageFromMemory`/`LoadTextureFromImage` on bytes the `ResCache` already pulled out of a ZIP. |
| Asset packaging | Loose files under `assets/`, loaded by path — matches `CONVENTIONS.md`'s existing "Games/Examples Directories Organization" section as-is. | A build step to pack `assets/` into `Resource.zip`, plus a ZIP-reading `IResourceFile` — new tooling and a new asset-authoring step (edit a file, repack, rerun) not needed today. |
| Loading path per resource type | One raylib call per type, called directly from the cache. | A loader class per type (`VGetPattern`, `VUseRawFile`, `VLoadResource`) registered into a lookup table — worth it once several types need custom decode logic, overhead when raylib's own loader already does the whole job. |
| Eviction | None — everything loaded stays loaded (fine at this project's current single-digit-entity, single-scene scale, per the engine-architecture skill's stated project-size framing). | LRU + byte budget, actively unloading unused resources under memory pressure — real value once scenes/levels are big enough, or the `PLATFORM_WEB` build's memory ceiling makes it matter. |
| Handle lifetime safety | A raw `Model&`/`Texture2D&` reference into the cache's map — nothing stops a caller from holding a stale reference past the cache's own lifetime, but nothing today creates/destroys the cache more than once either. | `shared_ptr<ResHandle>` gives real lifetime safety — a resource in active use can't be evicted out from under its user — genuinely stronger than the thin cache here. |
| Amount of new code | ~15 lines (skill's existing sketch). | A `ResCache`, `IResourceFile`/ZIP reader, `IResourceLoader` registry, `ResHandle`, LRU list, and one loader class per resource type — a real subsystem. |
| Matches current project scale | Yes — this is a from-scratch, single-scene, C++20 learning project (same framing ADR-0002's scene-graph-options ADR used for its own recommendation). | Sized for "insane complexity" per the book's own framing — more machinery than frame-3's current gameplay (none yet) exercises. |

## Recommendation

**Start with the thin cache** already sketched in `engine-architecture` §4, and land the full
`ResCache`'s individual pieces later, each gated on a concrete trigger rather than built ahead of
need — the same reasoning ADR-0001 used for deferring a hand-rolled ECS, and the same "don't build
before there's a problem" framing the skill file already states for this exact system:

- **ZIP bundling** — once `assets/` grows enough that many-small-files load time or distribution
  size is an actual, measured problem, not before.
- **Pluggable `IResourceLoader` registry** — once more than one or two resource types need
  nontrivial custom load logic beyond a single raylib call; until then it's a dispatch table over
  a switch that doesn't need to exist yet.
- **LRU + byte budget** — once a scene/level's total resource footprint is large enough to matter,
  or the `PLATFORM_WEB` build's tighter memory ceiling makes it a real constraint worth measuring
  against.
- **`ResHandle`/`shared_ptr` lifetime safety** — this piece is worth pulling forward independent of
  the rest: wrapping the thin cache's raylib values in a small `shared_ptr`-held handle costs
  little and closes the "stale reference outlives the cache" gap noted above, without requiring
  ZIP bundling, a loader registry, or LRU eviction to come along with it.

This isn't a rejection of the proposal — it's the same shape, sequenced so each piece lands when
frame-3 actually has the problem it solves, instead of all at once ahead of any gameplay code that
would exercise it.

## Tradeoffs accepted

- No hitch-free `Preload()`-driven loading-screen story yet — accepted because there are no levels
  or loading screens today for it to protect; revisit once a level boundary/loading screen exists.
- No LRU eviction — accepted at current project scale (per engine-architecture skill's explicit
  "small-scale learning project" framing); if the `PLATFORM_WEB` build shows real memory pressure
  before a native-scale reason appears, that's the trigger to pull LRU forward ahead of the other
  deferred pieces.
- No asset-bundle format (ZIP) — accepted because `assets/` is small and loose-file loading
  already matches `CONVENTIONS.md`'s existing asset-organization conventions; a repack step would
  be new authoring friction with no current payoff.
- Callers get a raw reference into the cache rather than a `shared_ptr<ResHandle>`, until the
  handle-safety piece above is pulled forward — accepted short-term since nothing today creates or
  tears down the cache more than once, but flagged as the one piece of the full proposal worth
  adopting early rather than only "when needed."

## Consequences / follow-ups

- `.claude/skills/engine-architecture/SKILL.md` §4 already states this direction; no change needed
  there unless this ADR's recommendation changes on review.
- When the thin `ResourceCache` sketch actually lands in code, update this ADR's Status to
  Accepted (or Superseded, if review favors the full `ResCache` proposal instead) and update the
  skill's "Current state" note per its own "update once it lands" convention (see ADR-0003's
  precedent).
- If/when any of the four deferred pieces above gets pulled forward, record that as its own
  decision (or an amendment here) naming the concrete trigger that justified it, so the "gated on
  a concrete trigger" reasoning stays checkable later rather than becoming "we just built it
  eventually."

## References

- *Game Coding Complete, 4th Edition* (McShaffry & Graham), Ch. 8 — `ResCache`, `IResourceFile`,
  `IResourceLoader`, `ResHandle`, LRU eviction, `Preload()`.
- `.claude/skills/engine-architecture/SKILL.md` §4 — the thin-cache sketch and "don't build ZIP
  bundling until there's an actual reason to" guidance this ADR builds on.
- [ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md) — same "defer the more complex version until
  there's a concrete need" reasoning applied to hand-rolling an ECS.
- [ADR-0002 (scene graph options)](0002-scene-graph-hierarchy-options.md) — same
  from-scratch/small-scale project framing used here to weigh the two options.

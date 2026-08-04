# 4. Resource cache: thin raylib-backed cache vs. the book's full `ResCache`

- Status: Accepted
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

## What actually shipped

`ResourceCache<T>` (`src/app/resource_cache.h`, header-only, templated over the resource type)
implements the thin cache plus the `ResHandle`/`shared_ptr` lifetime-safety piece together, not the
thin cache alone:

```cpp
template <typename T>
class ResourceCache {
public:
    using Loader = std::function<T(const char *path)>;
    using Unloader = std::function<void(T &resource)>;

    ResourceCache(Loader loader, Unloader unloader);

    // Loads `path` via `loader_` on first request (or if every previous handle to it has already
    // been released); repeat calls while a handle is still live return a shared_ptr to the same
    // underlying resource. The returned shared_ptr's own deleter calls `unloader_` -- runs the
    // moment its last holder releases it, cache included.
    std::shared_ptr<T> GetHandle(const std::string &path);

    void Clear();   // drops the cache's own bookkeeping; doesn't force-unload a live handle

private:
    Loader loader_;
    Unloader unloader_;
    std::unordered_map<std::string, std::weak_ptr<T>> resources_;   // weak: doesn't keep alive on its own
};
```

`Engine` (`src/app/engine.h`/`.cpp`) owns one `ResourceCache<Font>` and one `ResourceCache<Sound>`,
exposed via `Fonts()`/`Sounds()`. `Engine::Init()` now loads the font and coin sound through these
caches instead of calling `LoadFont`/`LoadSound` directly:

```cpp
fontHandle_ = fontCache_.GetHandle("resources/characters/mecha.png");
font = *fontHandle_;   // screens.h's plain-C code reads the extern `font` global, not a shared_ptr
soundHandle_ = soundCache_.GetHandle("resources/audio/fx/coin.wav");
fxCoin = *soundHandle_;
```

`fontHandle_`/`soundHandle_` (`std::shared_ptr<Font>`/`std::shared_ptr<Sound>` members) exist
because `screens.h`'s five `screen_*.c` files are still plain C and read `font`/`fxCoin` as plain
`extern` globals (ADR-0001, Decision 2) — they can't hold a `shared_ptr` themselves. `Engine` holds
the handle to keep the underlying resource alive for the app's whole lifetime; the plain globals
get an ordinary copy of raylib's value-type struct, which is how raylib already expects
`Font`/`Sound` to be passed around (a lightweight handle to GPU/audio-resident data, not the data
itself) — copying it doesn't duplicate the GPU/audio resource.

### A real hazard this surfaced, not called out in the original proposal

`Engine::Shutdown()` (pre-existing code) explicitly unloads in a specific order:
`UnloadFont`/`UnloadSound` *before* `CloseAudioDevice()`/`CloseWindow()`, because those `Unload*`
calls need a live GL/audio context. A `shared_ptr`-held handle whose deleter calls `UnloadFont`/
`UnloadSound` only runs that deleter when the *last* reference is released — which, for a plain
class member, is whenever the object holding it gets destroyed. If `fontHandle_`/`soundHandle_`
were left to their own destructors, that would happen when `Engine` itself is destroyed (`main`'s
`Engine engine;` going out of scope) — **after** `Shutdown()` already called `CloseAudioDevice()`/
`CloseWindow()`, since `Shutdown()` is called explicitly mid-`main`, not from `Engine`'s destructor.
That would call `UnloadFont`/`UnloadSound` into an already-closed context.

Fixed by resetting the handles (and calling `Clear()` on both caches) explicitly at the top of
`Engine::Shutdown()`, before `CloseAudioDevice()`/`CloseWindow()` — same discipline the pre-existing
manual `UnloadFont`/`UnloadSound` calls already followed, just moved behind the cache's handles
instead of calling `UnloadFont`/`UnloadSound` directly:

```cpp
void Engine::Shutdown() {
    fontHandle_.reset();
    soundHandle_.reset();
    fontCache_.Clear();
    soundCache_.Clear();

    UnloadMusicStream(music);
    CloseAudioDevice();
    CloseWindow();
}
```

Verified with a standalone `Init()`/`Shutdown()` smoke test run under `xvfb-run` (no window-close
event needed) — exits cleanly, raylib's own logging confirms the font texture loads once and
unloads once, no use-after-close.

### One deliberate deviation from "no eviction"

The comparison table above lists the thin cache's eviction policy as "None — everything loaded
stays loaded." What shipped is slightly different: `resources_` holds a `std::weak_ptr<T>` per
path rather than a `shared_ptr<T>`, so a resource unloads as soon as every `shared_ptr<T>` handle
to it is released — including the cache's own, since it never holds a strong reference. This is
**not** the book's LRU/byte-budget policy (no size tracking, no "least recently used" ordering,
nothing keyed on a memory ceiling) — it's a simpler property that falls out of using `shared_ptr`
for lifetime safety at all: a resource nobody references anymore doesn't linger just because the
cache itself is still alive. Chosen because it costs nothing extra to get (the alternative, a
`shared_ptr<T>` map that never releases its own reference, would need an explicit eviction call
somewhere to free anything) and doesn't contradict the ADR's "no LRU yet" decision — LRU is about
evicting things still in use under memory pressure; this only ever frees something already unused.

## Tradeoffs accepted

The list below is unchanged from the original proposal, plus one new item reflecting what actually
shipped (the `shared_ptr`/`ResHandle` piece landed alongside the thin cache, not after it, and the
weak_ptr-driven early-free behavior noted above):

- No hitch-free `Preload()`-driven loading-screen story yet — accepted because there are no levels
  or loading screens today for it to protect; revisit once a level boundary/loading screen exists.
- No LRU eviction — accepted at current project scale (per engine-architecture skill's explicit
  "small-scale learning project" framing); if the `PLATFORM_WEB` build shows real memory pressure
  before a native-scale reason appears, that's the trigger to pull LRU forward ahead of the other
  deferred pieces.
- No asset-bundle format (ZIP) — accepted because `assets/` is small and loose-file loading
  already matches `CONVENTIONS.md`'s existing asset-organization conventions; a repack step would
  be new authoring friction with no current payoff.
- **(New)** A resource can silently reload from disk if every handle to it is released and then
  `GetHandle` is called again for the same path — there's no way to tell, from the caller's side,
  whether a given `GetHandle` call was a cache hit or a fresh reload. Accepted because nothing in
  the codebase releases a handle and re-requests the same path yet (`Engine` holds its two handles
  for the app's whole lifetime); revisit with an explicit hit/miss signal if that pattern shows up
  and the reload cost turns out to matter.
- ~~Callers get a raw reference into the cache rather than a `shared_ptr<ResHandle>`, until the
  handle-safety piece above is pulled forward~~ — superseded by "What actually shipped" above: the
  `shared_ptr` handle landed in the same change as the thin cache, not deferred.

## Consequences / follow-ups

- `.claude/skills/engine-architecture/SKILL.md` §4 updated to point at `src/app/resource_cache.h`
  and `Engine::Fonts()`/`Sounds()` instead of only the sketch, per the skill's own "update once it
  lands" note.
- `Engine::Init()`/`Shutdown()` (`src/app/engine.cpp`) now go through `fontCache_`/`soundCache_`
  instead of calling `LoadFont`/`UnloadFont`/`LoadSound`/`UnloadSound` directly — see "What actually
  shipped" above, including the shutdown-ordering fix this surfaced.
- `src/tests/resource_cache_test.cpp` (doctest, ADR-0006) covers `ResourceCache<T>` with a fake
  int-based loader/unloader — no raylib/window dependency needed, same reasoning
  `event_manager_test.cpp`/`process_manager_test.cpp` already used.
- The four deferred pieces (ZIP bundling, pluggable loader registry, LRU+budget, `Preload()`) are
  still not built. If/when any gets pulled forward, record that as its own decision (or an
  amendment here) naming the concrete trigger that justified it, so the "gated on a concrete
  trigger" reasoning stays checkable later rather than becoming "we just built it eventually."
- The next resource type to go through `ResourceCache<T>` (e.g. `ResourceCache<Texture2D>` or
  `ResourceCache<Model>`, once something loads either) should be the first real test of whether the
  `Loader`/`Unloader` `std::function` signatures generalize cleanly, or whether a given raylib
  type's loader needs something the current shape (`T(*)(const char*)`-compatible, single-argument)
  doesn't fit (e.g. `LoadModelFromMesh` isn't a path-based loader at all).

## References

- *Game Coding Complete, 4th Edition* (McShaffry & Graham), Ch. 8 — `ResCache`, `IResourceFile`,
  `IResourceLoader`, `ResHandle`, LRU eviction, `Preload()`.
- `.claude/skills/engine-architecture/SKILL.md` §4 — the thin-cache sketch and "don't build ZIP
  bundling until there's an actual reason to" guidance this ADR builds on.
- [ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md) — same "defer the more complex version until
  there's a concrete need" reasoning applied to hand-rolling an ECS.
- [ADR-0002 (scene graph options)](0002-scene-graph-hierarchy-options.md) — same
  from-scratch/small-scale project framing used here to weigh the two options.

# 3. Resource cache: a thin path-keyed layer over raylib, not a ZIP-based `ResCache` port

- Status: Accepted
- Date: 2026-08-03

## Context

*Game Coding Complete* Ch. 8 builds a full resource cache: `IResourceFile` abstracts a source of
raw bytes (a loose directory or a ZIP archive), `IResourceLoader` implementations turn those raw
bytes into a usable in-engine type, `ResHandle` wraps the loaded result behind a `shared_ptr` so
nothing gets unloaded while still in use, and an LRU eviction policy keeps total memory under a
budget. That whole layer exists because Win32/DirectX in 2004 handed you neither a byte-stream
abstraction over archives nor GPU-upload-from-memory for free — the book had to build both.

`.claude/skills/engine-architecture` §4 already covers the raylib-era version of this in sketch
form: raylib's own loaders (`LoadTexture`, `LoadModel`, `LoadSound`, …) already do the hard part —
including GPU upload — so what's actually still worth having is a thin cache keyed by resolved
path, not a rebuild of the book's `IResourceFile`/ZIP/LRU machinery. This ADR exists because a
proposal for the full book-style port (below) was raised and needed a documented resolution
against that already-sketched lighter design, the same way ADR-0002 resolved the Event Manager
proposal against the skill's modernized design.

No resource cache of either shape exists in code yet (`.claude/skills/engine-architecture` §4's
"Current state" note still applies) — this ADR records the design to build toward, not something
already landed.

## Starting proposal vs. decided design

The starting point was a sketch of the book's Ch. 8 `ResCache` ported onto raylib fairly literally:

```cpp
// Starting proposal (not decided -- see below)
class RaylibTextureLoader : public IResourceLoader {
public:
    virtual std::string VGetPattern() { return "*.png"; }
    virtual bool VUseRawFile() { return false; } // requires processing

    virtual bool VLoadResource(char *rawBuffer, unsigned int rawSize, shared_ptr<ResHandle> handle) {
        Image img = LoadImageFromMemory(".png", (unsigned char*)rawBuffer, rawSize);
        Texture2D texture = LoadTextureFromImage(img);
        UnloadImage(img); // frees RAM copy, keeps the VRAM upload

        shared_ptr<RaylibTextureExtraData> extra(new RaylibTextureExtraData(texture));
        handle->SetExtra(extra);
        return true;
    }
};

// Setup:
pResCache->RegisterLoader(make_shared<RaylibTextureLoader>());

// Use:
Resource res("art/hero.png");
shared_ptr<ResHandle> textureHandle = pResCache->GetHandle(&res);
Texture2D* pTex = (Texture2D*)textureHandle->GetExtra()->VGet();
DrawTexture(*pTex, x, y, WHITE);
```

Backed by: assets packed into a ZIP archive at build/ship time (rather than loose files), `ResCache`
reading raw bytes out of that archive via `IResourceFile`, an LRU policy evicting the
least-recently-used resources once a memory budget (e.g. 128MB) is hit, and a `Preload()` step that
warms the cache during a loading screen to avoid mid-gameplay stutter.

The decided design (matches `.claude/skills/engine-architecture` §4's existing sketch):

```cpp
class ResourceCache {
public:
    Model &GetModel(const std::string &path) {
        auto it = models_.find(path);
        if (it != models_.end()) return it->second;
        return models_.emplace(path, LoadModel(path.c_str())).first->second;
    }
    ~ResourceCache() { for (auto &[_, m] : models_) UnloadModel(m); }
private:
    std::unordered_map<std::string, Model> models_;
};
```

One such cache per raylib resource type that needs sharing (`Model`, `Texture2D`, `Sound`, …),
keyed directly by the resolved asset path, loading straight off disk (or `PLATFORM_WEB`'s preloaded
virtual filesystem — already handled by raylib/Emscripten, not something this cache needs to know
about) via raylib's own `Load*` functions.

| Starting proposal | Decided design | Why |
|---|---|---|
| Assets packed into a ZIP, read via `IResourceFile` + `LoadImageFromMemory` | Assets stay loose files, loaded directly via `LoadTexture(path)`/`LoadModel(path)`/… | The book's ZIP format exists to solve a problem raylib doesn't have (no built-in archive/byte-stream loading in Win32) and a problem this project doesn't have yet (many small files, or a distribution/build-size concern). Revisit only once one of those becomes real, per the skill's existing guidance. |
| `IResourceLoader` per resource type, registered into a generic `ResCache` | One small `ResourceCache`-shaped class per raylib type that needs caching (as sketched in the skill), no shared loader-registration abstraction | The abstraction earns its keep when there are multiple *kinds* of `IResourceFile` (loose dir vs. ZIP) to swap between transparently. With one source (the filesystem) and raylib already doing format detection/decoding internally, the extra indirection has no behavior to abstract over yet. |
| `shared_ptr<ResHandle>` wrapping every loaded resource | A plain reference returned from the cache's map (e.g. `Model &`) | The book's `shared_ptr` matters because a resource can be evicted (LRU) while still referenced elsewhere. Without LRU eviction (see next row), nothing unloads a resource out from under a live reference except the cache's own destructor — so the extra ownership machinery isn't defending against anything real yet. Revisit together with LRU, not separately. |
| LRU eviction against a memory budget (e.g. 128MB) | No eviction — everything loaded stays loaded until `ResourceCache`'s destructor (or an explicit future `Unload(path)`) | Same "no problem yet" reasoning as the ZIP row: LRU exists to keep a *large*, *varied* asset set under a memory ceiling. A learning project's early demos don't have that asset volume; building the policy now would be tuning against a workload that doesn't exist. |
| `Preload()` during a loading screen (hitch-free gameplay) | Loads happen lazily, on first `GetModel`/`GetTexture`/… call | No loading-screen flow exists yet in `screens.h` to preload into; a `Preload(std::vector<std::string> paths)` method is straightforward to add to the sketch above once one does, without needing the rest of the book's machinery first. |

## Decision

Build the resource cache as a thin, path-keyed wrapper directly over raylib's own loaders — one
small cache class per raylib resource type that actually needs sharing, following the sketch above
— rather than porting the book's `IResourceFile`/`IResourceLoader`/ZIP/LRU/`ResHandle` machinery.
Defer each piece of that machinery specifically until its own triggering condition is real:

- **ZIP/archive packing** — once loose-file count or distribution size is an actual problem.
- **LRU eviction** — once the asset set is large/varied enough that total memory is a real risk.
- **`shared_ptr`-wrapped handles** — revisit together with LRU, since that's what it protects
  against.
- **`Preload()`** — add once a loading-screen flow exists to call it from; cheap to bolt onto the
  sketch above later, not a reason to build the rest of the book's layer now.

## Consequences / follow-ups

- No code changes from this ADR — `.claude/skills/engine-architecture` §4 already carries the
  sketch this decision points to; this ADR is the record of *why* the fuller book-style proposal
  was not adopted, for when that proposal (or a piece of it) comes up again.
- When the resource cache actually gets built, update the skill's §4 "Current state" note the same
  way §§1-3 were updated after `EventManager`/`ProcessManager`/`Engine`'s `entt::registry` landed
  (ADR-0001, ADR-0002).
- If a future need reopens any deferred piece (ZIP packing, LRU, `Preload`), it should cite the
  specific triggering condition from the table above, not be adopted wholesale because the book
  has it.

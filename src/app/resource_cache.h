// Resource cache (Game Coding Complete Ch. 8, modernized -- see docs/adr/0004 and
// .claude/skills/engine-architecture SKILL.md Sec 4). The book's ResCache exists to do work Win32
// never handed you for free: reading raw bytes out of a ZIP, decoding them, and uploading to the
// GPU/audio device. raylib already does decode + upload in one call (LoadTexture, LoadSound,
// LoadFont, LoadModel...), so this doesn't reimplement any of that -- it's a thin, path-keyed
// layer on top of whichever Load*/Unload* pair a caller hands it, so the same path never gets
// loaded (and re-uploaded to the GPU/audio device) twice.
//
// ADR-0004 recommends pulling one piece of the book's ResHandle forward even in the thin cache: a
// shared_ptr-held handle instead of a bare reference, so an in-use resource can never be freed out
// from under whoever still holds a handle to it. That's what GetHandle() returns here. The cache's
// own bookkeeping only holds a weak_ptr per path -- once every shared_ptr<T> a caller holds is
// gone, the resource unloads immediately via the shared_ptr's own deleter, and the weak_ptr simply
// expires. That's a smaller, simpler mechanism than the book's byte-budgeted LRU (ADR-0004
// explicitly defers LRU until a concrete memory-pressure reason to build it shows up), while still
// not leaking a resource nobody references anymore -- "no eviction" (ADR-0004's stated starting
// point) becomes "no eviction *policy* needed" rather than "resources pile up forever."
#ifndef RESOURCE_CACHE_H
#define RESOURCE_CACHE_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

template <typename T>
class ResourceCache {
public:
    // Loader/Unloader take/return T by value, matching raylib's own Load*/Unload* signatures
    // (e.g. `Font LoadFont(const char *)`, `void UnloadFont(Font)`) closely enough that a raylib
    // function can be passed directly: ResourceCache<Font> cache(LoadFont, UnloadFont);
    using Loader = std::function<T(const char *path)>;
    using Unloader = std::function<void(T &resource)>;

    ResourceCache(Loader loader, Unloader unloader)
        : loader_(std::move(loader)), unloader_(std::move(unloader)) {}

    // Returns a handle to the resource at `path`. If a live handle to that path already exists
    // (i.e. some other caller's shared_ptr<T> hasn't been released yet), returns a shared_ptr to
    // that same resource -- no duplicate load. Otherwise loads it via `loader_` and starts
    // tracking it. The returned shared_ptr's deleter calls `unloader_` and runs the moment the
    // last holder releases it, whether that's this cache dropping its own (weak) reference or a
    // caller resetting theirs.
    std::shared_ptr<T> GetHandle(const std::string &path) {
        auto it = resources_.find(path);
        if (it != resources_.end()) {
            if (std::shared_ptr<T> existing = it->second.lock()) return existing;
        }

        Unloader unloader = unloader_;
        std::shared_ptr<T> handle(new T(loader_(path.c_str())), [unloader](T *resource) {
            unloader(*resource);
            delete resource;
        });
        resources_[path] = handle;
        return handle;
    }

    // Drops the cache's own bookkeeping for every path. Does NOT force-unload a resource some
    // caller still holds a shared_ptr to -- that handle keeps working exactly as before, and
    // unloads normally once its last holder releases it. Intended to be called once, during
    // shutdown, so the cache doesn't hand out handles to a stale path after teardown starts.
    void Clear() { resources_.clear(); }

private:
    Loader loader_;
    Unloader unloader_;
    std::unordered_map<std::string, std::weak_ptr<T>> resources_;
};

#endif // RESOURCE_CACHE_H

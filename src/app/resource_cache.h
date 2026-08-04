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
//
// WARNING: that same "unloads via the deleter the moment the last handle is released" property
// means a handle must never outlive whatever closed the GL/audio context its Unloader needs (e.g.
// raylib's CloseWindow()/CloseAudioDevice()). A ResourceCache<T> itself has no way to enforce
// this -- it doesn't know when that context closes, only whoever owns the cache (Engine, for the
// caches it owns -- see engine.h) does. Reproduced as a real crash (SIGSEGV in
// rlUnloadShaderProgram) while implementing Engine::Models()/Textures()/GetShader(): a caller-held
// Model/Texture2D/Shader handle that outlived Engine::Shutdown() called Unload* into an
// already-closed context the moment it finally went out of scope. See ADR-0004.
#ifndef RESOURCE_CACHE_H
#define RESOURCE_CACHE_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

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

// ResourceCache<T> keys on a single string. Most raylib Load* functions take one file path
// (LoadModel, LoadTexture, LoadFont, LoadSound, ...) and fit that directly -- but a few take more
// than one (LoadShader(vsFileName, fsFileName)). Rather than redesigning ResourceCache<T> itself
// around a variadic key, Combine()/Split() let a caller fold N related paths into one cache key
// and unfold it back inside the Loader lambda. Not raylib-specific -- any multi-argument loader
// can reuse this instead of hand-rolling its own delimiter scheme.
namespace ResourceCacheKeys {
    // ASCII Unit Separator: not a character any of this project's asset paths would plausibly
    // contain, unlike more common delimiter choices (':', ';', '|', all valid in real filenames on
    // at least one platform this project targets).
    constexpr char kDelimiter = '\x1f';

    inline std::string Combine(const std::string &a, const std::string &b) {
        return a + kDelimiter + b;
    }

    // Splits a key built by Combine() back into its two parts. If `combined` has no delimiter
    // (i.e. it wasn't built by Combine()), returns it whole as the first part and an empty second
    // part, rather than failing -- lets a caller treat "no fragment path" as a valid input.
    inline std::pair<std::string, std::string> Split(const std::string &combined) {
        size_t pos = combined.find(kDelimiter);
        if (pos == std::string::npos) return {combined, std::string()};
        return {combined.substr(0, pos), combined.substr(pos + 1)};
    }
}

#endif // RESOURCE_CACHE_H

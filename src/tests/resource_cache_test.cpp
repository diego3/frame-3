#include "doctest/doctest.h"

#include "app/resource_cache.h"

// ResourceCache<T> is a template over any loader/unloader pair, so it's exercised here with a
// plain int standing in for a raylib resource type (Texture2D, Font, Sound, ...) -- no raylib
// dependency needed, matching event_manager_test.cpp/process_manager_test.cpp's own no-raylib
// approach for the other pure-logic systems.
namespace {
    struct FakeLoader {
        int loadCount = 0;
        int unloadCount = 0;
        int nextValue = 1;

        int Load(const char *) {
            loadCount++;
            return nextValue++;
        }

        void Unload(int &) { unloadCount++; }
    };
}

TEST_CASE("GetHandle loads a path only once while a handle to it is still held") {
    FakeLoader fake;
    ResourceCache<int> cache(
        [&](const char *path) { return fake.Load(path); },
        [&](int &value) { fake.Unload(value); });

    std::shared_ptr<int> first = cache.GetHandle("art/hero.png");
    std::shared_ptr<int> second = cache.GetHandle("art/hero.png");

    CHECK(fake.loadCount == 1);
    CHECK(first == second);   // same underlying resource, not a fresh load
    CHECK(*first == *second);
}

TEST_CASE("Different paths load independently") {
    FakeLoader fake;
    ResourceCache<int> cache(
        [&](const char *path) { return fake.Load(path); },
        [&](int &value) { fake.Unload(value); });

    std::shared_ptr<int> hero = cache.GetHandle("art/hero.png");
    std::shared_ptr<int> villain = cache.GetHandle("art/villain.png");

    CHECK(fake.loadCount == 2);
    CHECK(*hero != *villain);
}

TEST_CASE("Resource unloads once every handle to it is released") {
    FakeLoader fake;
    ResourceCache<int> cache(
        [&](const char *path) { return fake.Load(path); },
        [&](int &value) { fake.Unload(value); });

    {
        std::shared_ptr<int> handle = cache.GetHandle("art/hero.png");
        CHECK(fake.unloadCount == 0);
    }   // handle goes out of scope here -- nothing else references this resource

    CHECK(fake.unloadCount == 1);
}

TEST_CASE("A path already loaded again after every prior handle released reloads instead of reusing a stale one") {
    FakeLoader fake;
    ResourceCache<int> cache(
        [&](const char *path) { return fake.Load(path); },
        [&](int &value) { fake.Unload(value); });

    { std::shared_ptr<int> handle = cache.GetHandle("art/hero.png"); }
    CHECK(fake.unloadCount == 1);

    std::shared_ptr<int> reloaded = cache.GetHandle("art/hero.png");

    CHECK(fake.loadCount == 2);   // loaded again, not served from a stale/expired cache entry
    CHECK(*reloaded == 2);        // FakeLoader's second call returns a distinct value
}

TEST_CASE("A resource still held by a caller survives Clear()") {
    FakeLoader fake;
    ResourceCache<int> cache(
        [&](const char *path) { return fake.Load(path); },
        [&](int &value) { fake.Unload(value); });

    std::shared_ptr<int> handle = cache.GetHandle("art/hero.png");
    cache.Clear();

    CHECK(fake.unloadCount == 0);   // Clear() drops the cache's own bookkeeping only
    CHECK(*handle == 1);            // the handle itself is still perfectly usable

    handle.reset();
    CHECK(fake.unloadCount == 1);   // unloads normally once its last holder releases it
}

TEST_CASE("Clear() makes a subsequent GetHandle for the same path load fresh") {
    FakeLoader fake;
    ResourceCache<int> cache(
        [&](const char *path) { return fake.Load(path); },
        [&](int &value) { fake.Unload(value); });

    std::shared_ptr<int> first = cache.GetHandle("art/hero.png");
    cache.Clear();
    std::shared_ptr<int> second = cache.GetHandle("art/hero.png");

    CHECK(fake.loadCount == 2);
    CHECK(first != second);   // Clear() forgot the cache's link to `first`'s entry
}

// ResourceCacheKeys (used to key ResourceCache<Shader> on a vertex+fragment path pair -- see
// Engine::GetShader in engine.cpp) is plain string manipulation, no raylib dependency either.
TEST_CASE("ResourceCacheKeys::Split reverses Combine") {
    std::string combined = ResourceCacheKeys::Combine("shaders/pbr.vs", "shaders/pbr.fs");
    auto [vsPath, fsPath] = ResourceCacheKeys::Split(combined);

    CHECK(vsPath == "shaders/pbr.vs");
    CHECK(fsPath == "shaders/pbr.fs");
}

TEST_CASE("ResourceCacheKeys round-trips an empty second path") {
    // Mirrors calling Engine::GetShader("shaders/custom.vs", "") -- raylib's LoadShader treats an
    // empty/null fragment path as "use the default fragment shader".
    std::string combined = ResourceCacheKeys::Combine("shaders/custom.vs", "");
    auto [vsPath, fsPath] = ResourceCacheKeys::Split(combined);

    CHECK(vsPath == "shaders/custom.vs");
    CHECK(fsPath == "");
}

TEST_CASE("ResourceCacheKeys::Combine keeps two different vs/fs pairs distinct") {
    std::string a = ResourceCacheKeys::Combine("a.vs", "shared.fs");
    std::string b = ResourceCacheKeys::Combine("b.vs", "shared.fs");

    CHECK(a != b);
}

TEST_CASE("ResourceCacheKeys::Split on a key with no delimiter returns it whole, empty second part") {
    auto [first, second] = ResourceCacheKeys::Split("not/combined/by/Combine.png");

    CHECK(first == "not/combined/by/Combine.png");
    CHECK(second == "");
}

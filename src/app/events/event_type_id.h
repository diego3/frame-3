// Stable, compile-time type identity for events that cross a process or time boundary (network,
// event journal -- see docs/adr/0005). EventManager's local Subscribe/Emit/Queue key on
// std::type_index, which is implementation-defined and not guaranteed to match between two
// separately-built binaries, let alone a save/journal file read by a build compiled months later.
// entt::type_hash has the same problem, since it hashes a compiler-generated string
// (__PRETTY_FUNCTION__ or equivalent) that can differ across compiler vendors/versions.
//
// Fnv1aHash instead hashes a human-authored name string, entirely at compile time -- stable
// forever because both the algorithm and the input are fixed by us, nothing compiler-internal.
#ifndef EVENT_TYPE_ID_H
#define EVENT_TYPE_ID_H

#include <cstdint>
#include <string_view>

constexpr uint32_t Fnv1aHash(std::string_view s) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : s) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

#endif // EVENT_TYPE_ID_H

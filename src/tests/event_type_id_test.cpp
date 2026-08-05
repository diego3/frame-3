#include "doctest/doctest.h"

#include "app/event_type_id.h"

namespace {
    // Compile-time checks: Fnv1aHash is genuinely constexpr, and two distinct names never
    // collide for the specific handful of strings this project's early event types would use.
    static_assert(Fnv1aHash("EvtData_Destroy_Actor") == Fnv1aHash("EvtData_Destroy_Actor"));
    static_assert(Fnv1aHash("EvtData_Destroy_Actor") != Fnv1aHash("EvtData_Spawn_Actor"));
}

TEST_CASE("Fnv1aHash is stable for the same input") {
    CHECK(Fnv1aHash("EvtData_Destroy_Actor") == Fnv1aHash("EvtData_Destroy_Actor"));
}

TEST_CASE("Fnv1aHash differs for different input") {
    CHECK(Fnv1aHash("EvtData_Destroy_Actor") != Fnv1aHash("EvtData_Spawn_Actor"));
}

TEST_CASE("Fnv1aHash of an empty string is the FNV-1a offset basis") {
    CHECK(Fnv1aHash("") == 2166136261u);
}

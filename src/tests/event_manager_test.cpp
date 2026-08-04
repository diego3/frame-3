#include "doctest/doctest.h"

#include <vector>

#include "app/event_manager.h"

namespace {
    struct DamageEvent {
        int amount;
    };

    struct HealEvent {
        int amount;
    };
}

TEST_CASE("Emit calls the handler subscribed to that event type") {
    EventManager events;
    int callCount = 0;
    int lastAmount = 0;

    events.Subscribe<DamageEvent>([&](const DamageEvent &e) {
        callCount++;
        lastAmount = e.amount;
    });

    events.Emit(DamageEvent{5});

    CHECK(callCount == 1);
    CHECK(lastAmount == 5);
}

TEST_CASE("Multiple handlers for the same type all run, in subscription order") {
    EventManager events;
    std::vector<int> order;

    events.Subscribe<DamageEvent>([&](const DamageEvent &) { order.push_back(1); });
    events.Subscribe<DamageEvent>([&](const DamageEvent &) { order.push_back(2); });

    events.Emit(DamageEvent{1});

    REQUIRE(order.size() == 2);
    CHECK(order[0] == 1);
    CHECK(order[1] == 2);
}

TEST_CASE("Emit with no subscribers is a no-op, not a crash") {
    EventManager events;
    CHECK_NOTHROW(events.Emit(DamageEvent{1}));
}

TEST_CASE("Handlers only fire for the event type they subscribed to") {
    EventManager events;
    bool damageFired = false;
    bool healFired = false;

    events.Subscribe<DamageEvent>([&](const DamageEvent &) { damageFired = true; });
    events.Subscribe<HealEvent>([&](const HealEvent &) { healFired = true; });

    events.Emit(HealEvent{10});

    CHECK_FALSE(damageFired);
    CHECK(healFired);
}

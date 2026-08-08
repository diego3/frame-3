#include "doctest/doctest.h"

#include <vector>

#include "app/events/event_manager.h"

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

TEST_CASE("Queue does not dispatch until DispatchQueued is called") {
    EventManager events;
    int callCount = 0;

    events.Subscribe<DamageEvent>([&](const DamageEvent &) { callCount++; });
    events.Queue(DamageEvent{5});

    CHECK(callCount == 0);

    events.DispatchQueued();

    CHECK(callCount == 1);
}

TEST_CASE("DispatchQueued runs queued events in the order they were queued") {
    EventManager events;
    std::vector<int> order;

    events.Subscribe<DamageEvent>([&](const DamageEvent &e) { order.push_back(e.amount); });
    events.Queue(DamageEvent{1});
    events.Queue(DamageEvent{2});
    events.DispatchQueued();

    REQUIRE(order.size() == 2);
    CHECK(order[0] == 1);
    CHECK(order[1] == 2);
}

TEST_CASE("DispatchQueued only runs each queued event once, even across multiple calls") {
    EventManager events;
    int callCount = 0;

    events.Subscribe<DamageEvent>([&](const DamageEvent &) { callCount++; });
    events.Queue(DamageEvent{1});

    events.DispatchQueued();
    events.DispatchQueued();

    CHECK(callCount == 1);
}

TEST_CASE("A handler queuing another event during DispatchQueued defers it to the next call") {
    EventManager events;
    int dispatchCount = 0;

    events.Subscribe<DamageEvent>([&](const DamageEvent &e) {
        dispatchCount++;
        if (e.amount == 1) events.Queue(DamageEvent{2});
    });

    events.Queue(DamageEvent{1});
    events.DispatchQueued();

    CHECK(dispatchCount == 1);   // the re-queued event hasn't run yet...

    events.DispatchQueued();

    CHECK(dispatchCount == 2);   // ...until the next call.
}

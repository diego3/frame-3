// Event manager (Game Coding Complete Ch. 4, modernized -- see .claude/skills/engine-architecture).
// The book multicasts events by a 32-bit GUID plus a monolithic event-type enum; that's a
// workaround for a pre-templates C++ era. Here, events are just structs and subscribers key off
// std::type_index, so adding a new event type never touches a shared enum.
#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

class EventManager {
public:
    // Type-erased handler stored per std::type_index -- Subscribe<T> wraps the caller's
    // std::function<void(const T &)> in one of these so handlers_ can hold every T's handlers in
    // a single map.
    using RawHandler = std::function<void(const void *)>;
    // A deferred Emit<T> call captured by Queue<T>, replayed as-is by DispatchQueued().
    using PendingAction = std::function<void()>;

    // Registers a handler for events of type T. Multiple handlers may subscribe to the same T;
    // all of them run, in subscription order, on the next Emit<T>.
    template <typename T>
    void Subscribe(std::function<void(const T &)> handler) {
        handlers_[std::type_index(typeid(T))].push_back(
            [handler](const void *event) { handler(*static_cast<const T *>(event)); });
    }

    // Dispatches event to every handler subscribed to T, synchronously, in the calling frame.
    template <typename T>
    void Emit(const T &event) {
        auto it = handlers_.find(std::type_index(typeid(T)));
        if (it == handlers_.end()) return;

        for (auto &handler : it->second) handler(&event);
    }

    // Defers event to the next DispatchQueued() call instead of dispatching it immediately
    // (ADR-0005 Sec 5). Prefer this over Emit<T> for events crossing system boundaries: nothing
    // stops a handler from Subscribe<T>/Emit<T>-ing the same type it's currently handling, which
    // would invalidate Emit<T>'s in-progress iteration over handlers_[type]. Queue<T> sidesteps
    // that by never running a handler during another handler's own dispatch.
    template <typename T>
    void Queue(T event) {
        pending_.push_back([this, event = std::move(event)] { Emit(event); });
    }

    // Dispatches every event Queue()'d since the last call. Intended to be ticked once per frame
    // (see Engine::Run's TickAndUpdateDraw). Swaps pending_ into a local vector first, so an event
    // Queue()'d *during* this dispatch (e.g. a handler reacting to one queued event by queuing
    // another) lands in the now-empty pending_ and runs on the *next* DispatchQueued() call, not
    // this one -- avoids iterating a vector that's still being appended to mid-loop.
    void DispatchQueued() {
        std::vector<PendingAction> active;
        std::swap(active, pending_);

        for (auto &dispatch : active) dispatch();
    }

private:
    std::unordered_map<std::type_index, std::vector<RawHandler>> handlers_;
    std::vector<PendingAction> pending_;
};

#endif // EVENT_MANAGER_H

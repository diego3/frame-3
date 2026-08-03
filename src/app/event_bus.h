// Event bus (Game Coding Complete Ch. 4, modernized -- see .claude/skills/engine-architecture).
// The book multicasts events by a 32-bit GUID plus a monolithic event-type enum; that's a
// workaround for a pre-templates C++ era. Here, events are just structs and subscribers key off
// std::type_index, so adding a new event type never touches a shared enum.
#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

class EventBus {
public:
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

private:
    std::unordered_map<std::type_index, std::vector<std::function<void(const void *)>>> handlers_;
};

#endif // EVENT_BUS_H

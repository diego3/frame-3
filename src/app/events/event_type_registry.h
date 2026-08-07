// Factory table for reconstructing an ISerializableEvent from raw bytes plus its type ID -- the
// one place this design genuinely needs runtime type lookup (docs/adr/0005 Sec 3). Local dispatch
// (EventManager::Subscribe/Emit/Queue) never needs this: the compiler already knows the concrete
// type at every call site. The receiving end of a network message or a journal record only has a
// uint32_t type ID and a byte buffer, and has to produce the right concrete C++ type without
// knowing it at compile time -- a genuine property of crossing a serialization boundary at all,
// not a limitation of anything EventManager itself does locally.
#ifndef EVENT_TYPE_REGISTRY_H
#define EVENT_TYPE_REGISTRY_H

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

#include "app/events/serializable_event.h"

class EventTypeRegistry {
public:
    // Registers a factory for T under T::kTypeId. T is expected to derive from
    // ISerializableEvent and expose `static constexpr uint32_t kTypeId`, matching the shape
    // ADR-0005 Sec 2's example (EvtData_Destroy_Actor) uses.
    template <typename T>
    void Register() {
        factories_[T::kTypeId] = [] { return std::make_unique<T>(); };
    }

    // Returns a default-constructed instance of whichever registered type matches typeId, ready
    // for VDeserialize() to fill in -- or nullptr if nothing is registered under that ID (e.g. a
    // journal record written by a newer build than the one reading it).
    std::unique_ptr<ISerializableEvent> Create(uint32_t typeId) const {
        auto it = factories_.find(typeId);
        return it != factories_.end() ? it->second() : nullptr;
    }

private:
    std::unordered_map<uint32_t, std::function<std::unique_ptr<ISerializableEvent>()>> factories_;
};

#endif // EVENT_TYPE_REGISTRY_H

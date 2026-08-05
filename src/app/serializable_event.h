// Opt-in serialization contract for events that cross a process or time boundary -- multiplayer
// networking or an event journal (docs/adr/0005 Sec 2). Purely local events (the common case
// today) stay plain structs dispatched through EventManager::Subscribe/Emit/Queue and never touch
// this at all; only an event type that specifically needs to leave the process implements it.
#ifndef SERIALIZABLE_EVENT_H
#define SERIALIZABLE_EVENT_H

#include <cstdint>

#include "byte_stream.h"

class ISerializableEvent {
public:
    virtual ~ISerializableEvent() = default;

    // A stable, compile-time-computed type ID (Fnv1aHash of a human-authored name -- see
    // event_type_id.h), not std::type_index/entt::type_hash -- both are implementation-defined and
    // not guaranteed to match across separately-built binaries or a save/journal file read years
    // later by a different toolchain.
    virtual uint32_t VTypeId() const = 0;

    virtual void VSerialize(ByteWriter &out) const = 0;

    // recordVersion is the schema version the bytes were written with (e.g. by a future event
    // journal), letting an implementation up-convert an old payload layout if its fields change.
    virtual void VDeserialize(ByteReader &in, uint32_t recordVersion) = 0;
};

#endif // SERIALIZABLE_EVENT_H

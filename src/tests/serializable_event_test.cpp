#include "doctest/doctest.h"

#include "app/event_type_id.h"
#include "app/event_type_registry.h"
#include "app/serializable_event.h"

namespace {
    // Mirrors ADR-0005 Sec 2's own EvtData_Destroy_Actor example: a stand-in for "some event that
    // needs to cross the network or be journaled," not a real gameplay event -- nothing in the
    // codebase emits this yet.
    struct FakeDestroyActorEvent : ISerializableEvent {
        static constexpr std::string_view kEventName = "FakeDestroyActorEvent";
        static constexpr uint32_t kTypeId = Fnv1aHash(kEventName);

        uint32_t entityId = 0;

        uint32_t VTypeId() const override { return kTypeId; }

        void VSerialize(ByteWriter &out) const override { out.WriteU32(entityId); }

        void VDeserialize(ByteReader &in, uint32_t /*recordVersion*/) override {
            entityId = in.ReadU32();
        }
    };
}

TEST_CASE("EventTypeRegistry::Create returns nullptr for an unregistered type ID") {
    EventTypeRegistry registry;
    CHECK(registry.Create(FakeDestroyActorEvent::kTypeId) == nullptr);
}

TEST_CASE("EventTypeRegistry::Create returns an instance of the registered type") {
    EventTypeRegistry registry;
    registry.Register<FakeDestroyActorEvent>();

    std::unique_ptr<ISerializableEvent> event = registry.Create(FakeDestroyActorEvent::kTypeId);

    REQUIRE(event != nullptr);
    CHECK(event->VTypeId() == FakeDestroyActorEvent::kTypeId);
}

TEST_CASE("An event survives a full serialize -> registry reconstruct -> deserialize round trip") {
    EventTypeRegistry registry;
    registry.Register<FakeDestroyActorEvent>();

    FakeDestroyActorEvent original;
    original.entityId = 99;

    ByteWriter writer;
    original.VSerialize(writer);

    // The receiving side only has a type ID and bytes -- exactly what Create()/VDeserialize() are
    // for, standing in for what a network message or journal record would hand over.
    std::unique_ptr<ISerializableEvent> reconstructed = registry.Create(original.VTypeId());
    REQUIRE(reconstructed != nullptr);

    ByteReader reader(writer.Bytes());
    reconstructed->VDeserialize(reader, /*recordVersion=*/1);

    auto *reconstructedDestroyEvent = dynamic_cast<FakeDestroyActorEvent *>(reconstructed.get());
    REQUIRE(reconstructedDestroyEvent != nullptr);
    CHECK(reconstructedDestroyEvent->entityId == original.entityId);
}

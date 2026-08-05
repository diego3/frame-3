#include "doctest/doctest.h"

#include "app/byte_stream.h"

TEST_CASE("ByteReader reads back what ByteWriter wrote, in write order") {
    ByteWriter writer;
    writer.WriteU32(42);
    writer.WriteFloat(3.5f);
    writer.WriteU32(7);

    ByteReader reader(writer.Bytes());

    CHECK(reader.ReadU32() == 42);
    CHECK(reader.ReadFloat() == doctest::Approx(3.5f));
    CHECK(reader.ReadU32() == 7);
}

TEST_CASE("Reading past the end of the buffer throws instead of reading garbage") {
    ByteWriter writer;
    writer.WriteU32(1);

    ByteReader reader(writer.Bytes());
    reader.ReadU32();

    CHECK_THROWS_AS(reader.ReadU32(), std::out_of_range);
}

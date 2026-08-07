#include "doctest/doctest.h"

#include <filesystem>

#include "app/io/file_io.h"

namespace {
    const std::string kScratchDir = "test_scratch/file_io_test";
}

TEST_CASE("TryReadWholeFile returns false for a nonexistent file") {
    std::string contents;
    CHECK_FALSE(TryReadWholeFile(kScratchDir + "/does_not_exist.txt", contents));
}

TEST_CASE("WriteWholeFile then TryReadWholeFile round-trips the contents") {
    std::filesystem::remove_all(kScratchDir);
    std::string path = kScratchDir + "/roundtrip.txt";

    WriteWholeFile(path, "hello\nworld\n");

    std::string contents;
    REQUIRE(TryReadWholeFile(path, contents));
    CHECK(contents == "hello\nworld\n");

    std::filesystem::remove_all(kScratchDir);
}

TEST_CASE("WriteWholeFile creates missing parent directories") {
    std::filesystem::remove_all(kScratchDir);
    std::string path = kScratchDir + "/nested/dir/file.txt";

    WriteWholeFile(path, "content");

    CHECK(std::filesystem::exists(path));
    std::filesystem::remove_all(kScratchDir);
}

TEST_CASE("ReadWholeFile throws for a nonexistent file") {
    CHECK_THROWS_AS(ReadWholeFile(kScratchDir + "/nope.txt"), std::runtime_error);
}

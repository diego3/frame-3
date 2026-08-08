// Minimal in-memory byte serialization primitives backing ISerializableEvent::VSerialize/
// VDeserialize (docs/adr/0005 Sec 2). Deliberately small: just enough for a serializable event to
// write and read its own fields. Host byte order, no framing/versioning of the buffer itself -- an
// on-disk journal format (endianness, record framing, schema versioning) is explicitly a separate,
// later decision (see ADR-0005's "Consequences / follow-ups"), not something this type solves.
#ifndef BYTE_STREAM_H
#define BYTE_STREAM_H

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

class ByteWriter {
public:
    void WriteU32(uint32_t value) { AppendBytes(&value, sizeof(value)); }
    void WriteFloat(float value) { AppendBytes(&value, sizeof(value)); }

    const std::vector<uint8_t> &Bytes() const { return bytes_; }

private:
    void AppendBytes(const void *data, size_t size) {
        const uint8_t *bytes = static_cast<const uint8_t *>(data);
        bytes_.insert(bytes_.end(), bytes, bytes + size);
    }

    std::vector<uint8_t> bytes_;
};

class ByteReader {
public:
    explicit ByteReader(const std::vector<uint8_t> &bytes) : bytes_(bytes) {}

    uint32_t ReadU32() {
        uint32_t value;
        ReadBytes(&value, sizeof(value));
        return value;
    }

    float ReadFloat() {
        float value;
        ReadBytes(&value, sizeof(value));
        return value;
    }

private:
    void ReadBytes(void *out, size_t size) {
        if (offset_ + size > bytes_.size()) throw std::out_of_range("ByteReader: read past end of buffer");

        std::memcpy(out, bytes_.data() + offset_, size);
        offset_ += size;
    }

    const std::vector<uint8_t> &bytes_;
    size_t offset_ = 0;
};

#endif // BYTE_STREAM_H

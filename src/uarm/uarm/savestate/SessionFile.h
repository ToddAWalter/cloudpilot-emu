#ifndef _SESSION_FILE_H_
#define _SESSION_FILE_H_

#include <cstddef>
#include <cstdint>
#include <memory>

struct mz_stream_s;

class SessionFile {
   public:
    explicit SessionFile() = default;

    static bool IsSessionFile(size_t size, const void* data);

    uint32_t GetDeviceId() const;
    SessionFile& SetDeviceId(uint32_t deviceId);

    const void* GetMetadata() const;
    size_t GetMetadataSize() const;
    SessionFile& SetMetadata(size_t size, const void* data);

    const void* GetNor() const;
    size_t GetNorSize() const;
    SessionFile& SetNor(size_t size, const void* data);

    const void* GetNand() const;
    size_t GetNandSize() const;
    SessionFile& SetNand(size_t size, const void* data);

    const void* GetMemory() const;
    size_t GetMemorySize() const;
    SessionFile& SetMemory(size_t size, const void* data);

    const void* GetSavestate() const;
    size_t GetSavestateSize() const;
    SessionFile& SetSavestate(size_t size, const void* data);

    size_t GetRamSize();
    SessionFile& SetRamSize(uint32_t size);

    bool Serialize();
    const void* GetSerializedSession() const;
    size_t GetSerializedSessionSize() const;

    bool Deserialize(size_t size, const void* data);

   private:
    bool Write32(uint32_t data);
    uint32_t Read32(bool& success);

    bool AppendToCompressionStream(size_t size, const uint8_t* data, mz_stream_s& stream);
    bool GrowBuffer(mz_stream_s& stream);
    bool Flush(mz_stream_s& stream);

    bool Deserialize_v0();
    bool Deserialize_v1_v2(uint32_t version);

   private:
    uint32_t deviceId{0};

    size_t metadataSize{0};
    const uint8_t* metadata{nullptr};

    size_t norSize{0};
    const uint8_t* nor{nullptr};

    size_t nandSize{0};
    const uint8_t* nand{nullptr};

    size_t memorySize{0};
    const uint8_t* memory{nullptr};

    size_t savestateSize{0};
    const uint8_t* savestate{nullptr};

    size_t serializedSessionSize{0};
    const uint8_t* serializedSession{nullptr};

    size_t bufferSize{0};
    std::unique_ptr<uint8_t[]> buffer;

    uint32_t ramSize{0};

    uint8_t* cursor;
    const uint8_t* ccursor;

   private:
    SessionFile(const SessionFile&) = delete;
    SessionFile(SessionFile&&) = delete;
    SessionFile& operator=(const SessionFile&) = delete;
    SessionFile& operator=(SessionFile&&) = delete;
};

#endif  // _SESSION_FILE_H_
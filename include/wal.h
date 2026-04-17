#pragma once
#include <string>
#include <fstream>
#include <vector>

enum class RecordType  : uint8_t {
    PUT = 0,
    DELETE = 1
};

struct WALRecord{
    RecordType type;
    std::string key;
    std::string value;
};

class WAL{
    uint32_t crc32(const void* data, size_t len) const;
    std::ofstream file_;
    std::string path_;
public:
    explicit WAL(const std::string& path);
    ~WAL();

    void append(RecordType type, const std::string& key, const std::string& value);
    std::vector<WALRecord> recover(const std::string& path);
    void sync();
    void clear();

    const std::string& path() { return path_; };
};
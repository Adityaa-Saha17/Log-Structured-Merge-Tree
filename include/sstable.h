#pragma once
#include <vector>
#include <string>
#include "../include/bloom_filter.h"
#include "../include/sparse_index.h"
#include <optional>
#include <memory>
#include <fstream>
#include <cstdint>
#include <numeric>

struct SSTableEntry {
    std::string key;
    std::string value;
    bool deleted;
};

class SSTableWriter {
private:
    struct IdxEntry {
        std::string key;
        uint64_t offset;
    };
    void flush_data_block();

    std::ofstream file_;
    std::string path_;
    std::vector<IdxEntry> index_;
    std::vector<SSTableEntry> current_block_;
    std::unique_ptr<BloomFilter> bloom_;
    uint64_t write_offset_ = 0;
    static const size_t BLOCK_SIZE = 4096;
public:
    SSTableWriter(const std::string& path, size_t expected_entries = 1000);
    void write(const std::vector<std::pair<std::string, std::string>>& entries, const std::vector<bool>& tombstone);
    void finish();
};

class SSTableReader {
private:
    SparseIndex sparse_index_;
    std::string path_;
    std::unique_ptr<BloomFilter> bloom_;
public:
    explicit SSTableReader(const std::string& path);
    std::optional<SSTableEntry> get(const std::string& key);
    std::vector<SSTableEntry> range(const std::string& start, const std::string& end);
    std::vector<SSTableEntry> read_block_at(uint64_t offset);

    bool bloom_maybe_contains(const std::string& key) const;

    const SparseIndex& index() const { return sparse_index_; }
    const std::string& min_key() const { return sparse_index_.min_key(); }
    const std::string& max_key() const {  return sparse_index_.max_key(); }
    const std::string& path() const { return path_; }
};
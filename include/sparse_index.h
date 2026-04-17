#pragma once
#include <vector>
#include <string>
#include <optional>
#include <cstdint>

struct IndexEntry{
    std::string key;
    uint64_t offset;
    uint64_t size;
};

class SparseIndex{
private:
    std::vector<IndexEntry> entries_;
    std::string max_key_;
public:
    void add(const std::string& first_key, uint64_t offset, uint64_t size);

    std::optional<IndexEntry> find_block(const std::string& key) const;
    std::vector<IndexEntry> range_block(const std::string& start, const std::string& end) const;

    size_t block_count() const { return entries_.size(); }
    bool empty() const { return entries_.empty(); }

    const std::string& min_key() const { return entries_.front().key; }
    const std::string& max_key() const { return max_key_; }
    void set_max_key(const std::string& k) { max_key_ = k; }
};
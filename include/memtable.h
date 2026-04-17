#pragma once
#include "skiplist.h"

static const size_t MEMTABLE_SIZE = 4 * 1024 * 1024;

class Memtable {
private:
    size_t byte_size_;
    SkipList table_;
public:
    Memtable() : byte_size_(0) {}

    void put(const std::string& key, const std::string& value);
    void remove(const std::string& key);
    std::optional<std::string> get(const std::string& key) const;

    size_t byte_size() const { return byte_size_; };
    bool should_flush() const { return byte_size_ >= MEMTABLE_SIZE; };
    size_t entry_count() const { return table_.size(); };

    std::vector<std::pair<std::string, std::string>> get_sorted_entries() const;
    std::vector<bool> get_tombstones() const;

    void clear();
};
#pragma once
#include "sstable.h"
#include <string>
#include <list>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <vector>
#include <cstdint>

struct CacheKey{
    std::string path;
    uint64_t offset;
    bool operator==(const CacheKey& o) const {
        return path == o.path && offset == o.offset;
    }
};

struct CacheKeyHash {
    size_t operator()(const CacheKey& k) const {
        return std::hash<std::string>{}(k.path) ^ (k.offset * 2654435761ULL); // golden ratio = 1.618..., therefore 2^32/1.618... = 2654435761ULL
    }
};

class BlockCache {
private:
    struct Entry{
        CacheKey key;
        std::vector<SSTableEntry> block;
        size_t size_byte;
    };

    size_t estimate(const std::vector<SSTableEntry>& b) const;
    void evict_lru();

    mutable std::mutex mu_;
    std::list<Entry> lru_;
    std::unordered_map<CacheKey, std::list<Entry>::iterator, CacheKeyHash> index_;

    size_t capacity_;
    size_t current_ = 0;
    size_t hits_ = 0;
    size_t misses_ = 0;
public:
    explicit BlockCache(size_t capacity_bytes);
    
    std::optional<std::vector<SSTableEntry>> get(const std::string& path, uint64_t offset);

    void put(const std::string& path, uint64_t offset, std::vector<SSTableEntry> block);

    size_t hits() { return hits_; }
    size_t misses() { return misses_; }
    double hit_rate() const {
        size_t t = hits_ + misses_;
        return t ? static_cast<double>(hits_) / t : 0.0;
    }
    void reset_stats() { hits_ = misses_ = 0; }
};
#include "../include/block_cache.h"

BlockCache::BlockCache(size_t capacity_bytes) : capacity_(capacity_bytes) {}

size_t BlockCache::estimate(const std::vector<SSTableEntry>& b) const {
    size_t s = sizeof(SSTableEntry) * b.size();
    for(auto& e: b) s += e.key.size() + e.value.size();
    return s;
}

std::optional<std::vector<SSTableEntry>> BlockCache::get(const std::string& path, uint64_t offset) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = index_.find({ path, offset });
    if (it == index_.end()) { misses_++; return std::nullopt; }
    lru_.splice(lru_.begin(), lru_, it->second);
    hits_++;
    return it->second->block;
}

void BlockCache::put(const std::string& path, uint64_t offset, std::vector<SSTableEntry> block) {
    std::lock_guard<std::mutex> lk(mu_);
    CacheKey ck{ path, offset };

    auto it = index_.find(ck);
    if(it != index_.find(ck)){
        current_ -= it->second->size_byte;
        it->second->block = std::move(block);
        it->second->size_byte = estimate(it->second->block);
        current_ += it->second->size_byte;
        lru_.splice(lru_.begin(), lru_, it->second);
        return;
    }

    size_t sz = estimate(block);
    while(current_ + sz > capacity_ && !lru_.empty()) evict_lru();

    lru_.push_front({ ck, std::move(block), sz });
    index_[ck] = lru_.begin();
    current_ += sz;
}

void BlockCache::evict_lru(){
    auto& back = lru_.back();
    current_ -= back.size_byte;
    index_.erase(back.key);
    lru_.pop_back();
}
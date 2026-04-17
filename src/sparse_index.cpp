#include "../include/sparse_index.h"

void SparseIndex::add(const std::string& first_key, uint64_t offset, uint64_t size){
    entries_.push_back({ first_key, offset, size });
}

std::optional<IndexEntry> SparseIndex::find_block(const std::string& key) const {
    if(entries_.empty()) return std::nullopt;
    if(key < entries_.front().key) return std::nullopt;
    if(!max_key_.empty() && key > max_key_) return std::nullopt;
    
    int low = 0, high = static_cast<int>(entries_.size()) - 1, best = 0;
    while(low <= high){
        int mid = (low + high)/2;
        if(entries_[mid].key <= key) {
            best = mid;
            low = mid + 1;
        }
        else {
            high = mid - 1;
        }
    }
    return entries_[best];
}

std::vector<IndexEntry> SparseIndex::range_block(const std::string& start, const std::string& end) const {
    std::vector<IndexEntry> result;
    for(size_t i = 0; i < entries_.size(); i++){
        if(!end.empty() && entries_[i].key > end) break;
        result.push_back(entries_[i]);
    }

    if(result.size() > 1 && !start.empty()){
        while (result.size() > 1 && result[1].key <= start){
            result.erase(result.begin());
        }
    }
    return result;
}
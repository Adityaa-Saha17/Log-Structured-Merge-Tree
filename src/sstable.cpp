#include "../include/sstable.h"
#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <cstring>

// Writer

SSTableWriter::SSTableWriter(const std::string& path, size_t expected_entries) : path_(path) {
    file_.open(path, std::ios::binary | std::ios::trunc);
    if(!file_.is_open()){
        throw std::runtime_error("Cannot create SSTable : " + path);
    }
    bloom_ = std::make_unique<BloomFilter>(expected_entries > 0 ? expected_entries : 1, 0.01);
}

void SSTableWriter::write(const std::vector<std::pair<std::string, std::string>>& entries, const std::vector<bool>& tombstone){
    for(size_t i = 0; i < entries.size(); i++){
        SSTableEntry e{
            entries[i].first, entries[i].second, i < tombstone.size() ? tombstone[i] : false
        };
        bloom_->insert(e.key);
        current_block_.push_back(std::move(e));
        size_t est = 0;
        for(auto& ce :  current_block_){
            est += ce.key.size() + ce.value.size() + 9;
        }
        if(est >= BLOCK_SIZE) flush_data_block();
    }
    if (!current_block_.empty()) flush_data_block();
}

void SSTableWriter::flush_data_block(){
    if(current_block_.empty()) return;
    index_.push_back({ current_block_.front().key, write_offset_ });
    for(auto& e : current_block_){
        uint8_t type = e.deleted ? 2u : 1u;
        uint32_t klen = static_cast<uint32_t>(e.key.size());
        uint32_t vlen = static_cast<uint32_t>(e.value.size());
        file_.write(reinterpret_cast<const char*>(&type), 1);
        file_.write(reinterpret_cast<const char*>(&klen), 4);
        file_.write(reinterpret_cast<const char*>(&vlen), 4);
        file_.write(e.key.data(), klen);
        file_.write(e.value.data(), vlen);
        write_offset_ += 1 + 4 + 4 + klen + vlen;
    }
    current_block_.clear();
}

void SSTableWriter::finish(){
    uint64_t index_offset = write_offset_;
    uint32_t index_count = static_cast<uint32_t>(index_.size());
    file_.write(reinterpret_cast<const char*>(&index_count), 4);
    uint64_t cur = index_offset + 4;
    for(auto& ie : index_){
        uint32_t klen = static_cast<uint32_t>(ie.key.size());
        file_.write(reinterpret_cast<const char*>(&klen), 4);
        file_.write(ie.key.data(), klen);
        file_.write(reinterpret_cast<const char*>(&ie.offset), 8);
        cur += 4 + klen + 8;
    }
    uint64_t bloom_offset = cur;
    auto bloom_bytes = bloom_->serialize();
    uint32_t bloom_len = static_cast<uint32_t>(bloom_bytes.size());
    file_.write(reinterpret_cast<const char*>(&bloom_len), 4);
    file_.write(reinterpret_cast<const char*>(bloom_bytes.data()), bloom_len);
    uint64_t magic = 0xDB1234DB1234DB12ULL;
    file_.write(reinterpret_cast<const char*>(&index_offset), 8);
    file_.write(reinterpret_cast<const char*>(&bloom_offset), 8);
    file_.write(reinterpret_cast<const char*>(&magic),        8);
    file_.flush();
    file_.close();
}

// Reader

SSTableReader::SSTableReader(const std::string& path) : path_(path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if(!in.is_open()){
        throw std::runtime_error("Cannot open SSTable : " + path);
    }

    in.seekg(-24, std::ios::end);
    uint64_t index_offset, bloom_offset, magic;
    in.read(reinterpret_cast<char*>(&index_offset), 8);
    in.read(reinterpret_cast<char*>(&bloom_offset), 8);
    in.read(reinterpret_cast<char*>(&magic), 8);
    if (magic != 0xDB1234DB1234DB12ULL){
        throw std::runtime_error("Corrupt Data : " + path);
    }
    
    in.seekg(static_cast<std::streamoff>(index_offset));
    uint32_t index_count;
    in.read(reinterpret_cast<char*>(&index_count), 4);
    uint64_t prev_offset = 0;
    std::string last_key;
    for(uint32_t i = 0; i < index_count; i++){
        uint32_t klen;
        uint64_t offset;
        in.read(reinterpret_cast<char*>(&klen), 4);
        std::string key(klen, '\0');
        in.read(key.data(), klen);
        in.read(reinterpret_cast<char*>(&offset), 8);
        uint64_t sz = (i > 0) ? (offset - prev_offset) : 0;
        if (i > 0) sparse_index_.add(last_key, prev_offset, sz);
        prev_offset = offset;
        last_key = key;
    }

    if(index_count > 0) {
        sparse_index_.add(last_key, prev_offset, index_offset - prev_offset);
        sparse_index_.set_max_key(last_key);
    }

    in.seekg(static_cast<std::streamoff>(bloom_offset));
    uint32_t bloom_len;
    in.read(reinterpret_cast<char*>(&bloom_len), 4);
    std::vector<uint8_t> bloom_byte(bloom_len);
    in.read(reinterpret_cast<char*>(bloom_byte.data()), bloom_len);
    bloom_ = std::make_unique<BloomFilter>(bloom_byte);
}

std::vector<SSTableEntry> SSTableReader::read_block_at(uint64_t offset){
    std::ifstream in(path_, std::ios::binary);
    if(!in.is_open()) return {};
    in.seekg(static_cast<std::streamoff>(offset));

    std::vector<SSTableEntry> entries;
    std::string prev_key;
    while(in.peek() != EOF){
        uint8_t type; uint32_t klen, vlen;
        if(!in.read(reinterpret_cast<char*>(&klen), 4)) break;
        if(!in.read(reinterpret_cast<char*>(&vlen), 4)) break;
        if(!in.read(reinterpret_cast<char*>(&type), 1)) break;
        if(klen > 65536 || vlen > 1048576) break;
        std::string key(klen, '\0'), value(vlen, '\0');
        if(!in.read(key.data(), klen)) break;
        if(!in.read(value.data(), vlen)) break;
        if(!prev_key.empty() && key < prev_key) break;
        prev_key = key;
        entries.push_back({key, value, type == 2});
    }
    return entries;
}

std::optional<SSTableEntry> SSTableReader::get(const std::string& key){
    auto blk = sparse_index_.find_block(key);
    if(!blk) return std::nullopt;

    for(auto &e : read_block_at(blk->offset)){
        if(e.key == key) return e;
    }
    return std::nullopt;
}

std::vector<SSTableEntry> SSTableReader::range(const std::string& start, const std::string& end){
    std::vector<SSTableEntry> result;
    for(auto &blk : sparse_index_.range_block(start, end)){
        for(auto &e : read_block_at(blk.offset)){
            if(!start.empty() && e.key < start) continue;
            if(!end.empty() && e.key > end) break;
            result.push_back(e);
        }
    }
    return result;
}

bool SSTableReader::bloom_maybe_contains(const std::string& key) const {
    return bloom_ ? bloom_->maybe_contains(key) : true;
}
#include "../include/bloom_filter.h"

BloomFilter::BloomFilter(size_t n, double fp_rate){
    if(fp_rate <= 0 || fp_rate >= 1){
        throw std::invalid_argument("fp_rate must be between (0, 1)");
    }
    if(n == 0) n = 1;

    double ln2 = std::log(2.0);
    m_ = static_cast<size_t>(std::ceil(-static_cast<double>(n) * std::log(fp_rate)/ (ln2 * ln2)));
    k_ = static_cast<size_t>((std::round(static_cast<double>(m_)/n) * ln2));
    k_ = std::max<size_t>(k_, 1);
    m_ = std::max<size_t>(m_, 1);

    bits_.assign(m_, false);
}

BloomFilter::BloomFilter(const std::vector<uint8_t>& data){
    if(data.size() < 8){
        throw std::runtime_error("Corrupt Bloom filter bytes");
    }

    std::memcpy(&m_, data.data(), 4);
    std::memcpy(&k_, data.data() + 4, 4);

    bits_.assign(m_, false);
    for(size_t i = 0; i < m_; i++){
        size_t byte_idx = 8 + i/8;
        size_t bit_idx = i % 8;
        if(byte_idx < data.size()){
            bits_[i] = (data[byte_idx] >> bit_idx) & 1;
        }
    }
}

uint32_t BloomFilter::hash1(const std::string& key) const {
    uint32_t h = 0x9747b28c;
    for(unsigned char c : key){
        h ^= c;
        h *= 0x5bd1e995;
        h ^= h >> 15;
    }
    return h;
}

uint32_t BloomFilter::hash2(const std::string& key) const {
    uint32_t h = 0xc4ceb9fe;
    for (unsigned char c : key) {
        h ^= c;
        h *= 0x27d4eb2f;
        h ^= h >> 15;
    }
    return h;
}

size_t BloomFilter::nth_hash(uint32_t hash1, uint32_t hash2, int i) const {
    return (static_cast<uint64_t>(hash1) + static_cast<uint64_t>(i) * hash2) % m_;
}

void BloomFilter::insert(const std::string& key){
    uint32_t h1 = hash1(key);
    uint32_t h2 = hash2(key);
    for(int i = 0; i < static_cast<int>(k_); i++){
        bits_[nth_hash(h1, h2, i)] = true;
    }
}

bool BloomFilter::maybe_contains(const std::string& key) const{
    uint32_t h1 = hash1(key);
    uint32_t h2 = hash2(key);
    for(int i = 0; i < static_cast<int>(k_); i++){
        if(!bits_[nth_hash(h1, h2, i)]) return false;
    }
    return true;
}

std::vector<uint8_t> BloomFilter::serialize() const {
    size_t num_byte = (m_ + 7) / 8;
    std::vector<uint8_t> out(8 + num_byte, 0);
    std::memcpy(out.data(), &m_, 4);
    std::memcpy(out.data() + 4, &k_, 4);
    for(size_t i = 0; i < m_; i++){
        if(bits_[i]){
            out[8 + i/8] |= static_cast<uint8_t>(1u << (i % 8));
        }
    }
    return out;
}

double BloomFilter::current_fp_rate() const {
    size_t set_bits = 0;
    for(bool b : bits_){
        if(b) set_bits++;
    }
    double fill = static_cast<double>(set_bits) / static_cast<double>(m_);
    return std::pow(fill, static_cast<double>(k_));
}
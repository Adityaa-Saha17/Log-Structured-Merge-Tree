#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>

class BloomFilter{
private:
    size_t m_;
    size_t k_;
    std::vector<bool> bits_;

    uint32_t hash1(const std::string& key) const;
    uint32_t hash2(const std::string& key) const;
    size_t nth_hash(uint32_t hash1, uint32_t hash2, int i) const;
public:
    BloomFilter(size_t n, double fp_rate);
    explicit BloomFilter(const std::vector<uint8_t>& serialized);

    void insert(const std::string& key);
    bool maybe_contains(const std::string& key) const;

    std::vector<uint8_t> serialize() const;

    double current_fp_rate() const;
    size_t bit_count() const { return m_; }
    size_t hash_count() const { return k_; }
};
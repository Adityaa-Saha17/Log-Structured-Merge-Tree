#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <optional>
#include <stdlib.h>

static const int MAX_LEVEL = 12;
static const float P = 0.5f;

struct SkipNode{
    std::string key;
    std::string value;
    bool tombstone;
    std::vector<SkipNode*> forward;

    SkipNode(const std::string& k, const std::string& v, int level, int del = false)
        : key(k), value(v), tombstone(del), forward(level + 1, nullptr) {}
};

class SkipList {
private:
    int current_level_;
    SkipNode* head_;
    int random_level();
    size_t size_;
public:
    SkipList();
    ~SkipList();

    void put(const std::string& key, const std::string& value);
    void remove(const std::string& key);
    std::optional<std::string> get(const std::string& key) const;

    size_t size() const { return size_; };
};
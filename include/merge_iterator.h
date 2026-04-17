#pragma once
#include <vector>
#include <string>
#include <queue>
#include <memory>
#include <functional>

struct IterEntry{
    std::string key;
    std::string value;
    bool deleted;
    int source_id;

    bool operator>(const IterEntry& o)const {
        if(key != o.key) return key > o.key;
        return source_id > o.source_id;
    }
};

class EntryStream {
private:
    std::vector<IterEntry> entries_;
    size_t pos_ = 0;
public:
    EntryStream(std::vector<IterEntry> entries, int source_id);
    bool has_next() const;
    const IterEntry& peek() const;
    IterEntry pop();
};

class MergeIterator {
private:
    using Pair = std::pair<IterEntry, int>;
    using MinHeap = std::priority_queue<Pair, std::vector<Pair>, std::function<bool(const Pair&, const Pair&)>>;
    void advance_stream(int idx);

    std::vector<std::shared_ptr<EntryStream>> streams_;
    MinHeap heap_;
    std::string start_, end_;
public:
    MergeIterator(std::vector<std::shared_ptr<EntryStream>> streams, const std::string& start = "", const std::string& end = "");
    bool has_next() const;
    IterEntry next();
};
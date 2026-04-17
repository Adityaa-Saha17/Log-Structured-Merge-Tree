#pragma once
#include <string>
#include <unordered_set>
#include <mutex>

class CompactionGuard {
private:
    mutable std::mutex mu_;
    std::unordered_set<std::string> files_;
    std::unordered_set<int> levels_;
public:
    void mark_compacting(const std::string& path);
    void unmark_compacting(const std::string& path);
    bool is_compacting(const std::string& path) const;

    void mark_level(int level);
    void unmark_level(int level);
    bool level_has_compaction(int level) const;
};
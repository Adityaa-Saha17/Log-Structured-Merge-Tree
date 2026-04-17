#include "../include/compaction_guard.h"

void CompactionGuard::mark_compacting(const std::string& path){
    std::lock_guard<std::mutex> lk(mu_);
    files_.insert(path);
}

void CompactionGuard::unmark_compacting(const std::string& path){
    std::lock_guard<std::mutex> lk(mu_);
    files_.erase(path);
}

bool CompactionGuard::is_compacting(const std::string& path) const {
    std::lock_guard<std::mutex> lk(mu_);
    return files_.count(path) > 0;
}

void CompactionGuard::mark_level(int level){
    std::lock_guard<std::mutex> lk(mu_);
    levels_.insert(level);
}

void CompactionGuard::unmark_level(int level){
    std::lock_guard<std::mutex> lk(mu_);
    levels_.erase(level);
}

bool CompactionGuard::level_has_compaction(int level) const {
    std::lock_guard<std::mutex> lk(mu_);
    return levels_.count(level) > 0;
}
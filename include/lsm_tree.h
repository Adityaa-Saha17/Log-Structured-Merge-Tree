#pragma once
#include "memtable.h"
#include "wal.h"
#include "sstable.h"
#include "sparse_index.h"
#include "compaction_guard.h"
#include "merge_iterator.h"
#include "block_cache.h"
#include <string>
#include <vector>
#include <memory>
#include <optional>

struct Level{
    int number;
    std::vector<std::shared_ptr<SSTableReader>> files;
    size_t max_bytes;
};

class LSM_Tree{
private:
    void init_levels();
    void recover();
    void flush_memtable();
    void maybe_compact();
    void compact_levels(Level& src, Level& dst);
    std::vector<std::shared_ptr<SSTableReader>> merge_and_write(std::vector<std::shared_ptr<SSTableReader>> inputs, int dst_levels);
    std::string db_path_;
    std::unique_ptr<Memtable> memtable_;
    std::unique_ptr<WAL> wal_;
    std::vector<Level> levels_;
    int sst_counter_ = 0;

    std::shared_ptr<BlockCache> block_cache_;
    std::shared_ptr<CompactionGuard> compaction_guard_;
    
    mutable size_t reads_total_ = 0;
    mutable size_t bloom_filtered_ = 0;
    mutable size_t cache_hits_ = 0;
    mutable size_t disk_reads_ = 0;
    mutable size_t compaction_routed_ = 0;
    size_t writes_total_ = 0;
    size_t flushes_ = 0;
    size_t compactions_ = 0;

public:
    explicit LSM_Tree(const std::string& db_path);
    ~LSM_Tree() {}

    void put(const std::string& key, const std::string& value);
    void remove(const std::string& key);
    std::optional<std::string> get(const std::string& key);
    std::vector<std::pair<std::string, std::string>> scan(const std::string& start, const std::string& end);
    void print_stats() const;
    void close();
};
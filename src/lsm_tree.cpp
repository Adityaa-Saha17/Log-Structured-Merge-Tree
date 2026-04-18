#include "../include/lsm_tree.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <queue>
#include <stdexcept>

LSM_Tree::LSM_Tree(const std::string& db_path) : db_path_(db_path){
    std::filesystem::create_directory(db_path_);
    init_levels();

    block_cache_ = std::make_shared<BlockCache>(64*1024*1024);
    compaction_guard_ = std::make_shared<CompactionGuard>();

    std::vector<std::string> sst_files;
    for(auto& p : std::filesystem::directory_iterator(db_path)){
        if(p.path().extension() == ".sst"){
            sst_files.push_back(p.path().string());
        }
    }
    std::sort(sst_files.begin(), sst_files.end());
    for(auto& f : sst_files){
        try {
            levels_[0].files.push_back(std::make_shared<SSTableReader>(f));
        }
        catch (...){}
        sst_counter_++;
    }

    wal_ = std::make_unique<WAL>(db_path_ + "/wal.log");
    memtable_ = std::make_unique<Memtable>();
    recover();
}


void LSM_Tree::close(){
    if (memtable_ && memtable_->entry_count() > 0) flush_memtable();
}

void LSM_Tree::init_levels(){
    levels_ = {
        Level {0, {}, 4ULL * 1024 * 1024},
        Level {1, {}, 10ULL * 1024 * 1024},
        Level {2, {}, 100ULL * 1024 * 1024},
        Level {3, {}, 1000ULL * 1024 * 1024},
        Level {4, {}, 10000ULL * 1024 * 1024},
    };
}

void LSM_Tree::recover(){
    auto records = wal_->recover(db_path_ + "/wal.log");
    std::cout << "[LSM] Recovering " << records.size() << " records from WAL...\n";
    for(auto& r : records){
        if(r.type == RecordType::PUT) {
            std::cout << "[LSM] Recovering PUT: " << r.key << " -> " << r.value << "\n";
            memtable_->put(r.key, r.value);
        }
        else {
            std::cout << "[LSM] Recovering DELETE: " << r.key << "\n";
            memtable_->remove(r.key);
        }
    }
    if(!records.empty()){
        std::cout << "[LSM] Successfully recovered " << records.size() << " records from WAL\n";
    }
}

void LSM_Tree::put(const std::string& key, const std::string& value){
    wal_->append(RecordType::PUT, key, value);
    memtable_->put(key, value);
    writes_total_++;
    if(memtable_->should_flush()) flush_memtable();
}

void LSM_Tree::remove(const std::string& key){
    wal_->append(RecordType::DELETE, key, "");
    memtable_->remove(key);
    writes_total_++;
    if(memtable_->should_flush()) flush_memtable();
}

void LSM_Tree::flush_memtable() {
    if(memtable_->entry_count() == 0) return;
    auto entries = memtable_->get_sorted_entries();
    auto tombstones = memtable_->get_tombstones();
    std::string path = db_path_ + "/sst_" + std::to_string(sst_counter_++) + ".sst";
    SSTableWriter writer(path, entries.size());
    writer.write(entries, tombstones);
    writer.finish();
    wal_->sync();
    wal_->clear();
    wal_ = std::make_unique<WAL>(db_path_ + "/wal.log");
    memtable_->clear();
    levels_[0].files.push_back(std::make_shared<SSTableReader>(path));
    flushes_++;
    maybe_compact();
}

std::optional<std::string> LSM_Tree::get(const std::string& key){
    reads_total_++;
    if(auto v = memtable_->get(key)) return v;

    for(auto& level : levels_){
        auto files = level.files;
        std::reverse(files.begin(), files.end());

        for(auto& sst : files){
            if(compaction_guard_->is_compacting(sst->path())){
                compaction_routed_++;
                continue;
            }

            if(!sst->bloom_maybe_contains(key)){
                bloom_filtered_++;
                continue;
            }

            auto blk = sst->index().find_block(key);
            if (!blk) continue;

            auto cached = block_cache_->get(sst->path(), blk->offset);
            std::vector<SSTableEntry> entries;
            if(cached){
                entries = std::move(*cached);
                cache_hits_++;
            }
            else {
                entries = sst->read_block_at(blk->offset);
                block_cache_->put(sst->path(), blk->offset, entries);
                disk_reads_++;
            }
            for(auto& e : entries){
                if(e.key == key){
                    if(e.deleted) return std::nullopt;
                    return e.value;
                }
            }
        }
    }
    return std::nullopt;
}

std::vector<std::pair<std::string, std::string>> LSM_Tree::scan(const std::string& start, const std::string& end) {
    std::vector<std::shared_ptr<EntryStream>> streams;
    int src = 0;

    {
        auto entries = memtable_->get_sorted_entries();
        auto tombstones = memtable_->get_tombstones();
        std::vector<IterEntry> items;
        items.reserve(entries.size());
        for(size_t i = 0; i < entries.size(); i++){
            items.push_back({ entries[i].first, entries[i].second, tombstones[i], src});
        }
        streams.push_back(std::make_shared<EntryStream>(std::move(items), src++));
    }

    for(auto& level : levels_){
        for(auto it = level.files.rbegin(); it != level.files.rend(); it++){
            auto &sst =  *it;
            if(compaction_guard_->is_compacting(sst->path())) {
                src++;
                continue;
            }
            auto blocks = sst->index().range_block(start, end);
            std::vector<IterEntry> items;
            for(auto& blk : blocks){
                auto cached = block_cache_->get(sst->path(), blk.offset);
                std::vector<SSTableEntry> block_entries;
                if(cached){
                    block_entries = std::move(*cached);
                    cache_hits_++;
                }
                else {
                    block_entries = sst->read_block_at(blk.offset);
                    block_cache_->put(sst->path(), blk.offset, block_entries);
                    disk_reads_++;
                }
                for(auto& e : block_entries){
                    items.push_back({ e.key, e.value, e.deleted, src });
                }
            }
            streams.push_back(std::make_shared<EntryStream>(std::move(items), src++));
        }
    }

    MergeIterator it(streams, start, end);
    std::vector<std::pair<std::string, std::string>> results;
    while(it.has_next()){
        auto e = it.next();
        if(!e.deleted) results.emplace_back(e.key, e.value);
    }
    return results;
}

void LSM_Tree::maybe_compact(){
    for(size_t i = 0; i + 1 < levels_.size(); i++){
        size_t total = 0;
        for(auto f : levels_[i].files){
            if(std::filesystem::exists(f->path())){
                total += std::filesystem::file_size(f->path());
            }
        }
        if(total > levels_[i].max_bytes || levels_[i].files.size() > 4){
            compact_levels(levels_[i], levels_[i+1]);
            compactions_++;
        }
    }
}

void LSM_Tree::compact_levels(Level& src, Level& dst){
    if(src.files.empty()) return;
    auto& trigger = src.files.front();
    std::string low = trigger->min_key();
    std::string high = trigger->max_key();

    std::vector<std::shared_ptr<SSTableReader>> inputs = {trigger};
    std::vector<std::shared_ptr<SSTableReader>> survivors;
    for(auto& f : dst.files){
        if(f->max_key() >= low && f->min_key() <= high){
            inputs.push_back(f);
        }
        else{
            survivors.push_back(f);
        }
    }

    for(auto& f : inputs) compaction_guard_->mark_compacting(f->path());
    auto new_files = merge_and_write(inputs, dst.number);
    for(auto& f : inputs) compaction_guard_->unmark_compacting(f->path());
    
    src.files.erase(src.files.begin());
    dst.files = survivors;
    dst.files.insert(dst.files.end(), new_files.begin(), new_files.end());
    std::sort(dst.files.begin(), dst.files.end(), [](auto& a, auto& b) { return a->min_key() < b->min_key(); });
    for(auto& f : inputs){
        if(std::filesystem::exists(f->path())){
            std::filesystem::remove(f->path());
        }
    }
}


std::vector<std::shared_ptr<SSTableReader>> LSM_Tree::merge_and_write(std::vector<std::shared_ptr<SSTableReader>> inputs, int dst_level){
    struct HeapEntry {
        SSTableEntry entry;
        int file_idx;
        size_t pos;
        bool operator>(const HeapEntry& o) const { return entry.key > o.entry.key; }
    };

    std::vector<std::vector<SSTableEntry>> bufs(inputs.size());
    for(size_t i = 0; i < inputs.size(); i++){
        bufs[i] = inputs[i]->range("", "");
    }
    std::priority_queue<HeapEntry, std::vector<HeapEntry>, std::greater<HeapEntry>> heap;
    for(size_t i = 0; i < bufs.size(); i++){
        if(!bufs[i].empty()){
            heap.push({ bufs[i][0], static_cast<int>(i), 0 });
        }
    }
    std::vector<std::shared_ptr<SSTableReader>> out;
    std::unique_ptr<SSTableWriter> writer;
    std::string cur_path;
    size_t cur_size = 0;
    std::vector<std::pair<std::string, std::string>> blk;
    std::vector<bool> tombs;
    std::string last_key;
    bool is_last = (dst_level >= static_cast<int>(levels_.size()) - 1);
    static const size_t OUT_SIZE = 2 * 1024 * 1024;

    auto flush = [&] () {
        if(!writer || blk.empty()) return;
        writer->write(blk, tombs);
        writer->finish();
        out.push_back(std::make_shared<SSTableReader>(cur_path));
        writer.reset();
        blk.clear();
        tombs.clear();
        cur_size = 0;
    };

    while (!heap.empty()) {
        auto top = heap.top(); heap.pop();
        size_t nxt = top.pos + 1;
        if (nxt < bufs[top.file_idx].size())
            heap.push({ bufs[top.file_idx][nxt], top.file_idx, nxt });
        if (top.entry.key == last_key) continue;
        last_key = top.entry.key;
        if (top.entry.deleted && is_last) continue;
        if (!writer) {
            cur_path = db_path_ + "/sst_" + std::to_string(sst_counter_++) + ".sst";
            writer = std::make_unique<SSTableWriter>(cur_path, 1000);
        }
        blk.push_back({ top.entry.key, top.entry.value });
        tombs.push_back(top.entry.deleted);
        cur_size += top.entry.key.size() + top.entry.value.size();
        if (cur_size >= OUT_SIZE) flush();
    }
    flush();
    return out;
}

void LSM_Tree::print_stats() const {
    std::cout << "\n=== LSM Engine Stats ===\n"
              << "Writes:             " << writes_total_      << "\n"
              << "Reads:              " << reads_total_       << "\n"
              << "  Bloom filtered:   " << bloom_filtered_    << "\n"
              << "  Cache hits:       " << cache_hits_        << "\n"
              << "  Disk reads:       " << disk_reads_        << "\n"
              << "  Compaction skip:  " << compaction_routed_ << "\n";
    if (reads_total_ > 0)
        printf("  Bloom hit rate:   %.1f%%\n  Cache hit rate:   %.1f%%\n",
               100.0 * bloom_filtered_ / reads_total_,
               100.0 * cache_hits_     / reads_total_);
    std::cout << "Flushes:            " << flushes_      << "\n"
              << "Compactions:        " << compactions_  << "\n"
              << "Block cache:        "
              << block_cache_->hits() << " hits / "
              << block_cache_->misses() << " misses ("
              << static_cast<int>(block_cache_->hit_rate() * 100) << "%)\n";
    std::cout << "Level file counts:\n";
    for (auto& l : levels_)
        std::cout << "  L" << l.number << ": " << l.files.size() << " files\n";
}

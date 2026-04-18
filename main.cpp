#include "./include/lsm_tree.h"
#include "./include/bloom_filter.h"
#include <iostream>
#include <cassert>
#include <chrono>
#include <random>
#include <string>
#include <filesystem>

using Clock = std::chrono::high_resolution_clock;

struct Timer {
    Clock::time_point start = Clock::now();
    double stop() { return std::chrono::duration<double, std::milli>(Clock::now() - start).count(); }
};

static double elapsed_ms(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}

static std::string rand_str(std::mt19937& rng, size_t len = 8) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::string s(len, ' ');
    for (auto& c : s) c = chars[rng() % (sizeof(chars) - 1)];
    return s;
}

void test_basic_correctness() {
    std::cout << "Test 1: Basic correctness \n";
    std::filesystem::remove_all("/tmp/lsm_t1");
    LSM_Tree db("/tmp/lsm_t1");

    db.put("apple",  "fruit");
    db.put("banana", "yellow");
    db.put("cherry", "red");

    assert(db.get("apple")  == "fruit");
    assert(db.get("banana") == "yellow");
    assert(db.get("cherry") == "red");
    assert(!db.get("durian").has_value());
    std::cout << "  PASS: put / get / missing key\n";

    db.put("apple", "green_fruit");
    assert(db.get("apple") == "green_fruit");
    std::cout << "  PASS: overwrite / update\n";

    db.remove("banana");
    assert(!db.get("banana").has_value());
    std::cout << "  PASS: delete (tombstone)\n";

    db.put("ant", "insect");
    db.put("ape", "animal");
    db.put("arc", "shape");
    auto results = db.scan("ant", "arc");
    assert(results.size() == 4);
    std::cout << "  PASS: range scan (" << results.size() << " results: ";
    for (auto& [k,v] : results) std::cout << k << " ";
    std::cout << ")\n\n";
}

void test_persistence() {
    std::cout << "Test 2: Persistence / WAL recovery\n";
    std::filesystem::remove_all("/tmp/lsm_t2");
    {
        LSM_Tree db("/tmp/lsm_t2");
        db.put("k2", "v2");
        db.put("k1", "v1");
        db.put("k3", "v3");
    }
    {
        LSM_Tree db("/tmp/lsm_t2");
        assert(db.get("k2") == "v2");
        assert(db.get("k1") == "v1");
        assert(db.get("k3") == "v3");
        std::cout << "  PASS: all 3 keys recovered after restart\n\n";
    }
}

void test_bloom_filter() {
    std::cout << "Test 3: Bloom filter false positive benchmark\n";
    printf("  %-10s %-10s %-12s %-12s\n", "Target FP", "Bits/key", "Observed FP", "Size");
    // printf("  %-10s %-10s %-12s %-12s\n",
    //        "----------", "--------", "-----------", "----");

    for (double fp_target : { 0.10, 0.05, 0.01, 0.005, 0.001 }) {
        const size_t N = 10000;   // 10k keys per filter — light on memory
        BloomFilter bf(N, fp_target);

        for (size_t i = 0; i < N; i++)
            bf.insert("key_" + std::to_string(i));

        size_t fp_count = 0;
        for (size_t i = N; i < 2 * N; i++)
            if (bf.maybe_contains("key_" + std::to_string(i)))
                fp_count++;

        double observed     = static_cast<double>(fp_count) / N;
        double bits_per_key = static_cast<double>(bf.bit_count()) / N;
        size_t size_bytes   = bf.serialize().size();

        printf("  %-10.3f %-10.1f %-12.4f %zu B\n",
               fp_target, bits_per_key, observed, size_bytes);
    }
    std::cout << "\n";
}

void test_throughput() {
    std::cout << "Test 4: Write / read throughput\n";
    std::filesystem::remove_all("/tmp/lsm_t4");
    LSM_Tree db("/tmp/lsm_t4");

    std::mt19937 rng(42);
    const int N = 2000;
    std::vector<std::string> keys;
    keys.reserve(N);

    // Writes
    auto t0 = Clock::now();
    for (int i = 0; i < N; i++) {
        std::string k = "key_" + rand_str(rng);
        db.put(k, "val_" + rand_str(rng, 32));
        keys.push_back(k);
    }
    double write_ms = elapsed_ms(t0);
    printf("  %d writes : %.1f ms  (%d writes/sec)\n",
           N, write_ms, (int)(N / (write_ms / 1000.0)));

    // Reads — present keys (hits)
    auto t1 = Clock::now();
    int hits = 0;
    for (auto& k : keys) if (db.get(k).has_value()) hits++;
    double hit_ms = elapsed_ms(t1);
    printf("  %d present-key reads : %.1f ms  (hits=%d)\n", N, hit_ms, hits);

    // Reads — absent keys (Bloom filter should skip most disk reads)
    auto t2 = Clock::now();
    int fp = 0;
    for (int i = 0; i < N; i++)
        if (db.get("absent_" + std::to_string(i)).has_value()) fp++;
    double miss_ms = elapsed_ms(t2);
    printf("  %d absent-key reads  : %.1f ms  (false_positives=%d)\n\n",
           N, miss_ms, fp);

    db.print_stats();
}

void test_write_heavy() {
    std::cout << "Test 5: Write-Heavy Stress (100k keys)\n";
    std::filesystem::remove_all("/tmp/lsm_t5");
    LSM_Tree db("/tmp/lsm_t5");
    std::mt19937 rng(123);

    const int N = 100000;
    std::vector<std::string> keys;
    keys.reserve(N);

    Timer t;
    for (int i = 0; i < N; i++) {
        std::string k = "wk_" + std::to_string(i) + "_" + rand_str(rng, 4);
        db.put(k, "val_" + rand_str(rng, 32));
        if (i % 10000 == 0 && i > 0) std::cout << "  Inserted " << i << " keys...\n";
        keys.push_back(k);
    }
    double ms = t.stop();
    printf("  %d writes : %.1f ms (%.1f writes/sec)\n", N, ms, N / (ms / 1000.0));

    int verified = 0;
    for (int i = 0; i < N; i += 100) {
        if (db.get(keys[i]).has_value()) verified++;
    }
    std::cout << "  Sample verification: " << verified << "/1000 keys found\n";
    db.print_stats();
    std::cout << "\n";
}

void test_update_delete() {
    std::cout << "Test 6: Update/Delete Stress (100k ops on 1k keys)\n";
    std::filesystem::remove_all("/tmp/lsm_t6");
    LSM_Tree db("/tmp/lsm_t6");
    std::mt19937 rng(456);

    const int pool_size = 1000;
    const int total_ops = 100000;
    std::vector<std::string> key_pool;
    for (int i = 0; i < pool_size; i++) {
        key_pool.push_back("ukey_" + std::to_string(i));
    }

    std::vector<std::string> final_values(pool_size, "");
    std::vector<bool> exists(pool_size, false);

    Timer t;
    for (int i = 0; i < total_ops; i++) {
        int idx = rng() % pool_size;
        bool do_put = rng() % 2 == 0;
        if (do_put) {
            std::string val = "val_" + std::to_string(i);
            db.put(key_pool[idx], val);
            final_values[idx] = val;
            exists[idx] = true;
        } else {
            db.remove(key_pool[idx]);
            exists[idx] = false;
        }
    }
    double ms = t.stop();
    printf("  %d ops : %.1f ms (%.1f ops/sec)\n", total_ops, ms, total_ops / (ms / 1000.0));

    int correct = 0;
    for (int i = 0; i < pool_size; i++) {
        auto res = db.get(key_pool[i]);
        if (exists[i]) {
            if (res.has_value() && res.value() == final_values[i]) correct++;
        } else {
            if (!res.has_value()) correct++;
        }
    }
    std::cout << "  Correctness: " << correct << "/" << pool_size << " keys match\n";
    db.print_stats();
    std::cout << "\n";
}

void test_mixed_workload() {
    std::cout << "Test 7: Mixed Workload Stress (100k ops)\n";
    std::filesystem::remove_all("/tmp/lsm_t7");
    LSM_Tree db("/tmp/lsm_t7");
    std::mt19937 rng(789);

    const int total_ops = 100000;
    const int hot_set_size = 1000;
    std::vector<std::string> hot_keys;
    for (int i = 0; i < hot_set_size; i++) hot_keys.push_back("hot_" + std::to_string(i));

    Timer t;
    for (int i = 0; i < total_ops; i++) {
        int op_type = rng() % 100;
        if (op_type < 70) {
            std::string k = (rng() % 10 < 7) ? hot_keys[rng() % hot_set_size] : "cold_" + std::to_string(i);
            db.put(k, "val_" + std::to_string(i));
        } else if (op_type < 90) {
            std::string k = (rng() % 10 < 7) ? hot_keys[rng() % hot_set_size] : "cold_" + std::to_string(rng() % (i + 1));
            db.get(k);
        } else {
            std::string start = "key_" + std::to_string(rng() % (i + 1));
            std::string end = "key_" + std::to_string(rng() % (i + 1) + 100);
            db.scan(start, end);
        }
    }
    double ms = t.stop();
    printf("  %d mixed ops : %.1f ms (%.1f ops/sec)\n", total_ops, ms, total_ops / (ms / 1000.0));
    db.print_stats();
    std::cout << "\n";
}

void test_large_values() {
    std::cout << "Test 8: Large Value Stress\n";
    std::filesystem::remove_all("/tmp/lsm_t8");
    LSM_Tree db("/tmp/lsm_t8");
    std::mt19937 rng(1011);

    const int N = 100;
    std::vector<std::string> keys;
    for (int i = 0; i < N; i++) {
        std::string k = "large_" + std::to_string(i);
        size_t size = 64 * 1024 + (rng() % (1024 * 1024));
        std::string val(size, 'x');
        db.put(k, val);
        keys.push_back(k);
    }

    Timer t;
    for (int i = 0; i < 500; i++) {
        db.get(keys[rng() % N]);
    }
    double ms = t.stop();
    printf("  500 large-value reads : %.1f ms\n", ms);
    db.print_stats();
    std::cout << "\n";
}

int main(){
    std::cout << "\n╔══════════════════════════════════════════╗\n";
    std::cout <<   "║   LSM-Tree + Search Optimization Tests   ║\n";
    std::cout <<   "╚══════════════════════════════════════════╝\n\n";
    try{
        test_basic_correctness();
        test_persistence();
        test_bloom_filter();
        test_throughput();
        test_write_heavy();
        test_update_delete();
        test_mixed_workload();
        test_large_values();
    }
    catch(const std::exception& e){
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
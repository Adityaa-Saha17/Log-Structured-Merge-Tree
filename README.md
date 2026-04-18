# README.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Run Commands
- Build: `mkdir -p build && cd build && cmake .. && make`
- Run Tests: `./build/lsm_runner`
- Compiler: C++26 (CMake based)

## Architecture Overview
This project implements a Log-Structured Merge-Tree (LSM-Tree) storage engine.

### Core Components
- `LSM_Tree`: The main coordinator managing the memtable, WAL, and SSTables.
- `MemTable` / `SkipList`: In-memory buffer for writes, using a skip list for sorted storage.
- `SSTable` / `SparseIndex`: On-disk sorted strings tables with sparse indexes for efficient lookups.
- `WAL`: Write-Ahead Log for crash recovery and persistence.
- `BloomFilter`: Probabilistic check to avoid unnecessary disk reads for non-existent keys.
- `BlockCache`: Caches disk blocks to reduce I/O.
- `MergeIterator`: Merges results from multiple SSTables and the memtable for reads and range scans.
- `CompactionGuard`: Manages the background compaction process to merge SSTables and remove deleted keys.

### Data Path
- **Writes**: `LSM_Tree` $\rightarrow$ `WAL` (persistence) $\rightarrow$ `MemTable` (in-memory). When full, `MemTable` is flushed to an `SSTable`.
- **Reads**: `MemTable` $\rightarrow$ `BloomFilter` (check) $\rightarrow$ `SSTable` (via `SparseIndex` and `BlockCache`).
- **Range Scans**: Uses `MergeIterator` to provide a sorted stream of keys across all levels.

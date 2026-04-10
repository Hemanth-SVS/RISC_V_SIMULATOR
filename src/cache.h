#ifndef CACHE_H
#define CACHE_H

#include <string>
#include <vector>
#include <cstdint>
using namespace std;

enum ReplacementPolicy { LRU, FIFO };

struct CacheLine {
    bool valid = false;
    bool dirty = false;
    uint32_t tag = 0;
    uint64_t counter = 0;  // used for both LRU and FIFO ordering
};

class Cache {
public:
    Cache();
    Cache(string name, int size, int block_size, int assoc, int latency, ReplacementPolicy pol);

    bool access(uint32_t addr, bool is_write);
    void install(uint32_t addr, bool is_write, bool &evicted_dirty, uint32_t &evicted_addr);

    int get_latency();
    string get_name();

    // stats
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t accesses = 0;

private:
    string name;
    int total_size;
    int block_size;
    int assoc;
    int latency;
    ReplacementPolicy policy;

    int num_sets;
    int offset_bits;
    int index_bits;

    vector<vector<CacheLine>> sets;
    uint64_t counter = 0;

    uint32_t get_tag(uint32_t addr);
    int get_index(uint32_t addr);
    uint32_t make_addr(uint32_t tag, int idx);
    int find_victim(int idx);
    int log2(int val);
};

#endif

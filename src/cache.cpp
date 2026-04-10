#include "cache.h"
#include <iostream>

using namespace std;

int Cache::log2(int val) {
    int r = 0;
    while ((1 << r) < val) r++;
    return r;
}

Cache::Cache() {
    total_size = 0;
    block_size = 0;
    assoc = 0;
    latency = 0;
    policy = LRU;
    num_sets = 0;
    offset_bits = 0;
    index_bits = 0;
}

Cache::Cache(string name_, int size_, int block_, int assoc_, int lat_, ReplacementPolicy pol_) {
    name = name_;
    total_size = size_;
    block_size = block_;
    assoc = assoc_;
    latency = lat_;
    policy = pol_;

    num_sets = total_size / (block_size * assoc);
    if (num_sets <= 0) num_sets = 1;

    offset_bits = log2(block_size);
    index_bits = log2(num_sets);

    sets.resize(num_sets);
    for (int i = 0; i < num_sets; i++) {
        sets[i].resize(assoc);
    }
}

uint32_t Cache::get_tag(uint32_t addr) {
    return addr >> (offset_bits + index_bits);
}

int Cache::get_index(uint32_t addr) {
    return (addr >> offset_bits) & ((1 << index_bits) - 1);
}

uint32_t Cache::make_addr(uint32_t tag, int idx) {
    return (tag << (offset_bits + index_bits)) | (idx << offset_bits);
}

bool Cache::access(uint32_t addr, bool is_write) {
    accesses++;
    int idx = get_index(addr);
    uint32_t tag = get_tag(addr);

    for (int w = 0; w < assoc; w++) {
        if (sets[idx][w].valid && sets[idx][w].tag == tag) {
            // hit
            hits++;
            if (policy == LRU) {
                counter++;
                sets[idx][w].counter = counter;
            }
            if (is_write) sets[idx][w].dirty = true;
            return true;
        }
    }

    // miss
    misses++;
    return false;
}

int Cache::find_victim(int idx) {
    // first check for empty line
    for (int w = 0; w < assoc; w++) {
        if (!sets[idx][w].valid) return w;
    }

    // pick victim with smallest counter (LRU = least recently used, FIFO = earliest inserted)
    int victim = 0;
    uint64_t min_val = sets[idx][0].counter;
    for (int w = 1; w < assoc; w++) {
        if (sets[idx][w].counter < min_val) {
            min_val = sets[idx][w].counter;
            victim = w;
        }
    }
    return victim;
}

void Cache::install(uint32_t addr, bool is_write, bool &evicted_dirty, uint32_t &evicted_addr) {
    int idx = get_index(addr);
    uint32_t tag = get_tag(addr);

    evicted_dirty = false;
    evicted_addr = 0;

    int victim = find_victim(idx);
    CacheLine &line = sets[idx][victim];

    if (line.valid && line.dirty) {
        evicted_dirty = true;
        evicted_addr = make_addr(line.tag, idx);
    }

    line.valid = true;
    line.tag = tag;
    line.dirty = is_write;
    counter++;
    line.counter = counter;
}

int Cache::get_latency() { return latency; }
string Cache::get_name() { return name; }

#ifndef CACHE_HIERARCHY_H
#define CACHE_HIERARCHY_H

#include "cache.h"
#include <string>
using namespace std;

struct CacheConfig {
    int l1i_size = 1024;
    int l1i_block = 64;
    int l1i_assoc = 2;
    int l1i_latency = 1;

    int l1d_size = 1024;
    int l1d_block = 64;
    int l1d_assoc = 2;
    int l1d_latency = 1;

    int l2_size = 4096;
    int l2_block = 64;
    int l2_assoc = 4;
    int l2_latency = 4;

    int mem_latency = 100;
    ReplacementPolicy policy = LRU;
};

class CacheHierarchy {
public:
    CacheHierarchy(CacheConfig cfg);
    int instruction_access(uint32_t addr);
    int data_access(uint32_t addr, bool is_write);
    void print_stats();

    Cache l1i;
    Cache l1d;
    Cache l2;
    int mem_latency;
};

#endif

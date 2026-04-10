#include "cache_hierarchy.h"
#include <iostream>

using namespace std;

CacheHierarchy::CacheHierarchy(CacheConfig cfg) {
    l1i = Cache("L1I", cfg.l1i_size, cfg.l1i_block, cfg.l1i_assoc, cfg.l1i_latency, cfg.policy);
    l1d = Cache("L1D", cfg.l1d_size, cfg.l1d_block, cfg.l1d_assoc, cfg.l1d_latency, cfg.policy);
    l2  = Cache("L2",  cfg.l2_size,  cfg.l2_block,  cfg.l2_assoc,  cfg.l2_latency,  cfg.policy);
    mem_latency = cfg.mem_latency;
}

int CacheHierarchy::instruction_access(uint32_t addr) {
    // check L1I first
    bool hit = l1i.access(addr, false);
    if (hit) return l1i.get_latency();

    // L1I miss, check L2
    hit = l2.access(addr, false);
    if (hit) {
        bool ev_dirty; uint32_t ev_addr;
        l1i.install(addr, false, ev_dirty, ev_addr);
        return l1i.get_latency() + l2.get_latency();
    }

    // L2 miss, go to main memory
    bool ev_dirty; uint32_t ev_addr;
    l2.install(addr, false, ev_dirty, ev_addr);
    l1i.install(addr, false, ev_dirty, ev_addr);
    return l1i.get_latency() + l2.get_latency() + mem_latency;
}

int CacheHierarchy::data_access(uint32_t addr, bool is_write) {
    // check L1D first
    bool hit = l1d.access(addr, is_write);
    if (hit) return l1d.get_latency();

    // L1D miss, check L2
    hit = l2.access(addr, is_write);
    if (hit) {
        bool ev_dirty; uint32_t ev_addr;
        l1d.install(addr, is_write, ev_dirty, ev_addr);
        if (ev_dirty) l2.access(ev_addr, true);
        return l1d.get_latency() + l2.get_latency();
    }

    // L2 miss, go to main memory
    bool ev_dirty; uint32_t ev_addr;
    l2.install(addr, is_write, ev_dirty, ev_addr);
    l1d.install(addr, is_write, ev_dirty, ev_addr);
    if (ev_dirty) l2.access(ev_addr, true);
    return l1d.get_latency() + l2.get_latency() + mem_latency;
}

void CacheHierarchy::print_stats() {
    cout << "\n=== Cache Statistics ===" << "\n";

    double l1i_mr = (l1i.accesses > 0) ? (double)l1i.misses / l1i.accesses * 100.0 : 0;
    double l1d_mr = (l1d.accesses > 0) ? (double)l1d.misses / l1d.accesses * 100.0 : 0;
    double l2_mr  = (l2.accesses > 0)  ? (double)l2.misses  / l2.accesses  * 100.0 : 0;

    cout << "L1I: accesses=" << l1i.accesses
         << ", hits=" << l1i.hits
         << ", misses=" << l1i.misses
         << ", miss_rate=" << l1i_mr << "%" << "\n";

    cout << "L1D: accesses=" << l1d.accesses
         << ", hits=" << l1d.hits
         << ", misses=" << l1d.misses
         << ", miss_rate=" << l1d_mr << "%" << "\n";

    cout << "L2:  accesses=" << l2.accesses
         << ", hits=" << l2.hits
         << ", misses=" << l2.misses
         << ", miss_rate=" << l2_mr << "%" << "\n";
}

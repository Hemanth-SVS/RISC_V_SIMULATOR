#ifndef VM_H
#define VM_H

#include <cstdint>
#include <vector>
#include <map>
#include <queue>
#include <string>
using namespace std;

// TLB entry - basically a cache for page table entries
struct TLBEntry {
    uint32_t vpn;           // Virtual page number
    uint32_t pfn;           // Physical frame number
    bool valid = false;     // Is this entry valid?
    bool dirty = false;     // Someone wrote to this page
    uint64_t last_access;   // For LRU - when did we last use this
    uint64_t insert_time;   // For FIFO - when did we put this in
};

// Page table entry - maps virtual to physical
struct PTEntry {
    uint32_t pfn = 0;       // Which physical frame
    bool valid = false;     // Valid entry?
    bool dirty = false;     // Modified?
    bool present = false;   // Actually in RAM or swapped out
};

// Physical frame - actual RAM page
struct Frame {
    bool allocated = false;   // Someone using this?
    uint32_t vpn = 0;       // Which virtual page lives here
    bool dirty = false;     // Need to write back to disk?
};

enum VMReplacementPolicy { VM_FIFO, VM_LRU };

struct VMConfig {
    uint32_t virtual_size_bytes = 65536;
    uint32_t physical_size_bytes = 16384;
    uint32_t page_size_bytes = 4096;
    uint32_t dtlb_entries = 4;
    uint32_t tlb_hit_latency = 1;
    uint32_t page_walk_latency = 10;
    uint32_t page_fault_latency = 50;
    VMReplacementPolicy replacement_policy = VM_LRU;
};

class VirtualMemory {
public:
    VirtualMemory(const VMConfig &cfg);

    // Main translation function - turns virtual addr into physical addr
    // Returns how many cycles this takes
    // Also sets physical_addr to the result
    int translate(uint32_t virtual_addr, uint32_t &physical_addr, bool is_write);

    // Getters for stats - prof wants these in output
    uint64_t get_tlb_hits() const { return tlb_hits; }
    uint64_t get_tlb_misses() const { return tlb_misses; }
    uint64_t get_page_walks() const { return page_walks; }
    uint64_t get_page_faults() const { return page_faults; }
    uint64_t get_page_evictions() const { return page_evictions; }
    uint64_t get_dirty_evictions() const { return dirty_evictions; }
    uint64_t get_total_translation_penalty() const { return total_translation_penalty; }

    void print_stats() const;
    void reset_stats();

private:
    VMConfig cfg;

    // Derived parameters
    uint32_t num_virtual_pages;
    uint32_t num_physical_frames;
    uint32_t page_offset_bits;
    uint32_t vpn_mask;
    uint32_t offset_mask;

    // TLB (fully associative)
    vector<TLBEntry> dtlb;

    // Page Table (flat)
    vector<PTEntry> page_table;

    // Physical frames
    vector<Frame> frames;

    // Free frame management
    queue<uint32_t> free_frames;

    // Statistics
    uint64_t tlb_hits = 0;
    uint64_t tlb_misses = 0;
    uint64_t page_walks = 0;
    uint64_t page_faults = 0;
    uint64_t page_evictions = 0;
    uint64_t dirty_evictions = 0;
    uint64_t total_translation_penalty = 0;

    // Access counter for LRU/FIFO
    uint64_t access_counter = 0;

    // Bit manipulation stuff - extract VPN and offset from address
    uint32_t get_vpn(uint32_t addr) const { return (addr >> page_offset_bits) & vpn_mask; }
    uint32_t get_offset(uint32_t addr) const { return addr & offset_mask; }
    uint32_t make_physical_addr(uint32_t pfn, uint32_t offset) const { return (pfn << page_offset_bits) | offset; }

    // TLB stuff - lookup, insert, etc.
    int tlb_lookup(uint32_t vpn);           // Find in TLB
    void tlb_insert(uint32_t vpn, uint32_t pfn, bool dirty);  // Add to TLB
    void tlb_update_dirty(uint32_t vpn);    // Mark as written
    int find_tlb_victim();                  // Who to kick out

    // Frame management
    int allocate_frame();       // Get free frame
    void evict_frame();         // Kick someone out
    int find_victim_frame();    // Who to evict
};

#endif

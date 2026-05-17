#include "vm.h"
#include <iostream>
#include <algorithm>

using namespace std;

VirtualMemory::VirtualMemory(const VMConfig &cfg_) : cfg(cfg_) {
    // Figure out how many pages and frames we have
    num_virtual_pages = cfg.virtual_size_bytes / cfg.page_size_bytes;
    num_physical_frames = cfg.physical_size_bytes / cfg.page_size_bytes;

    // Calculate how many bits for page offset (log2 of page size)
    page_offset_bits = 0;
    uint32_t temp = cfg.page_size_bytes;
    while (temp > 1) {
        temp >>= 1;
        page_offset_bits++;
    }

    // Masks for extracting VPN and offset from address
    offset_mask = (1U << page_offset_bits) - 1;
    vpn_mask = (1U << (32 - page_offset_bits)) - 1;

    // Set up TLB with given number of entries
    dtlb.resize(cfg.dtlb_entries);

    // Create page table - one entry per virtual page
    page_table.resize(num_virtual_pages);

    // All frames start as free
    frames.resize(num_physical_frames);
    for (uint32_t i = 0; i < num_physical_frames; i++) {
        free_frames.push(i);
    }
}

int VirtualMemory::translate(uint32_t virtual_addr, uint32_t &physical_addr, bool is_write) {
    // Split address into VPN and offset
    uint32_t vpn = get_vpn(virtual_addr);
    uint32_t offset = get_offset(virtual_addr);

    // First, try TLB - it's fast!
    int tlb_idx = tlb_lookup(vpn);

    if (tlb_idx >= 0) {
        // Sweet, TLB hit!
        // cout << "tlb hit for vpn " << vpn << endl; // DEBUG
        tlb_hits++;
        access_counter++;
        dtlb[tlb_idx].last_access = access_counter;

        // Mark as dirty if writing
        if (is_write) {
            dtlb[tlb_idx].dirty = true;
            page_table[vpn].dirty = true;  // Keep page table in sync tbh
        }

        uint32_t pfn = dtlb[tlb_idx].pfn;
        physical_addr = make_physical_addr(pfn, offset);
        total_translation_penalty += cfg.tlb_hit_latency;
        return cfg.tlb_hit_latency;  // Fast path done
    }

    // Crap, TLB miss - gotta do a page walk
    tlb_misses++;
    int latency = 0;

    page_walks++;
    latency += cfg.page_walk_latency;  // Walking the page table costs cycles

    PTEntry &pte = page_table[vpn];

    if (!pte.valid || !pte.present) {
        // Page fault! Page isn't in memory
        // printf("FAULT at %x\n", virtual_addr);
        page_faults++;
        latency += cfg.page_fault_latency;  // Fault handling is expensive

        // Try to get a free frame
        int frame_idx = allocate_frame();

        if (frame_idx < 0) {
            // No free frames left, gotta evict someone
            evict_frame();
            frame_idx = allocate_frame();  // Should work now hopefully
        }

        // Set up the mapping
        pte.pfn = frame_idx;
        pte.valid = true;
        pte.present = true;
        pte.dirty = is_write;

        frames[frame_idx].allocated = true;
        frames[frame_idx].vpn = vpn;
        frames[frame_idx].dirty = is_write;
    } else {
        // Page is already in memory, just mark dirty if writing
        if (is_write) {
            pte.dirty = true;
            frames[pte.pfn].dirty = true;
        }
    }

    // Update TLB so we don't page walk next time
    tlb_insert(vpn, pte.pfn, pte.dirty);

    uint32_t pfn = pte.pfn;
    physical_addr = make_physical_addr(pfn, offset);

    total_translation_penalty += latency;
    return latency;
}

int VirtualMemory::tlb_lookup(uint32_t vpn) {
    // Linear search through TLB - it's small so this is fine
    for (size_t i = 0; i < dtlb.size(); i++) {
        if (dtlb[i].valid && dtlb[i].vpn == vpn) {
            return i;  // Found it!
        }
    }
    return -1;  // Not in TLB
}

void VirtualMemory::tlb_insert(uint32_t vpn, uint32_t pfn, bool dirty) {
    // Find someone to kick out (or an empty slot)
    int victim = find_tlb_victim();

    // Put new entry there
    dtlb[victim].vpn = vpn;
    dtlb[victim].pfn = pfn;
    dtlb[victim].valid = true;
    dtlb[victim].dirty = dirty;
    dtlb[victim].insert_time = access_counter;
    dtlb[victim].last_access = access_counter;
}

void VirtualMemory::tlb_update_dirty(uint32_t vpn) {
    for (size_t i = 0; i < dtlb.size(); i++) {
        if (dtlb[i].valid && dtlb[i].vpn == vpn) {
            dtlb[i].dirty = true;
            return;
        }
    }
}

int VirtualMemory::find_tlb_victim() {
    // First, look for invalid entry
    for (size_t i = 0; i < dtlb.size(); i++) {
        if (!dtlb[i].valid) {
            return i;
        }
    }

    // All entries valid, pick victim based on policy
    int victim = 0;
    uint64_t min_val = dtlb[0].last_access;

    for (size_t i = 1; i < dtlb.size(); i++) {
        uint64_t val = (cfg.replacement_policy == VM_FIFO) ? dtlb[i].insert_time : dtlb[i].last_access;

        if (val < min_val) {
            min_val = val;
            victim = i;
        }
    }

    return victim;
}

int VirtualMemory::allocate_frame() {
    if (free_frames.empty()) {
        return -1;
    }

    int frame = free_frames.front();
    free_frames.pop();
    return frame;
}

void VirtualMemory::evict_frame() {
    // Figure out who to evict
    int victim_idx = find_victim_frame();

    if (victim_idx < 0) return;  // Shouldn't happen but whatever

    Frame &victim = frames[victim_idx];
    uint32_t vpn = victim.vpn;

    // Mark page as not in memory anymore
    page_table[vpn].present = false;

    // Count dirty writeback if needed
    if (victim.dirty || page_table[vpn].dirty) {
        dirty_evictions++;
    }

    page_evictions++;

    // Invalidate TLB entries for this page so we don't have stale mappings
    for (size_t i = 0; i < dtlb.size(); i++) {
        if (dtlb[i].valid && dtlb[i].vpn == vpn) {
            dtlb[i].valid = false;
        }
    }

    // Free the frame
    victim.allocated = false;
    victim.dirty = false;
    free_frames.push(victim_idx);
}

int VirtualMemory::find_victim_frame() {
    // Find a frame to evict using the replacement policy
    int victim = -1;
    uint64_t min_access = UINT64_MAX;

    for (uint32_t i = 0; i < num_physical_frames; i++) {
        if (!frames[i].allocated) continue;

        uint32_t vpn = frames[i].vpn;
        uint64_t last_acc = page_table[vpn].dirty ? access_counter : 0;  // Use page table dirty as proxy

        // We need to track access times properly - use TLB info if available
        for (const auto &entry : dtlb) {
            if (entry.valid && entry.vpn == vpn) {
                last_acc = entry.last_access;
                break;
            }
        }

        if (cfg.replacement_policy == VM_LRU) {
            if (last_acc < min_access) {
                min_access = last_acc;
                victim = i;
            }
        } else { // VM_FIFO
            // For FIFO, we track when the frame was allocated
            // Use a simple approximation: first allocated frame
            if (victim == -1) {
                victim = i;
            }
        }
    }

    // If FIFO and no victim found, pick the first allocated frame
    if (victim == -1) {
        for (uint32_t i = 0; i < num_physical_frames; i++) {
            if (frames[i].allocated) {
                victim = i;
                break;
            }
        }
    }

    return victim;
}

void VirtualMemory::print_stats() const {
    cout << "\n=== Virtual Memory Statistics ===" << "\n";
    cout << "TLB hits: " << tlb_hits << "\n";
    cout << "TLB misses: " << tlb_misses << "\n";
    cout << "Page walks: " << page_walks << "\n";
    cout << "Page faults: " << page_faults << "\n";
    cout << "Page evictions: " << page_evictions << "\n";
    cout << "Dirty evictions (writebacks): " << dirty_evictions << "\n";
    cout << "Total translation penalty cycles: " << total_translation_penalty << "\n";

    double tlb_hit_rate = (tlb_hits + tlb_misses > 0) ?
        (double)tlb_hits / (tlb_hits + tlb_misses) * 100 : 0;
    cout << "TLB hit rate: " << tlb_hit_rate << "%" << "\n";
}

void VirtualMemory::reset_stats() {
    tlb_hits = 0;
    tlb_misses = 0;
    page_walks = 0;
    page_faults = 0;
    page_evictions = 0;
    dirty_evictions = 0;
    total_translation_penalty = 0;
}

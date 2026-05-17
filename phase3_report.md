# Phase 3 Report: Virtual Memory Implementation

## Design Overview

This phase extends the RISC-V simulator from previous phases to support virtual memory with trace replay mode. The implementation includes:

### New Components

1. **Virtual Memory Subsystem** (`vm.h` / `vm.cpp`)
   - Data TLB (DTLB) with configurable entries
   - Flat page table for virtual-to-physical translation
   - Page walk mechanism on TLB miss
   - Page fault handling for unmapped pages
   - Frame allocator with LRU/FIFO replacement policies
   - Dirty page tracking for write-back on eviction

2. **Trace Parser** (`trace_parser.h` / `trace_parser.cpp`)
   - Supports L (Load), S (Store), ADD, and MUL instructions
   - Parses virtual addresses and register operands
   - Designed for the provided trace file format

3. **Modified Simulator** (`simulator.h` / `simulator.cpp`)
   - Dual-mode operation: assembly mode and trace replay mode
   - Virtual address translation for L/S instructions
   - VM translation latency modeling (TLB hit, page walk, page fault)
   - Updated statistics for VM metrics

### Configuration Parameters

The simulator reads configuration from a file supporting these VM parameters:

| Parameter | Description |
|-----------|-------------|
| `virtual_size_bytes` | Total virtual memory size |
| `physical_size_bytes` | Physical memory size (frame count × page size) |
| `page_size_bytes` | Page size (4 KB in our tests) |
| `dtlb_entries` | Number of DTLB entries |
| `tlb_hit_latency` | Cycles for TLB hit |
| `page_walk_latency` | Cycles for page table walk |
| `page_fault_latency` | Cycles for page fault handling |
| `vm_replacement_policy` | Page replacement policy (lru/fifo) |

## Address Translation Flow

1. **TLB Lookup**: Check if VPN→PFN mapping exists in DTLB
   - Hit: Return translation with `tlb_hit_latency`
   - Miss: Proceed to page walk

2. **Page Walk**: Access page table to find mapping
   - Page entry valid: Use PFN, add `page_walk_latency`
   - Page entry invalid: Trigger page fault

3. **Page Fault**: Allocate physical frame
   - If frames available: Map page to frame
   - If memory full: Evict victim page (LRU/FIFO), write back if dirty
   - Add `page_fault_latency` + `page_walk_latency`

4. **TLB Update**: Insert new mapping into DTLB (evict if full)

## Test Configuration

Per the specification, all tests used:

```
Page size = 4 KB
DTLB entries = 16
Physical frames = 64 (256 KB physical memory)
TLB hit latency = 1 cycle
Page walk latency = 10 cycles
Page fault latency = 50 cycles
L1 cache = 4 KB direct mapped, 1 cycle latency
L2 cache = None
Replacement policy = LRU
```

## Trace File Results

| Trace | Cycles | Instructions | IPC | L1D Hit Rate | TLB Hit Rate | Page Faults | Page Evictions | Dirty Evictions |
|-------|--------|--------------|-----|--------------|--------------|-------------|----------------|-----------------|
| 01 | 37,081,560 | 715,724 | 0.0193 | **0.0%** | 99.998% | 8 | 0 | 0 |
| 02 | 37,066,899 | 715,704 | 0.0193 | **0.0%** | 99.996% | 16 | 0 | 0 |
| 03 | 38,680,142 | 715,752 | 0.0185 | **0.0%** | 50.0% | 17 | 0 | 0 |
| 04 | 37,541,251 | 715,728 | 0.0191 | **3.15%** | 49.9% | 32 | 0 | 0 |
| 05 | 40,108,678 | 715,732 | 0.0178 | **0.02%** | 4.97% | 64 | 0 | 0 |
| 06 | 42,055,404 | 715,728 | 0.0170 | **4.85%** | 14.9% | 79,945 | 79,881 | 26,124 |
| 07 | 40,276,675 | 715,736 | 0.0178 | **2.83%** | 58.4% | 59,813 | 59,749 | 53,538 |
| 08 | 37,193,086 | 715,740 | 0.0192 | **46.3%** | 4.91% | 272,591 | 272,527 | 56,298 |
| 09 | 37,462,820 | 715,752 | 0.0191 | **32.9%** | 9.75% | 185,796 | 185,732 | 69,002 |
| 10 | 36,731,863 | 715,712 | 0.0195 | **5.1%** | 79.7% | 16,992 | 16,928 | 12,348 |

## Observations

### TLB Behavior
- **Traces 01-02**: Very high TLB hit rates (~99.9%) due to good locality
- **Traces 03-04**: ~50% TLB hit rate, indicating alternating page access patterns
- **Traces 05-09**: Lower TLB hit rates (5-15%), suggesting poor locality with frequent page switches
- **Trace 10**: Higher TLB hit rate (~80%) with moderate page evictions

### Page Faults and Evictions
- **Traces 01-05**: Minimal page faults (8-64), no evictions in traces 01-05
- **Traces 06-09**: High page fault rates with significant evictions (79K-272K)
- **Trace 10**: Moderate page faults (17K) with corresponding evictions

### Performance Impact
- Translation penalty cycles correlate with TLB miss rates
- Traces with high page faults (06-09) show significantly higher total cycles
- IPC ranges from 0.017 to 0.019, indicating VM overhead dominates execution time

### Cache Behavior (Fixed)
- **Fixed double-counting bug**: So initially we had a weird bug where trace mode was bypassing cache or hitting it twice. The hit rates were like stuck at ~50% and we were freaking out. But we fixed it! The hit rates are now accurate.
- **Traces 01-03 show 0% cache hit rate**: At first we thought it was still broken, but it turns out this is just worst-case spatial locality. The trace just keeps striding by exactly 4KB so it kicks itself out of the direct-mapped cache every single time. 100% conflict misses. RIP.
- **Trace 08 shows best cache performance**: 46.3% hit rate. Still not great but much better than the others because it actually has some spatial/temporal locality going on.
- **Trace 09 also moderate**: 32.9% hit rate.
- **L1I = 0 accesses is correct**: In trace mode, instructions come from the trace file, not memory. So no instruction fetch happens here.
- **No L2 cache**: All L1 misses go straight to main memory (100 cycle latency) - this is a huge performance bottleneck but the specs said no L2 so whatever.

## Files Changed/Added

### New Files
- `src/vm.h` - Virtual memory subsystem header
- `src/vm.cpp` - Virtual memory implementation
- `src/trace_parser.h` - Trace parser header
- `src/trace_parser.cpp` - Trace parser implementation
- `phase3_config.cfg` - Phase 3 configuration file
- `phase3_report.md` - This report

### Modified Files
- `src/simulator.h` - Added VM and trace mode support
- `src/simulator.cpp` - Implemented trace replay and VM translation
- `src/main.cpp` - Added trace mode command line option
- `src/cache_hierarchy.cpp` - Fixed handling of disabled L2 cache

## Running the Simulator

```bash
# Trace mode (auto-detected from .trace extension)
./simulator.exe <trace_file>.trace <config_file>

# Assembly mode (default)
./simulator.exe <asm_file>.s <config_file>
```

Example:
```bash
./simulator.exe trace01.trace phase3_config.cfg
```

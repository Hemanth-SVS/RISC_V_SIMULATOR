# RISC-V Pipeline Simulator (Phase 1 + Phase 2)

A 5-stage pipelined RISC-V simulator written in C++. It supports data forwarding, dynamic instruction latencies, hazard detection/stalling, and a **two-level cache hierarchy** with configurable replacement policies.

## Phase 2 Features (Cache Extension)

- **Two-level cache hierarchy:**
  - **L1I** — Instruction cache (accessed during IF stage)
  - **L1D** — Data cache (accessed during MEM stage for LW/SW)
  - **L2** — Unified cache (accessed on L1 miss)
- **Variable latency:** Both IF and MEM stages now have variable latency based on cache hit/miss
- **Replacement policies:** LRU (Least Recently Used) and FIFO (First-In First-Out)
- **Write policy:** Write-back with write-allocate
- **Statistics output:** Cache miss rates, stall breakdown (data hazard vs cache), IPC

## How to Build and Run (Final Phase 3 Version)

### 1. Compiling the Simulator
If you need to recompile the simulator, use the following commands:
```powershell
cd src
g++ -std=c++17 -O2 -Wall *.cpp -o simulator.exe
move simulator.exe ..\
cd ..
```

### 2. Running Trace Replay Mode (Phase 3)
To run all 10 traces automatically and save the results to `phase3_results.txt`:
```powershell
.\run_all_traces.bat
```

To run a single trace manually:
```powershell
.\simulator.exe phase3_traces\trace01.trace config.cfg
```

### 3. Running Assembly Mode (Phase 1 & 2)
The simulator still supports executing `.asm` or `.s` files from earlier phases:
```powershell
.\simulator.exe asm\bubble_sort.asm config.cfg
```

## Configuration (config.cfg)

### Phase 1 Parameters
| Parameter | Description | Default |
|-----------|-------------|---------|
| `forwarding` | Enable data forwarding | `true` |
| `memory_size` | Simulated memory in bytes | `8192` |
| `latency_<op>` | Execution latency per instruction | `1`–`2` |
| `max_cycles` | Maximum simulation cycles | `1000000` |
| `data_base` | Base address for data section | `256` |

### Phase 2 Cache Parameters
| Parameter | Description | Default |
|-----------|-------------|---------|
| `l1i_size` | L1 Instruction cache size (bytes) | `1024` |
| `l1i_block_size` | L1I block size (bytes) | `64` |
| `l1i_assoc` | L1I associativity (ways) | `2` |
| `l1i_latency` | L1I access latency (cycles) | `1` |
| `l1d_size` | L1 Data cache size (bytes) | `1024` |
| `l1d_block_size` | L1D block size (bytes) | `64` |
| `l1d_assoc` | L1D associativity (ways) | `2` |
| `l1d_latency` | L1D access latency (cycles) | `1` |
| `l2_size` | Unified L2 cache size (bytes) | `4096` |
| `l2_block_size` | L2 block size (bytes) | `64` |
| `l2_assoc` | L2 associativity (ways) | `4` |
| `l2_latency` | L2 access latency (cycles) | `4` |
| `mem_latency` | Main memory access latency (cycles) | `100` |
| `replacement_policy` | `lru` or `fifo` | `lru` |

## Sample Output
```
Forwarding: ENABLED
Cache: L1I=1024B L1D=1024B L2=4096B Block=64B Policy=LRU

=== Simulation Results ===
Cycles: 1520
Instructions retired: 906
Total stalls: 447
  Data hazard stalls: 134
  Cache stalls: 313
IPC: 0.5961

=== Cache Statistics ===
  L1I: accesses=1037, hits=1034, misses=3, miss_rate=0.29%
  L1D: accesses=135, hits=134, misses=1, miss_rate=0.74%
  L2 : accesses=4, hits=0, misses=4, miss_rate=100.00%

Memory dump at data_base (256) array -> 1, 2, 3, 4, 5, 7, 8, 9
```

## Architecture

```
Instruction Fetch (IF)  ──► L1I Cache ──► L2 Cache ──► Main Memory
         │
    Decode (ID)
         │
    Execute (EX)
         │
Memory Access (MEM)     ──► L1D Cache ──► L2 Cache ──► Main Memory
         │
   Writeback (WB)
```

## Source Files

| File | Description |
|------|-------------|
| `main.cpp` | Entry point |
| `simulator.h/.cpp` | 5-stage pipeline engine with cache integration |
| `parser.h/.cpp` | Assembly file parser |
| `memory.h/.cpp` | Byte-addressable memory |
| `registers.h/.cpp` | 32 integer registers |
| `cache.h/.cpp` | Generic set-associative cache (LRU/FIFO) |
| `cache_hierarchy.h/.cpp` | L1I + L1D + L2 hierarchy wrapper |
| `utils.h` | String utilities |

## Minutes of Meeting

Date: 22-Feb-2026
Members: Hemanth, Santhosh, Jaswanth
Accomplished: Read through the Phase 1 project document and RISC-V manual.
Decisions: Chose C++ as our programming language for its performance and OOP features. Created the private GitHub repository early to avoid last-minute merge conflicts.
Tasks: Everyone to review the standard 5-stage pipeline diagram and understand structural hazards before the next meeting.
Notes: Celebrated the professor extending the deadline to March 8th. Ordered biryani to kick off the project.

Date: 26-Feb-2026
Members: Hemanth, Santhosh, Jaswanth
Accomplished: Project repo is set up. Dummy Makefile created.
Decisions: Finalized our custom instruction set (ADD, SUB, BNE, JAL, LW, SW, ADDI, SLT). Decided to represent simulated memory as a simple std::vector<uint8_t> of size 8192 bytes.
Tasks: Santhosh to write the initial bubble_sort.asm code. Jaswanth to build parser.cpp to read assembly strings. Hemanth to build the registers.cpp and memory.cpp classes.
Deadline: Have all individual components ready to merge into simulator.cpp by March 2nd.

Date: 03-Mar-2026
Members: Hemanth, Santhosh, Jaswanth
Accomplished: Parser successfully reads instructions and memory data blocks. Basic instructions execute in a single-cycle fashion.
Decisions: Realized that looping through the pipeline from Fetch to Writeback in order causes data to mutate in the same cycle. Decided to completely restructure step_cycle() to evaluate stages in reverse order (WB -> MEM -> EX -> ID -> IF) using a next_pipeline buffer to mimic true hardware cycle isolation.
Tasks: Jaswanth to write the dynamic latency tracking using the config.cfg file. Hemanth to rewrite the step_cycle() loop and basic Load-Use hazard stalling.
Deadline: Pipeline restructuring to be done by March 5th.
Notes: Argued for 20 minutes about how data forwarding actually works in the Execute stage.

Date: 06-Mar-2026
Members: Hemanth, Santhosh, Jaswanth
Accomplished: The 5-stage pipeline is running, but the array output is completely unsorted.
Decisions: Spent 2 hours debugging a C++ std::invalid_argument exception before realizing stoi() was failing on non-numeric data labels like n(x0). Decided to build a parse_imm_token helper to resolve labels into memory addresses. Also found an out-of-bounds memory bug in bubble_sort.asm where it was sorting the n variable into the array.
Tasks: Hemanth to implement the parse_imm_token logic in the Decode stage. Santhosh to fix the inner loop condition in the assembly file to stop at j < n - 1.
Notes: Had Maggi at the hostel canteen at 2 AM while trying to figure out why bne wasn't branching.

Date: 08-Mar-2026
Members: Hemanth, Santhosh, Jaswanth
Accomplished: Data forwarding logic is fully functional. Simulator successfully sorts the array without crashing.
Decisions: Discovered that the jal x0, done infinite loop at the end of the assembly file was maxing out the cycle count to 1,000,000 and ruining the IPC calculation. Decided to delete the final jump to let the pipeline drain and exit gracefully.
Tasks: Hemanth to run final validation tests, push the finalized code to the private GitHub repository, and add coa2026iittp as a collaborator before 11:59 PM.
Notes: Stressed about the deadline. Decided to skip dinner until the repo is successfully pushed to GitHub.

Date: 15-May-2026
Members: Hemanth, Santhosh, Jaswanth
Accomplished: Finally finished Phase 3 Virtual Memory and Trace replay just before the deadline. 
Decisions: Found a horrible bug where we were accessing the cache twice in trace mode and all traces had a fake 50% hit rate. Fixed it and realized traces 01-03 actually have a 0% hit rate because of strided accesses kicking out the direct-mapped cache blocks. Decided it's not a bug, it's just bad locality. 
Tasks: Hemanth to push the final fixed version to GitHub right now so we don't get a late penalty. 
Notes: We are running on zero sleep. If this doesn't compile on the TA's machine I'm going to cry.

---

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


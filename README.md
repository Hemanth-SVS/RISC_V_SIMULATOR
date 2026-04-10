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

## How to Build and Run

### Option 1: Using Make (Linux / macOS)
```bash
make clean
make
./build/riscv_sim asm/bubble_sort.asm config.cfg
```

### Option 2: Manual Compilation (Windows PowerShell)
```powershell
cd src
g++ -std=c++17 -O2 -Wall *.cpp -o simulator.exe
.\simulator.exe ..\asm\bubble_sort.asm ..\config.cfg
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

Date: 08-Mar-2026
Members: Hemanth, Santhosh, Jaswanth
Accomplished: Data forwarding logic is fully functional. Simulator successfully sorts the array without crashing.
Decisions: Discovered that the jal x0, done infinite loop at the end of the assembly file was maxing out the cycle count to 1,000,000 and ruining the IPC calculation. Decided to delete the final jump to let the pipeline drain and exit gracefully.
Tasks: Hemanth to run final validation tests, push the finalized code to the private GitHub repository, and add coa2026iittp as a collaborator before 11:59 PM.
Notes: Stressed about the deadline. Decided to skip dinner until the repo is successfully pushed to GitHub.

Date: 06-Mar-2026
Members: Hemanth, Santhosh, Jaswanth
Accomplished: The 5-stage pipeline is running, but the array output is completely unsorted.
Decisions: Spent 2 hours debugging a C++ std::invalid_argument exception before realizing stoi() was failing on non-numeric data labels like n(x0). Decided to build a parse_imm_token helper to resolve labels into memory addresses. Also found an out-of-bounds memory bug in bubble_sort.asm where it was sorting the n variable into the array.
Tasks: Hemanth to implement the parse_imm_token logic in the Decode stage. Santhosh to fix the inner loop condition in the assembly file to stop at j < n - 1.
Notes: Had Maggi at the hostel canteen at 2 AM while trying to figure out why bne wasn't branching.

Date: 03-Mar-2026
Members: Hemanth, Santhosh, Jaswanth
Accomplished: Parser successfully reads instructions and memory data blocks. Basic instructions execute in a single-cycle fashion.
Decisions: Realized that looping through the pipeline from Fetch to Writeback in order causes data to mutate in the same cycle. Decided to completely restructure step_cycle() to evaluate stages in reverse order (WB -> MEM -> EX -> ID -> IF) using a next_pipeline buffer to mimic true hardware cycle isolation.
Tasks: Jaswanth to write the dynamic latency tracking using the config.cfg file. Hemanth to rewrite the step_cycle() loop and basic Load-Use hazard stalling.
Deadline: Pipeline restructuring to be done by March 5th.
Notes: Argued for 20 minutes about how data forwarding actually works in the Execute stage.

Date: 26-Feb-2026
Members: Hemanth, Santhosh, Jaswanth
Accomplished: Project repo is set up. Dummy Makefile created.
Decisions: Finalized our custom instruction set (ADD, SUB, BNE, JAL, LW, SW, ADDI, SLT). Decided to represent simulated memory as a simple std::vector<uint8_t> of size 8192 bytes.
Tasks: Santhosh to write the initial bubble_sort.asm code. Jaswanth to build parser.cpp to read assembly strings. Hemanth to build the registers.cpp and memory.cpp classes.
Deadline: Have all individual components ready to merge into simulator.cpp by March 2nd.

Date: 22-Feb-2026
Members: Hemanth, Santhosh, Jaswanth
Accomplished: Read through the Phase 1 project document and RISC-V manual.
Decisions: Chose C++ as our programming language for its performance and OOP features. Created the private GitHub repository early to avoid last-minute merge conflicts.
Tasks: Everyone to review the standard 5-stage pipeline diagram and understand structural hazards before the next meeting.
Notes: Celebrated the professor extending the deadline to March 8th. Ordered biryani to kick off the project.

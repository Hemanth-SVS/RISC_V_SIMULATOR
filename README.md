# RISC-V Pipeline Simulator (Phase 1)

A 5-stage pipelined RISC-V simulator written in C++. It supports data forwarding, dynamic instruction latencies, and hazard detection/stalling.

## How to Build and Run

### Option 1: Using Make (Linux / macOS)
A `Makefile` is provided to automate the build process. From the root directory of the project, run:
```bash
make clean
make
To run the simulation:
./build/riscv_sim asm/bubble_sort.asm config.cfg

Option 2: Manual Compilation (Windows PowerShell)

cd src

g++ -std=c++17 -O2 -Wall *.cpp -o simulator.exe

.\simulator.exe ..\asm\bubble_sort.asm ..\config.cfg



Minutes of Meeting :
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

#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "parser.h"
#include "trace_parser.h"
#include "memory.h"
#include "registers.h"
#include "cache_hierarchy.h"
#include "vm.h"
#include <string>
#include <map>
#include <vector>
using namespace std;

struct Config {
    bool forwarding = true;
    size_t memory_size = 4096;
    map<string,int> latency;
    uint64_t max_cycles = 1000000;
    uint32_t data_base = 0x100;
    CacheConfig cache_cfg;   // Phase 2
    VMConfig vm_cfg;         // Phase 3
    bool trace_mode = false; // Phase 3: trace replay mode
};

class Simulator {
public:
    Simulator(const string &input_file, const string &config_file, bool is_trace_mode = false);
    bool load();
    void run();
private:
    string input_file, cfg_file;
    bool is_trace_mode;
    Program prog;                // For assembly mode
    TraceProgram trace_prog;     // For trace mode (Phase 3)
    Config cfg;
    Memory *mem;
    Registers regs;
    CacheHierarchy *cache;       // Phase 2
    VirtualMemory *vm;             // Phase 3
    uint64_t cycles = 0;
    uint64_t instr_executed = 0;
    uint64_t stalls = 0;
    uint64_t cache_stalls = 0;   // Phase 2
    uint64_t vm_stalls = 0;      // Phase 3: stalls due to VM translation

    struct PipelineSlot {
        bool valid = false;
        Instruction ins;              // For assembly mode
        TraceInstruction trace_ins;   // For trace mode (Phase 3)
        int pc_index = -1;
        int ex_cycles_left = 0;
        int stage = 0;
        int rd=-1, rs1=-1, rs2=-1;
        int imm=0;
        bool writes_rd=false;
        uint32_t alu_result=0;
        uint32_t mem_addr=0;          // Virtual address for memory ops
        uint32_t phys_addr=0;         // Physical address after translation (Phase 3)
        uint32_t mem_data=0;
        int if_cycles_left = 0;       // Phase 2
        int mem_cycles_left = 0;      // Phase 2
        bool mem_latency_set = false; // Phase 2
        int vm_cycles_left = 0;       // Phase 3: cycles remaining for VM translation
        bool vm_latency_set = false;  // Phase 3: VM latency computed
    };

    vector<PipelineSlot> pipeline;
    int pc = 0;

    void load_config();
    void step_cycle();
    void decode_stage(PipelineSlot &slot);
    void execute_stage(PipelineSlot &slot, bool &flush, int &next_pc, const vector<PipelineSlot> &next_pipe);
    void mem_stage(PipelineSlot &slot);
    void wb_stage(const PipelineSlot &slot);

    int reg_index_from_str(const string &s);
    int parse_imm_token(const string &s, bool &ok);
    void print_stats();
};

#endif
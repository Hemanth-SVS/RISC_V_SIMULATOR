#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "parser.h"
#include "memory.h"
#include "registers.h"
#include "cache_hierarchy.h"
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
};

class Simulator {
public:
    Simulator(const string &asm_file, const string &config_file);
    bool load();
    void run();
private:
    string asm_file, cfg_file;
    Program prog;
    Config cfg;
    Memory *mem;
    Registers regs;
    CacheHierarchy *cache;       // Phase 2
    uint64_t cycles = 0;
    uint64_t instr_executed = 0;
    uint64_t stalls = 0;
    uint64_t cache_stalls = 0;   // Phase 2

    struct PipelineSlot {
        bool valid = false;
        Instruction ins;
        int pc_index = -1;
        int ex_cycles_left = 0;
        int stage = 0;
        int rd=-1, rs1=-1, rs2=-1;
        int imm=0;
        bool writes_rd=false;
        uint32_t alu_result=0;
        uint32_t mem_addr=0;
        uint32_t mem_data=0;
        int if_cycles_left = 0;       // Phase 2
        int mem_cycles_left = 0;      // Phase 2
        bool mem_latency_set = false; // Phase 2
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
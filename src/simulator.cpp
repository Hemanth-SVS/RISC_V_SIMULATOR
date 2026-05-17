#include "simulator.h"
#include "utils.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

using namespace std;

Simulator::Simulator(const string &input_file_, const string &config_file_, bool trace_mode_)
: input_file(input_file_), cfg_file(config_file_), is_trace_mode(trace_mode_),
  mem(nullptr), cache(nullptr), vm(nullptr) {}

static string cfg_trim(const string &s) {
    auto t = trim(s);
    if (t.size()>0 && t[0]=='#') return "";
    return t;
}

void Simulator::load_config() {
    ifstream ifs(cfg_file);
    if (!ifs) {
        cfg.forwarding = true;
        cfg.memory_size = 4096;
        cfg.latency["add"] = 1;
        cfg.latency["sub"] = 1;
        cfg.latency["addi"] = 1;
        cfg.latency["slt"] = 1;
        cfg.latency["lw"] = 2;
        cfg.latency["sw"] = 1;
        cfg.latency["ADD"] = 1;
        cfg.latency["MUL"] = 3;
        cfg.max_cycles = 1000000;
        cfg.data_base = 0x100;
        return;
    }

    string line;
    while (getline(ifs,line)) {
        line = cfg_trim(line);
        if (line.empty()) continue;
        auto eq = line.find('=');
        if (eq==string::npos) continue;

        string k = trim(line.substr(0,eq));
        string v = trim(line.substr(eq+1));

        if (k=="forwarding") cfg.forwarding = (v=="true" || v=="1");
        else if (k=="trace_mode") cfg.trace_mode = (v=="true" || v=="1");
        else if (k=="memory_size") cfg.memory_size = stoul(v);
        else if (k.rfind("latency_",0)==0) {
            string instr = k.substr(8);
            cfg.latency[instr] = stoi(v);
        }
        else if (k=="max_cycles") cfg.max_cycles = stoull(v);
        else if (k=="data_base") cfg.data_base = stoul(v,nullptr,0);
        // Phase 2: cache config
        else if (k=="l1i_size") cfg.cache_cfg.l1i_size = stoi(v);
        else if (k=="l1i_block_size") cfg.cache_cfg.l1i_block = stoi(v);
        else if (k=="l1i_assoc") cfg.cache_cfg.l1i_assoc = stoi(v);
        else if (k=="l1i_latency") cfg.cache_cfg.l1i_latency = stoi(v);
        else if (k=="l1d_size") cfg.cache_cfg.l1d_size = stoi(v);
        else if (k=="l1d_block_size") cfg.cache_cfg.l1d_block = stoi(v);
        else if (k=="l1d_assoc") cfg.cache_cfg.l1d_assoc = stoi(v);
        else if (k=="l1d_latency") cfg.cache_cfg.l1d_latency = stoi(v);
        else if (k=="l2_size") cfg.cache_cfg.l2_size = stoi(v);
        else if (k=="l2_block_size") cfg.cache_cfg.l2_block = stoi(v);
        else if (k=="l2_assoc") cfg.cache_cfg.l2_assoc = stoi(v);
        else if (k=="l2_latency") cfg.cache_cfg.l2_latency = stoi(v);
        else if (k=="mem_latency") cfg.cache_cfg.mem_latency = stoi(v);
        else if (k=="replacement_policy") {
            if (v=="fifo" || v=="FIFO")
                cfg.cache_cfg.policy = FIFO;
            else
                cfg.cache_cfg.policy = LRU;
        }
        // Phase 3: VM config
        else if (k=="virtual_size_bytes") cfg.vm_cfg.virtual_size_bytes = stoul(v);
        else if (k=="physical_size_bytes") cfg.vm_cfg.physical_size_bytes = stoul(v);
        else if (k=="page_size_bytes") cfg.vm_cfg.page_size_bytes = stoul(v);
        else if (k=="dtlb_entries") cfg.vm_cfg.dtlb_entries = stoul(v);
        else if (k=="tlb_hit_latency") cfg.vm_cfg.tlb_hit_latency = stoul(v);
        else if (k=="page_walk_latency") cfg.vm_cfg.page_walk_latency = stoul(v);
        else if (k=="page_fault_latency") cfg.vm_cfg.page_fault_latency = stoul(v);
        else if (k=="vm_replacement_policy") {
            if (v=="fifo" || v=="FIFO")
                cfg.vm_cfg.replacement_policy = VM_FIFO;
            else
                cfg.vm_cfg.replacement_policy = VM_LRU;
        }
    }
}

bool Simulator::load() {
    load_config();

    // Phase 3: Use physical memory size from VM config if trace mode
    if (is_trace_mode || cfg.trace_mode) {
        is_trace_mode = true;
        cfg.memory_size = cfg.vm_cfg.physical_size_bytes;
    }

    mem = new Memory(cfg.memory_size);
    cache = new CacheHierarchy(cfg.cache_cfg);
    vm = new VirtualMemory(cfg.vm_cfg);

    if (is_trace_mode) {
        // Phase 3: Load trace file
        trace_prog = parse_trace_file(input_file);
        if (trace_prog.instrs.empty()) {
            cerr << "Error: No valid trace instructions loaded.\n";
            return false;
        }
    } else {
        // Assembly mode
        prog = parse_asm_file(input_file);

        uint32_t next_free = cfg.data_base;
        for (auto &db : prog.data_blocks) {
            uint32_t addr = db.addr;
            if (addr == 0) addr = next_free;
            for (size_t i=0;i<db.vals.size();++i)
                mem->store_word(addr + (uint32_t)i*4, db.vals[i]);
            if (!db.label.empty())
                prog.data_labels[db.label] = addr;
            next_free = max(next_free, addr + (uint32_t)db.vals.size()*4);
        }
    }

    pipeline.clear();
    pc = 0;
    cycles = stalls = cache_stalls = vm_stalls = instr_executed = 0;
    return true;
}

int Simulator::reg_index_from_str(const string &s) {
    if (s.size()>0 && s[0]=='x') {
        int idx = stoi(s.substr(1));
        if (idx<0 || idx>31) throw runtime_error("invalid reg");
        return idx;
    }
    return -1;
}

int Simulator::parse_imm_token(const string &s, bool &ok) {
    ok = true;
    if (prog.data_labels.count(s)) return prog.data_labels[s];
    if (prog.labels.count(s)) return prog.labels[s];
    try { return stoi(s, nullptr, 0); }
    catch (...) { ok = false; return 0; }
}

void Simulator::run() {
    cout << "Forwarding: " << (cfg.forwarding ? "ENABLED" : "DISABLED") << "\n";
    cout << "Cache: L1I=" << cfg.cache_cfg.l1i_size << "B"
         << " L1D=" << cfg.cache_cfg.l1d_size << "B"
         << " L2=" << cfg.cache_cfg.l2_size << "B"
         << " Block=" << cfg.cache_cfg.l1d_block << "B"
         << " Policy=" << (cfg.cache_cfg.policy == LRU ? "LRU" : "FIFO") << "\n";

    // Phase 3: Display VM config
    if (is_trace_mode) {
        cout << "Virtual Memory: DTLB=" << cfg.vm_cfg.dtlb_entries
             << " VPN=" << (cfg.vm_cfg.virtual_size_bytes / cfg.vm_cfg.page_size_bytes)
             << " PFN=" << (cfg.vm_cfg.physical_size_bytes / cfg.vm_cfg.page_size_bytes)
             << " Page=" << cfg.vm_cfg.page_size_bytes << "B"
             << " Policy=" << (cfg.vm_cfg.replacement_policy == VM_LRU ? "LRU" : "FIFO") << "\n";
        cout << "Trace Mode: ENABLED\n";
    }

    const uint64_t MAXC = cfg.max_cycles;
    int total_instrs = is_trace_mode ? (int)trace_prog.instrs.size() : (int)prog.instrs.size();

    while (cycles < MAXC) {
        step_cycle();
        cycles++;
        if (pipeline.empty() && pc >= total_instrs) break;
    }
    print_stats();
}

void Simulator::step_cycle() {
    vector<PipelineSlot> next_pipeline;
    bool stall = false;
    bool flush = false;
    int next_pc = pc;
    int total_instrs = is_trace_mode ? (int)trace_prog.instrs.size() : (int)prog.instrs.size();

    // 1. Writeback Stage
    for (const auto &slot : pipeline) {
        if (slot.stage == 4) {
            wb_stage(slot);
            instr_executed++;
        }
    }

    // 2. Memory Stage - with cache and VM latency (Phase 3)
    for (auto slot : pipeline) {
        if (slot.stage == 3) {
            // Phase 3: VM translation latency (only for trace mode memory ops)
            if (is_trace_mode && !slot.vm_latency_set) {
                TraceOpType op = slot.trace_ins.type;
                if (op == TRACE_LOAD || op == TRACE_STORE) {
                    bool is_write = (op == TRACE_STORE);
                    uint32_t phys_addr;
                    slot.vm_cycles_left = vm->translate(slot.mem_addr, phys_addr, is_write);
                    slot.phys_addr = phys_addr;
                    slot.vm_latency_set = true;
                } else {
                    slot.vm_cycles_left = 0;
                    slot.vm_latency_set = true;
                }
            }

            // Handle VM translation cycles
            if (is_trace_mode && slot.vm_cycles_left > 1) {
                slot.vm_cycles_left--;
                next_pipeline.push_back(slot);
                stall = true;
                vm_stalls++;
                continue;
            }

            // first time entering MEM, compute cache latency
            if (!slot.mem_latency_set) {
                string op = is_trace_mode ? "" : slot.ins.op;
                TraceOpType trace_op = is_trace_mode ? slot.trace_ins.type : TRACE_ADD;

                bool is_mem_op = false;
                bool is_write = false;
                uint32_t addr = 0;

                if (is_trace_mode) {
                    is_mem_op = (trace_op == TRACE_LOAD || trace_op == TRACE_STORE);
                    is_write = (trace_op == TRACE_STORE);
                    addr = slot.phys_addr;
                } else {
                    is_mem_op = (op == "lw" || op == "sw");
                    is_write = (op == "sw");
                    addr = slot.mem_addr;
                }

                if (is_mem_op)
                    slot.mem_cycles_left = cache->data_access(addr, is_write);
                else
                    slot.mem_cycles_left = 1;
                slot.mem_latency_set = true;
            }

            if (slot.mem_cycles_left > 1) {
                slot.mem_cycles_left--;
                next_pipeline.push_back(slot);
                stall = true;
                cache_stalls++;
            } else {
                mem_stage(slot);
                slot.stage = 4;
                next_pipeline.push_back(slot);
            }
        }
    }

    // 3. Execute Stage
    for (auto slot : pipeline) {
        if (slot.stage == 2) {
            if (stall) {
                next_pipeline.push_back(slot);
                continue;
            }
            if (slot.ex_cycles_left > 1) {
                slot.ex_cycles_left--;
                next_pipeline.push_back(slot);
                stall = true;
            } else {
                execute_stage(slot, flush, next_pc, next_pipeline);
                slot.stage = 3;
                next_pipeline.push_back(slot);
            }
        }
    }

    // 4. Decode Stage & Hazard Detection
    for (auto slot : pipeline) {
        if (slot.stage == 1) {
            if (flush) continue;
            if (stall) {
                next_pipeline.push_back(slot);
                continue;
            }

            decode_stage(slot);
            bool hazard = false;

            for (const auto &older : pipeline) {
                if (older.stage == 2 || older.stage == 3) {
                    if (older.writes_rd && older.rd != 0) {
                        if (slot.rs1 == older.rd || slot.rs2 == older.rd) {
                            if (!cfg.forwarding) {
                                hazard = true;
                            } else {
                                // Check for load-use or multi-cycle hazard
                                bool older_is_load = is_trace_mode ?
                                    (older.trace_ins.type == TRACE_LOAD) :
                                    (older.ins.op == "lw");
                                if (older.stage == 2 && older_is_load) hazard = true;
                                if (older.stage == 2 && older.ex_cycles_left > 1) hazard = true;
                            }
                        }
                    }
                }
            }

            if (hazard) {
                stall = true;
                stalls++;
                next_pipeline.push_back(slot);
            } else {
                int lat = 1;
                if (is_trace_mode) {
                    // Get latency for trace instructions
                    TraceOpType op = slot.trace_ins.type;
                    if (op == TRACE_ADD) lat = cfg.latency.count("ADD") ? cfg.latency["ADD"] : 1;
                    else if (op == TRACE_MUL) lat = cfg.latency.count("MUL") ? cfg.latency["MUL"] : 3;
                    else lat = 1;  // L/S have no EX latency
                } else {
                    if (cfg.latency.count(slot.ins.op)) lat = cfg.latency[slot.ins.op];
                }
                slot.ex_cycles_left = lat;
                slot.stage = 2;
                next_pipeline.push_back(slot);
            }
        }
    }

    // 5. Fetch Stage
    for (auto slot : pipeline) {
        if (slot.stage == 0) {
            if (flush) continue;
            if (stall) {
                next_pipeline.push_back(slot);
                continue;
            }

            // In trace mode, no instruction fetch from cache
            if (!is_trace_mode && slot.if_cycles_left > 1) {
                slot.if_cycles_left--;
                next_pipeline.push_back(slot);
                stall = true;
                cache_stalls++;
            } else {
                slot.stage = 1;
                next_pipeline.push_back(slot);
            }
        }
    }

    // Fetch New Instructions
    if (flush) {
        pc = next_pc;
    } else if (!stall && pc < total_instrs) {
        PipelineSlot ns;
        ns.valid = true;
        if (is_trace_mode) {
            ns.trace_ins = trace_prog.instrs[pc];
            // In trace mode, instruction fetch has no latency (per spec: "you do not need to simulate instruction cache/instruction misses")
            ns.if_cycles_left = 0;
        } else {
            ns.ins = prog.instrs[pc];
            ns.pc_index = pc;
            // Phase 2: instruction fetch goes through L1I cache
            uint32_t instr_addr = (uint32_t)pc * 4;
            ns.if_cycles_left = cache->instruction_access(instr_addr);
        }
        ns.stage = 0;
        next_pipeline.push_back(ns);
        pc++;
    }

    pipeline = next_pipeline;
}

void Simulator::decode_stage(PipelineSlot &slot) {
    slot.rd = slot.rs1 = slot.rs2 = -1;
    slot.imm = 0;
    slot.writes_rd = false;

    if (is_trace_mode) {
        // Trace mode decoding
        const auto &tins = slot.trace_ins;
        switch (tins.type) {
            case TRACE_LOAD:
                slot.rd = tins.rd;
                slot.mem_addr = tins.addr;  // Virtual address
                slot.writes_rd = true;
                break;
            case TRACE_STORE:
                slot.rs2 = tins.rs2;
                slot.mem_addr = tins.addr;  // Virtual address
                break;
            case TRACE_ADD:
                slot.rd = tins.rd;
                slot.rs1 = tins.rs1;
                slot.rs2 = tins.rs2;
                slot.writes_rd = true;
                break;
            case TRACE_MUL:
                slot.rd = tins.rd;
                slot.rs1 = tins.rs1;
                slot.rs2 = tins.rs2;
                slot.writes_rd = true;
                break;
        }
    } else {
        // Assembly mode decoding
        const auto &ins = slot.ins;
        string op = ins.op;
        bool ok;

        if (op=="add" || op=="sub" || op=="slt") {
            slot.rd  = reg_index_from_str(ins.args[0]);
            slot.rs1 = reg_index_from_str(ins.args[1]);
            slot.rs2 = reg_index_from_str(ins.args[2]);
            slot.writes_rd = true;
        }
        else if (op=="addi") {
            slot.rd  = reg_index_from_str(ins.args[0]);
            slot.rs1 = reg_index_from_str(ins.args[1]);
            slot.imm = parse_imm_token(ins.args[2], ok);
            slot.writes_rd = true;
        }
        else if (op=="lw") {
            slot.rd  = reg_index_from_str(ins.args[0]);
            slot.imm = parse_imm_token(ins.args[1], ok);
            slot.rs1 = reg_index_from_str(ins.args[2]);
            slot.writes_rd = true;
        }
        else if (op=="sw") {
            slot.rs2 = reg_index_from_str(ins.args[0]);
            slot.imm = parse_imm_token(ins.args[1], ok);
            slot.rs1 = reg_index_from_str(ins.args[2]);
        }
        else if (op=="bne") {
            slot.rs1 = reg_index_from_str(ins.args[0]);
            slot.rs2 = reg_index_from_str(ins.args[1]);
            if (prog.labels.count(ins.args[2])) slot.imm = prog.labels[ins.args[2]];
        }
        else if (op=="jal") {
            slot.rd = reg_index_from_str(ins.args[0]);
            if (prog.labels.count(ins.args[1])) slot.imm = prog.labels[ins.args[1]];
            slot.writes_rd = true;
        }
    }
}

void Simulator::execute_stage(PipelineSlot &slot, bool &flush, int &next_pc, const vector<PipelineSlot> &next_pipe) {
    uint32_t val1 = regs.get(slot.rs1);
    uint32_t val2 = regs.get(slot.rs2);

    // Forwarding for both modes
    if (cfg.forwarding) {
        for (const auto &older : next_pipe) {
            if (older.stage == 4 && older.writes_rd && older.rd != 0) {
                bool older_is_load = is_trace_mode ?
                    (older.trace_ins.type == TRACE_LOAD) : (older.ins.op == "lw");
                if (older_is_load) {
                    if (slot.rs1 == older.rd) val1 = older.mem_data;
                    if (slot.rs2 == older.rd) val2 = older.mem_data;
                } else {
                    if (slot.rs1 == older.rd) val1 = older.alu_result;
                    if (slot.rs2 == older.rd) val2 = older.alu_result;
                }
            }
        }
    }

    if (is_trace_mode) {
        // Trace mode execution
        TraceOpType op = slot.trace_ins.type;
        switch (op) {
            case TRACE_LOAD:
                // mem_addr already set in decode (virtual address)
                break;
            case TRACE_STORE:
                slot.alu_result = val2;  // Value to store
                break;
            case TRACE_ADD:
                slot.alu_result = val1 + val2;
                break;
            case TRACE_MUL:
                slot.alu_result = val1 * val2;
                break;
        }
    } else {
        // Assembly mode execution
        string op = slot.ins.op;

        if (op=="add") slot.alu_result = val1 + val2;
        else if (op=="sub") slot.alu_result = val1 - val2;
        else if (op=="addi") slot.alu_result = val1 + slot.imm;
        else if (op=="slt") slot.alu_result = ((int32_t)val1 < (int32_t)val2) ? 1 : 0;
        else if (op=="lw" || op=="sw") {
            slot.mem_addr = (val1 + slot.imm) % cfg.memory_size;
            slot.alu_result = val2;
        }
        else if (op=="bne") {
            if (val1 != val2) {
                flush = true;
                next_pc = slot.imm;
                stalls += 2;
            }
        }
        else if (op=="jal") {
            if (slot.rd >= 0 && slot.rd != 0)
                slot.alu_result = slot.pc_index + 1;
            flush = true;
            next_pc = slot.imm;
            stalls += 2;
        }
    }
}

void Simulator::mem_stage(PipelineSlot &slot) {
    if (is_trace_mode) {
        // Trace mode memory operations use physical address through cache
        TraceOpType op = slot.trace_ins.type;
        if (op == TRACE_LOAD) {
            // FINALLY fixed the double cache access bug. Almost failed the assignment lol.
            try { slot.mem_data = mem->load_word(slot.phys_addr); }
            catch (...) { slot.mem_data = 0; }
        }
        else if (op == TRACE_STORE) {
            try { mem->store_word(slot.phys_addr, slot.alu_result); }
            catch (...) {}
        }
    } else {
        // Assembly mode
        string op = slot.ins.op;
        if (op=="lw") {
            try { slot.mem_data = mem->load_word(slot.mem_addr); }
            catch (...) { slot.mem_data = 0; }
        }
        else if (op=="sw") {
            try { mem->store_word(slot.mem_addr, slot.alu_result); }
            catch (...) {}
        }
    }
}

void Simulator::wb_stage(const PipelineSlot &slot) {
    if (slot.writes_rd && slot.rd != 0) {
        bool is_load = false;
        if (is_trace_mode) {
            is_load = (slot.trace_ins.type == TRACE_LOAD);
        } else {
            is_load = (slot.ins.op == "lw");
        }

        if (is_load) regs.set(slot.rd, slot.mem_data);
        else regs.set(slot.rd, slot.alu_result);
    }
}

void Simulator::print_stats() {
    cout << "\n=== Simulation Results ===" << "\n";
    cout << "Cycles: " << cycles << "\n";
    cout << "Instructions retired: " << instr_executed << "\n";

    if (is_trace_mode) {
        cout << "Total stalls: " << (stalls + cache_stalls + vm_stalls) << "\n";
        cout << "  Data hazard stalls: " << stalls << "\n";
        cout << "  Cache stalls: " << cache_stalls << "\n";
        cout << "  VM translation stalls: " << vm_stalls << "\n";
    } else {
        cout << "Total stalls: " << (stalls + cache_stalls) << "\n";
        cout << "  Data hazard stalls: " << stalls << "\n";
        cout << "  Cache stalls: " << cache_stalls << "\n";
    }

    double ipc = (cycles>0) ? ((double)instr_executed / cycles) : 0;
    cout << "IPC: " << ipc << "\n";

    cache->print_stats();

    // Phase 3: VM stats for trace mode
    if (is_trace_mode && vm) {
        vm->print_stats();
    }

    if (!is_trace_mode && prog.data_labels.count("array")) {
        uint32_t addr = prog.data_labels["array"];
        cout << "Memory dump at data_base (" << cfg.data_base << ") array -> ";
        for (int i=0;i<8;i++) {
            uint32_t v = mem->load_word(addr + i*4);
            cout << v;
            if (i!=7) cout << ", ";
        }
        cout << "\n";
    }
}
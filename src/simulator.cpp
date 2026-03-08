#include "simulator.h"
#include "utils.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

using namespace std;

Simulator::Simulator(const string &asm_file_, const string &config_file_)
: asm_file(asm_file_), cfg_file(config_file_), mem(nullptr) {}

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
        else if (k=="memory_size") cfg.memory_size = stoul(v);
        else if (k.rfind("latency_",0)==0) {
            string instr = k.substr(8);
            cfg.latency[instr] = stoi(v);
        }
        else if (k=="max_cycles") cfg.max_cycles = stoull(v);
        else if (k=="data_base") cfg.data_base = stoul(v,nullptr,0);
    }
}

bool Simulator::load() {
    load_config();
    mem = new Memory(cfg.memory_size);
    prog = parse_asm_file(asm_file);

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

    pipeline.clear();
    pc = 0;
    cycles = stalls = instr_executed = 0;
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
    const uint64_t MAXC = cfg.max_cycles;
    while (cycles < MAXC) {
        step_cycle();
        cycles++;
        if (pipeline.empty() && pc >= (int)prog.instrs.size()) break;
    }
    print_stats();
}

void Simulator::step_cycle() {
    vector<PipelineSlot> next_pipeline;
    bool stall = false;
    bool flush = false;
    int next_pc = pc;

    // 1. Writeback Stage
    for (const auto &slot : pipeline) {
        if (slot.stage == 4) {
            wb_stage(slot);
            instr_executed++;
        }
    }

    // 2. Memory Stage
    for (auto slot : pipeline) {
        if (slot.stage == 3) {
            mem_stage(slot);
            slot.stage = 4;
            next_pipeline.push_back(slot);
        }
    }

    // 3. Execute Stage
    for (auto slot : pipeline) {
        if (slot.stage == 2) {
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

            // Pipeline read state is now protected, hazard checks are perfectly accurate
            for (const auto &older : pipeline) {
                if (older.stage == 2 || older.stage == 3) {
                    if (older.writes_rd && older.rd != 0) {
                        if (slot.rs1 == older.rd || slot.rs2 == older.rd) {
                            if (!cfg.forwarding) {
                                hazard = true; 
                            } else {
                                if (older.stage == 2 && older.ins.op == "lw") hazard = true;
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
                if (cfg.latency.count(slot.ins.op)) lat = cfg.latency[slot.ins.op];
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
            } else {
                slot.stage = 1;
                next_pipeline.push_back(slot);
            }
        }
    }

    // Fetch New Instructions
    if (flush) {
        pc = next_pc;
    } else if (!stall && pc < (int)prog.instrs.size()) {
        PipelineSlot ns;
        ns.valid = true;
        ns.ins = prog.instrs[pc];
        ns.pc_index = pc;
        ns.stage = 0;
        next_pipeline.push_back(ns);
        pc++;
    }

    pipeline = next_pipeline;
}

void Simulator::decode_stage(PipelineSlot &slot) {
    const auto &ins = slot.ins;
    slot.rd = slot.rs1 = slot.rs2 = -1;
    slot.imm = 0;
    slot.writes_rd = false;
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

void Simulator::execute_stage(PipelineSlot &slot, bool &flush, int &next_pc, const vector<PipelineSlot> &next_pipe) {
    uint32_t val1 = regs.get(slot.rs1);
    uint32_t val2 = regs.get(slot.rs2);

    if (cfg.forwarding) {
        for (const auto &older : next_pipe) {
            if (older.stage == 4 && older.writes_rd && older.rd != 0) {
                if (older.ins.op == "lw") {
                    if (slot.rs1 == older.rd) val1 = older.mem_data;
                    if (slot.rs2 == older.rd) val2 = older.mem_data;
                } else {
                    if (slot.rs1 == older.rd) val1 = older.alu_result;
                    if (slot.rs2 == older.rd) val2 = older.alu_result;
                }
            }
        }
    }

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

void Simulator::mem_stage(PipelineSlot &slot) {
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

void Simulator::wb_stage(const PipelineSlot &slot) {
    string op = slot.ins.op;
    if (slot.writes_rd && slot.rd != 0) {
        if (op=="lw") regs.set(slot.rd, slot.mem_data);
        else regs.set(slot.rd, slot.alu_result);
    }
}

void Simulator::print_stats() {
    cout << "\nSimulation results:\n";
    cout << "Cycles: " << cycles << "\n";
    cout << "Instructions retired: " << instr_executed << "\n";
    cout << "Stalls counted: " << stalls << "\n";

    double ipc = (cycles>0) ? ((double)instr_executed / cycles) : 0;
    cout << "IPC: " << ipc << "\n";

    if (prog.data_labels.count("array")) {
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

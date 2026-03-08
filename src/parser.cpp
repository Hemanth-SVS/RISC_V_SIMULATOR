#include "parser.h"
#include "utils.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cctype>

static string strip_comment(const string &s) {
    auto pos = s.find('#');
    if (pos == string::npos) return s;
    return s.substr(0,pos);
}

Program parse_asm_file(const string &filename) {
    Program prog;
    ifstream ifs(filename);
    if (!ifs) {
        cerr << "Cannot open asm file: " << filename << "\n";
        return prog;
    }

    string line;
    int instr_idx = 0;
    while (getline(ifs, line)) {
        line = strip_comment(line);
        line = trim(line);
        if (line.empty()) continue;

        string label;
        size_t colon = line.find(':');
        if (colon != string::npos) {
            label = trim(line.substr(0, colon));
            string rest = trim(line.substr(colon+1));
            if (rest.empty()) {
                prog.labels[label] = instr_idx;
                continue;
            } else {
                if (rest.rfind(".word", 0) == 0) {
                    DataBlock db;
                    db.label = label;
                    string nums = rest.substr(5);
                    for (char &c: nums) if (c==',') c=' ';
                    auto toks = split_ws(nums);
                    for (auto &t: toks) {
                        try { db.vals.push_back((uint32_t)stoi(t,nullptr,0)); } catch(...) { db.vals.push_back(0); }
                    }
                    prog.data_blocks.push_back(db);
                    continue;
                } else {
                    line = rest;
                    prog.labels[label] = instr_idx;
                }
            }
        }

        if (line.rfind(".word", 0) == 0) {
            DataBlock db;
            string nums = line.substr(5);
            for (char &c: nums) if (c==',') c=' ';
            auto toks = split_ws(nums);
            for (auto &t: toks) {
                try { db.vals.push_back((uint32_t)stoi(t,nullptr,0)); } catch(...) { db.vals.push_back(0); }
            }
            prog.data_blocks.push_back(db);
            continue;
        }

        string s = line;
        for (char &c: s) if (c==',') c=' ';
        for (char &c: s) if (c=='(' || c==')') c=' ';
        auto toks = split_ws(s);
        if (toks.empty()) continue;
        Instruction ins;
        ins.op = toks[0];
        ins.raw = line;
        ins.asm_index = instr_idx;
        for (size_t i=1;i<toks.size();++i) ins.args.push_back(toks[i]);
        prog.instrs.push_back(ins);
        instr_idx++;
    }

    return prog;
}
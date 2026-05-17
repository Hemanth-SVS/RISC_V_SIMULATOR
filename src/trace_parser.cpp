#include "trace_parser.h"
#include "utils.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std;

// Convert register string like "x5" to integer 5
static int reg_index_from_str(const string &s) {
    if (s.size() > 0 && s[0] == 'x') {
        int idx = stoi(s.substr(1));  // Skip the 'x' and parse number
        if (idx < 0 || idx > 31) throw runtime_error("invalid register: " + s);
        return idx;
    }
    throw runtime_error("invalid register format: " + s);
}

// Read the trace file and parse all instructions
TraceProgram parse_trace_file(const string &filename) {
    TraceProgram prog;
    ifstream ifs(filename);

    if (!ifs) {
        cerr << "Error: Cannot open trace file: " << filename << "\n";
        return prog;
    }

    string line;
    int line_num = 0;

    // Read file line by line
    while (getline(ifs, line)) {
        line_num++;
        line = trim(line);
        if (line.empty()) continue;  // Skip empty lines

        istringstream iss(line);
        string op;
        iss >> op;  // Get the operation

        TraceInstruction ti;
        ti.raw = line;  // Save original for debugging

        try {
            if (op == "L") {
                // Load: L <addr> <dest_reg>
                ti.type = TRACE_LOAD;
                string addr_str, reg_str;
                iss >> addr_str >> reg_str;

                if (addr_str.empty() || reg_str.empty()) {
                    cerr << "Error: Invalid L format at line " << line_num << ": " << line << "\n";
                    continue;
                }

                ti.addr = stoul(addr_str, nullptr, 0);
                ti.rd = reg_index_from_str(reg_str);
                ti.rs1 = -1;
                ti.rs2 = -1;

            } else if (op == "S") {
                // Store: S <addr> <src_reg>
                ti.type = TRACE_STORE;
                string addr_str, reg_str;
                iss >> addr_str >> reg_str;

                if (addr_str.empty() || reg_str.empty()) {
                    cerr << "Error: Invalid S format at line " << line_num << ": " << line << "\n";
                    continue;
                }

                ti.addr = stoul(addr_str, nullptr, 0);
                ti.rd = -1;
                ti.rs1 = -1;
                ti.rs2 = reg_index_from_str(reg_str);

            } else if (op == "ADD") {
                // Add: ADD <dest> <src1> <src2>
                ti.type = TRACE_ADD;
                string rd_str, rs1_str, rs2_str;
                iss >> rd_str >> rs1_str >> rs2_str;

                if (rd_str.empty() || rs1_str.empty() || rs2_str.empty()) {
                    cerr << "Error: Invalid ADD format at line " << line_num << ": " << line << "\n";
                    continue;
                }

                ti.addr = 0;
                ti.rd = reg_index_from_str(rd_str);
                ti.rs1 = reg_index_from_str(rs1_str);
                ti.rs2 = reg_index_from_str(rs2_str);

            } else if (op == "MUL") {
                // Multiply: MUL <dest> <src1> <src2>
                ti.type = TRACE_MUL;
                string rd_str, rs1_str, rs2_str;
                iss >> rd_str >> rs1_str >> rs2_str;

                if (rd_str.empty() || rs1_str.empty() || rs2_str.empty()) {
                    cerr << "Error: Invalid MUL format at line " << line_num << ": " << line << "\n";
                    continue;
                }

                ti.addr = 0;
                ti.rd = reg_index_from_str(rd_str);
                ti.rs1 = reg_index_from_str(rs1_str);
                ti.rs2 = reg_index_from_str(rs2_str);

            } else {
                cerr << "Warning: Unknown operation '" << op << "' at line " << line_num << ", skipping (pls don't be in the final test)\n";
                continue;
            }

            prog.instrs.push_back(ti);  // Add to our list

        } catch (const exception &e) {
            cerr << "Error parsing line " << line_num << ": " << line << " - " << e.what() << " (bro what is this file)\n";
            continue;
        }
    }

    return prog;
}

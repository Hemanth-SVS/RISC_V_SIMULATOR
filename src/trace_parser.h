#ifndef TRACE_PARSER_H
#define TRACE_PARSER_H

#include <string>
#include <vector>
#include <cstdint>
using namespace std;

// Types of instructions in the trace file
enum TraceOpType {
    TRACE_LOAD,      // L - load from memory
    TRACE_STORE,     // S - store to memory
    TRACE_ADD,       // ADD - add two registers
    TRACE_MUL        // MUL - multiply two registers
};

// One instruction from the trace file
struct TraceInstruction {
    TraceOpType type;    // What kind of instruction
    uint32_t addr;       // Memory address (for L and S)
    int rd;              // Destination register
    int rs1;             // First source register
    int rs2;             // Second source register
    string raw;          // Original line (for debugging if needed)
};

// Holds all the instructions from the trace
struct TraceProgram {
    vector<TraceInstruction> instrs;
};

// Read and parse a trace file
TraceProgram parse_trace_file(const string &filename);

#endif

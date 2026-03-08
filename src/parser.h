#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>
using namespace std;

struct Instruction {
    string op;
    vector<string> args;
    string raw;
    int asm_index;
};

struct DataBlock {
    uint32_t addr = 0;         
    vector<uint32_t> vals;
    string label;              
};

struct Program {
    vector<Instruction> instrs;
    map<string,int> labels;          
    vector<DataBlock> data_blocks;   
    map<string,uint32_t> data_labels; 
};

Program parse_asm_file(const string &filename);

#endif
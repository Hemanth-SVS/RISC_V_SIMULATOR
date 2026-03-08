#include "memory.h"
#include <cstring>
#include <stdexcept>

using namespace std;

Memory::Memory(size_t bytes) : mem(bytes, 0) {}

uint32_t Memory::load_word(uint32_t addr) {
    if (addr + 4 > mem.size()) throw out_of_range("load_word out of range");
    uint32_t val = 0;
    memcpy(&val, &mem[addr], 4);
    return val;
}

void Memory::store_word(uint32_t addr, uint32_t value) {
    if (addr + 4 > mem.size()) throw out_of_range("store_word out of range");
    memcpy(&mem[addr], &value, 4);
}

size_t Memory::size() const { return mem.size(); }
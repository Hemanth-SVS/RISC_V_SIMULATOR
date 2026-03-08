#ifndef MEMORY_H
#define MEMORY_H

#include <vector>
#include <cstdint>
using namespace std;

class Memory {
public:
    Memory(size_t bytes);
    uint32_t load_word(uint32_t addr);
    void store_word(uint32_t addr, uint32_t value);
    size_t size() const;
private:
    vector<uint8_t> mem;
};

#endif
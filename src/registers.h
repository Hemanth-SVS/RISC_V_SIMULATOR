#ifndef REGISTERS_H
#define REGISTERS_H

#include <array>
#include <cstdint>
using namespace std;

class Registers {
public:
    Registers();
    uint32_t get(int idx) const;
    void set(int idx, uint32_t val);
private:
    array<uint32_t, 32> regs;
};

#endif
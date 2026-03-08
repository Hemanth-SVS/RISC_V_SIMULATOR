#include "registers.h"
using namespace std;

Registers::Registers() { regs.fill(0); }

uint32_t Registers::get(int idx) const {
    if (idx == 0) return 0;
    if (idx < 0 || idx > 31) return 0;
    return regs[idx];
}

void Registers::set(int idx, uint32_t val) {
    if (idx == 0) return;
    if (idx < 0 || idx > 31) return;
    regs[idx] = val;
}
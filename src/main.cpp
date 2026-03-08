#include "simulator.h"
#include <iostream>
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <asm_file> <config_file>\n";
        return 1;
    }
    std::string asmfile = argv[1];
    std::string cfgfile = argv[2];
    Simulator sim(asmfile, cfgfile);
    if (!sim.load()) {
        std::cerr << "Failed to load program.\n";
        return 1;
    }
    sim.run();
    return 0;
}

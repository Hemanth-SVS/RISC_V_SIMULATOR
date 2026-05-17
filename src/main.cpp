#include "simulator.h"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <config_file> [--trace]\n";
        std::cerr << "  input_file: Assembly file (.s) or Trace file (.trace)\n";
        std::cerr << "  --trace: Enable trace replay mode for .trace files\n";
        return 1;
    }

    std::string input_file = argv[1];
    std::string cfgfile = argv[2];
    bool trace_mode = false;

    // Check for --trace flag or auto-detect based on file extension
    for (int i = 3; i < argc; i++) {
        if (std::string(argv[i]) == "--trace") {
            trace_mode = true;
        }
    }

    // Auto-detect trace mode from file extension
    if (input_file.size() > 6 && input_file.substr(input_file.size() - 6) == ".trace") {
        trace_mode = true;
    }

    Simulator sim(input_file, cfgfile, trace_mode);
    if (!sim.load()) {
        std::cerr << "Failed to load program.\n";
        return 1;
    }
    sim.run();
    return 0;
}

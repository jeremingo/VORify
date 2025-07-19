#include "entry.h"
#include <iostream>
#include <vector>

std::vector<Entry> runExtractorWithPopen(const std::string& cmd);

int main() {
    std::string extractCmd = "../stations-within-range/stations-within-range 35 128 400 ../VOR.CSV";

    std::vector<Entry> entries = runExtractorWithPopen(extractCmd);

    for (const auto& e : entries) {
        std::cout << "ID: " << e.id << " Freq: " << e.frequency << "\n";
    }
}


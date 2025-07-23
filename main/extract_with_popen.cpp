#include "entry.h"
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <cstdio>

std::vector<Entry> runExtractorWithPopen(const std::string& cmd) {
    std::vector<Entry> entries;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        perror("popen failed");
        return entries;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        std::istringstream iss(buffer);
        Entry e;
        if (iss >> e.id >> e.location.lat >> e.location.lon >> e.frequency) {
            entries.push_back(e);
        } else {
            std::cerr << "Skipping malformed line: " << buffer;
        }
    }

    pclose(pipe);
    return entries;
}


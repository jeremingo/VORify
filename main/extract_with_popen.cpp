#include "entry.h"
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <cstdio>

std::vector<Entry> runExtractorWithPopen(const double lat, const double lon, const int range) {
  std::string cmd = "../stations-within-range/stations-within-range " + std::to_string(lat) + " " + std::to_string(lon) + " " + std::to_string(range) + " ../VOR.CSV" ;
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


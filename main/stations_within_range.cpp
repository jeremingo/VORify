#include "entry.h"
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <algorithm>
#include <memory>

std::vector<std::shared_ptr<Entry>> getStationsWithinRange(const double lat, const double lon, const int range) {
  std::string cmd = "../stations-within-range/stations-within-range " + std::to_string(lat) + " " + std::to_string(lon) + " " + std::to_string(range) + " ../VOR.CSV" ;
  std::vector<std::shared_ptr<Entry>> entries;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        perror("popen failed");
        return entries;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        std::istringstream iss(buffer);
        Entry e;
        if (iss >> e.name >> e.id >> e.location.lat >> e.location.lon >> e.frequency) {
          std::cout << e.id << std::endl;
            auto entry = std::make_shared<Entry>();
            entry->name = e.name;
            entry->id = e.id;
            entry->location = e.location;
            entry->frequency = e.frequency;
            std::cout << entry->id << std::endl;
            entries.push_back(entry);
        } else {
            std::cerr << "Skipping malformed line: " << buffer;
        }
    }

    pclose(pipe);
    return entries;
}

void updateStationsWithinRange(std::vector<std::shared_ptr<Entry>>& entries1, double lat, double lon, int range) {
    std::vector<std::shared_ptr<Entry>> entries2 = getStationsWithinRange(lat, lon, range);

    for (auto& newEntry : entries2) {
        auto it = std::find_if(entries1.begin(), entries1.end(), [&](const std::shared_ptr<Entry>& e) {
            return e->id == newEntry->id && e->frequency == newEntry->frequency;
        });

        if (it != entries1.end()) {
            newEntry->is_identified = (*it)->is_identified;
            newEntry->bearing = (*it)->bearing;
        }
    }

    entries1.swap(entries2);
}

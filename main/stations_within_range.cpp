#include "entry.h"
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <algorithm>
#include <memory>

using namespace std;

vector<shared_ptr<Entry>> getStationsWithinRange(const double lat, const double lon, const int range) {
  string cmd = "../stations-within-range/stations-within-range " + to_string(lat) + " " + to_string(lon) + " " + to_string(range) + " ../VOR.CSV" ;
  vector<shared_ptr<Entry>> entries;

  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    perror("popen failed");
    return entries;
  }

  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe)) {
    istringstream iss(buffer);
    Entry e;
    if (iss >> e.name >> e.id >> e.location.lat >> e.location.lon >> e.frequency) {
      cout << e.id << endl;
      auto entry = make_shared<Entry>();
      entry->name = e.name;
      entry->id = e.id;
      entry->location = e.location;
      entry->frequency = e.frequency;
      cout << entry->id << endl;
      entries.push_back(entry);
    } else {
      cerr << "Skipping malformed line: " << buffer;
    }
  }

  pclose(pipe);
  return entries;
}

void updateStationsWithinRange(vector<shared_ptr<Entry>>& entries1, double lat, double lon, int range) {
  vector<shared_ptr<Entry>> entries2 = getStationsWithinRange(lat, lon, range);

  for (auto& newEntry : entries2) {
    auto it = find_if(entries1.begin(), entries1.end(), [&](const shared_ptr<Entry>& e) {
        return e->id == newEntry->id && e->frequency == newEntry->frequency;
        });

    if (it != entries1.end()) {
      newEntry->is_identified = (*it)->is_identified;
      newEntry->bearing = (*it)->bearing;
    }
  }

  entries1.swap(entries2);
}

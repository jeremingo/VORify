#include "entry.h"
#include <iostream>
#include <vector>
#include <optional>
#include <sstream>
#include <cstdio>
#include <thread>
#include <mutex>
#include <chrono>
#include <memory>

using namespace std;

string buildIntersectionCommand(const vector<shared_ptr<Entry>>& entries) {
  string cmd = "../intersection/intersection ";
  for (const auto& entry : entries) {
    if (entry->is_identified && entry->bearing.has_value() && chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now()  - entry->bearing->timestamp).count() <= 15) {
      cmd += entry->location.lat + "," + entry->location.lon + "," + to_string(entry->bearing->value) + " ";
    }
  }
  cout << cmd << endl;
  return cmd;
}

optional<Location> intersection(const vector<shared_ptr<Entry>>& entries) {
  string cmd = buildIntersectionCommand(entries);
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    cerr << "Failed to run command: " << cmd << '\n';
    return nullopt;
  }

  char buffer[128];
  string result;
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result += buffer;
  }

  int status = pclose(pipe);
  if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
    cerr << "Command exited with code " << WEXITSTATUS(status) << '\n';
    return nullopt;
  }

  istringstream iss(result);
  double lat, lon;
  if (iss >> lat >> lon) {
    return Location{to_string(lat), to_string(lon)};
  } else {
    cerr << "Failed to parse intersection output: " << result << '\n';
    return nullopt;
  }
}

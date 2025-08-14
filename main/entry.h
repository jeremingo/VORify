#pragma once
#include <string>
#include <optional>
#include <chrono>

using namespace std;

struct BearingInfo {
  double value;
  chrono::steady_clock::time_point timestamp;
};

struct Location {
  string lat;
  string lon;

  bool operator==(const Location& other) const {
    return lat == other.lat && lon == other.lon;
  }
};

struct Entry {
  string name;
  string id;
  double frequency;
  Location location;
  bool is_identified = true;
  optional<BearingInfo> bearing;
  optional<double> distance;
};

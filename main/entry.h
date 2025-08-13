#pragma once
#include <string>
#include <optional>
#include <chrono>
#include <mutex>

struct BearingInfo {
  double value;
  std::chrono::steady_clock::time_point timestamp;
};

struct Location {
  std::string lat;
  std::string lon;

  bool operator==(const Location& other) const {
    return lat == other.lat && lon == other.lon;
  }
};

struct Entry {
  std::string name;
  std::string id;
  double frequency;
  Location location;
  bool is_identified = true;
  std::optional<BearingInfo> bearing;
  std::optional<double> distance;
  std::mutex mutex;
};

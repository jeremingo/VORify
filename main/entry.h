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
};

struct Entry {
    std::string id;
    double frequency;
    Location location;
    bool is_identified = true;
    std::optional<BearingInfo> bearing;
  std::mutex mutex;
};

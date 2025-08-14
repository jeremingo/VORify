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

std::string buildIntersectionCommand(const std::vector<std::shared_ptr<Entry>>& entries) {
  std::string cmd = "../intersection/intersection ";
  for (const auto& entry : entries) {
    if (entry->is_identified && entry->bearing.has_value() && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()  - entry->bearing->timestamp).count() <= 15) {
      cmd += entry->location.lat + "," + entry->location.lon + "," + std::to_string(entry->bearing->value) + " ";
    }
  }
  std::cout << cmd << std::endl;
  return cmd;
}

std::optional<Location> intersection(const std::vector<std::shared_ptr<Entry>>& entries) {
  std::string cmd = buildIntersectionCommand(entries);
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    std::cerr << "Failed to run command: " << cmd << '\n';
    return std::nullopt;
  }

  char buffer[128];
  std::string result;
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result += buffer;
  }

  int status = pclose(pipe);
  if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
    std::cerr << "Command exited with code " << WEXITSTATUS(status) << '\n';
    return std::nullopt;
  }

  std::istringstream iss(result);
  double lat, lon;
  if (iss >> lat >> lon) {
    return Location{std::to_string(lat), std::to_string(lon)};
  } else {
    std::cerr << "Failed to parse intersection output: " << result << '\n';
    return std::nullopt;
  }
}

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
#include <GeographicLib/Geodesic.hpp>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <boost/process.hpp>
#include <boost/asio.hpp>

using namespace GeographicLib;

std::string generateNMEA(double lat, double lon);
std::vector<std::shared_ptr<Entry>> getStationsWithinRange(const double lat, const double lon, const int range);
std::optional<double> calculateBearing(double frequency);
std::optional<Location> intersection(const std::vector<std::shared_ptr<Entry>>& entries);
std::string entriesToJson(const std::vector<std::shared_ptr<Entry>>& entries, const std::optional<Location>& location);
void updateStationsWithinRange(std::vector<std::shared_ptr<Entry>>& entries1, double lat, double lon, int range);

FILE* startBluetoothServer() {
  FILE* pipe = popen("../bluetooth-server/bluetooth-server", "w");
  if (!pipe) {
    std::cerr << "Failed to start bluetooth server\n";
    return nullptr;
  }
  return pipe;
}

void sendToBluetooth(FILE* pipe, const std::string& data) {
  if (!pipe) return;
  fputs(data.c_str(), pipe);
  fflush(pipe);
  // Pipe stays open for reuse
}

std::mutex entriesMutex;
std::mutex locationMutex;
std::mutex originLocationMutex;

void startJSONOutput(std::vector<std::shared_ptr<Entry>>& entries,
    std::optional<Location>& location,
    std::optional<Location>& origin_location,
    bool& running) {
  std::thread([&entries, &location, &origin_location, &running]() {
      boost::asio::io_context io;

      boost::process::opstream child_stdin;
      boost::process::ipstream child_stdout;

      boost::process::child python_process(
          "../venv/bin/python3 ../ui/ui.py",
          boost::process::std_in < child_stdin,
          boost::process::std_out > child_stdout,
          io
          );

      // Reader thread
      std::thread reader([&location, &origin_location, &child_stdout]() {
          std::string line;
          while (std::getline(child_stdout, line)) {
          double lat, lon;
          if (sscanf(line.c_str(), "%lf %lf", &lat, &lon) == 2) {
          std::lock_guard<std::mutex> locationLock(locationMutex);
          std::lock_guard<std::mutex> originLocationLock(originLocationMutex);
          origin_location = Location{std::to_string(lat), std::to_string(lon)};
          location = std::nullopt;
          std::cout << "Updated origin_location to: " << lat << ", " << lon << std::endl;
          }
          }
          });

      // Writer loop
      while (running && python_process.running()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::lock_guard<std::mutex> entriesLock(entriesMutex);
        std::string json = entriesToJson(entries, location);
        child_stdin << json << std::endl;
      }

      child_stdin.pipe().close();
      python_process.wait();
      reader.join();
  }).detach();
}

void startBearingUpdater(std::vector<std::shared_ptr<Entry>>& entries, bool& running) {
  std::thread([&entries, &running]() {
      while (running) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      auto now = std::chrono::steady_clock::now();
      std::lock_guard<std::mutex> entriesLock(entriesMutex);

      if (entries.empty()) {
      continue;
      }
      auto it = std::find_if(entries.begin(), entries.end(), [now](const std::shared_ptr<Entry>& entry) {
          std::lock_guard<std::mutex> entryLock(entry->mutex);
          return entry->is_identified &&
          (!entry->bearing.has_value() ||
           std::chrono::duration_cast<std::chrono::seconds>(now - entry->bearing->timestamp).count() >= 15);
          });

      if (it != entries.end()) {
      std::unique_lock<std::mutex> entryLock((*it)->mutex);
      double frequency = (*it)->frequency;
      entryLock.unlock();

      std::optional<double> bearing = calculateBearing(frequency);

      if(bearing) {
        entryLock.lock();
        (*it)->bearing = BearingInfo{*bearing, std::chrono::steady_clock::now()};
        entryLock.unlock();
      }
      }
      }
  }).detach();
}


void startStationsUpdater(std::vector<std::shared_ptr<Entry>>& entries, std::optional<Location>& location, std::optional<Location>& origin_location, std::optional<Location>& last_search_location, bool& running) {
  std::thread([&entries, &location, &origin_location, &last_search_location, &running]() {
      while (running) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      //std::lock_guard<std::mutex> locationLock(locationMutex);
      std::lock_guard<std::mutex> originLocationLock(originLocationMutex);
      if (origin_location) {
      std::cout << "UPDATING STATIONS" << std::endl;

      std::lock_guard<std::mutex> entriesLock(entriesMutex);

      last_search_location = origin_location;

      origin_location = std::nullopt;

      updateStationsWithinRange(entries, std::stod(origin_location->lat), std::stod(origin_location->lon), 200);
      }
      }
      }).detach();
}

void startBluetoothServer(std::optional<Location>& location, bool& running) {
  std::thread([&location, &running]() {
      FILE* bluetoothPipe = startBluetoothServer();
      if (!bluetoothPipe) return 1;
      while (running) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      std::lock_guard<std::mutex> locationLock(locationMutex);
      if (location) {
      sendToBluetooth(bluetoothPipe, generateNMEA(std::stod(location->lat), std::stod(location->lon)));
      }
      }
      }).detach();
}

double computeDistance(double lat1, double lon1, double lat2, double lon2) {
  const Geodesic& geod = Geodesic::WGS84();
  double s12;
  geod.Inverse(lat1, lon1, lat2, lon2, s12);
  return s12 / 1000.0; // convert meters to kilometers
}

int main() {
  std::vector<std::shared_ptr<Entry>> entries; 
  std::optional<Location> location = std::nullopt;
  std::optional<Location> origin_location = std::nullopt;
  std::optional<Location> last_search_location = std::nullopt;
  bool running = true;

  startJSONOutput(entries, location, origin_location, running);
  startBearingUpdater(entries, running);
  startStationsUpdater(entries, location, origin_location, last_search_location, running);
  startBluetoothServer(location, running);

  while (running) {
    std::this_thread::sleep_for(std::chrono::seconds(4));

    std::lock_guard<std::mutex> entriesLock(entriesMutex);

    int count = 0;
    for (auto& entry : entries) {
      std::lock_guard<std::mutex> entryLock(entry->mutex);
      if (entry->is_identified && entry->bearing.has_value() && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - entry->bearing->timestamp).count() <= 15)
        ++count;
    }

    if (count >= 2) {
      std::cout << "intersecting " << count << std::endl;
      std::lock_guard<std::mutex> locationLock(locationMutex);
      location = intersection(entries);
      if (location) {
        std::cout << location->lat << " " << location->lon << "\n";

        for (auto& entry : entries) {
          std::lock_guard<std::mutex> entryLock(entry->mutex);
          entry->distance = computeDistance(
              std::stod(location->lat),
              std::stod(location->lon),
              std::stod(entry->location.lat),
              std::stod(entry->location.lon));
        }
      }
      else {
        for (auto& entry : entries) {
          std::lock_guard<std::mutex> entryLock(entry->mutex);
          entry->distance = std::nullopt;
        }
      }
    }
  }

  return 0;
}


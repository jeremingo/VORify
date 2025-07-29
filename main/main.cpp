#include "entry.h"
#include <iostream>
#include <vector>
#include <optional>
#include <sstream>
#include <cstdio>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <memory>
#include <GeographicLib/Geodesic.hpp>

using namespace GeographicLib;

std::string generateNMEA(double lat, double lon);
std::vector<std::shared_ptr<Entry>> getStationsWithinRange(const double lat, const double lon, const int range);
std::optional<double> calculateBearing(double frequency);
std::optional<Location> intersection(const std::vector<std::shared_ptr<Entry>>& entries);
std::string entriesToJson(const std::vector<std::shared_ptr<Entry>>& entries);
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

std::shared_mutex entriesMutex;
std::mutex locationMutex;

void startJSONOutput(std::vector<std::shared_ptr<Entry>>& entries, bool& running) {
  std::thread([&entries, &running]() {
    std::string command = "python3 ../ui/ui.py";
    FILE* python = popen(command.c_str(), "w");
    if (!python) {
      std::cerr << "Failed to start Python script" << std::endl;
      return;
    }

    while (running) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      std::shared_lock<std::shared_mutex> entriesLock(entriesMutex);
      std::string json = entriesToJson(entries);
      fprintf(python, "%s\n", json.c_str());
      fflush(python);
    }

    pclose(python);
  }).detach();
}

void startBearingUpdater(std::vector<std::shared_ptr<Entry>>& entries, bool& running) {
    std::thread([&entries, &running]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            auto now = std::chrono::steady_clock::now();
            std::shared_lock<std::shared_mutex> entriesLock(entriesMutex);

            for (auto& entry : entries) {
              std::unique_lock<std::mutex> entryLock(entry->mutex);
                if (entry->is_identified &&
                    (!entry->bearing.has_value() ||
                     std::chrono::duration_cast<std::chrono::seconds>(now - entry->bearing->timestamp).count() >= 8)) {
                  entryLock.unlock();
                  std::cout << "getting bearing " << entry->id << std::endl;
                    std::optional<double> bearing = calculateBearing(entry->frequency);

                    if (bearing) {
                      entryLock.lock();
                        entry->bearing = BearingInfo{*bearing, std::chrono::steady_clock::now()};
                        entryLock.unlock();
                    }
                }
            }
        }
    }).detach();
}


void startStationsUpdater(std::vector<std::shared_ptr<Entry>>& entries, std::optional<Location>& location, bool& running) {
    std::thread([&entries, &location, &running]() {
        while (running) {
          std::this_thread::sleep_for(std::chrono::seconds(60));
          std::lock_guard<std::mutex> locationLock(locationMutex);
          if (location) {
            std::cout << "UPDATING STATIONS" << std::endl;

            std::unique_lock<std::shared_mutex> entriesLock(entriesMutex);

            updateStationsWithinRange(entries, std::stod(location->lat), std::stod(location->lon), 400);
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
    std::vector<std::shared_ptr<Entry>> entries = getStationsWithinRange(35.0, 128.0, 400);
    std::optional<Location> location = std::nullopt;
    bool running = true;

    startJSONOutput(entries, running);
    startBearingUpdater(entries, running);
    startStationsUpdater(entries, location, running);
    startBluetoothServer(location, running);

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(4));

        std::shared_lock<std::shared_mutex> entriesLock(entriesMutex);

        int count = 0;
        for (auto& entry : entries) {
            std::lock_guard<std::mutex> entryLock(entry->mutex);
            if (entry->is_identified && entry->bearing.has_value() && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - entry->bearing->timestamp).count() <= 8)
                ++count;
        }

        if (count >= 2) {
          std::cout << "intersecting " << count << std::endl;
          std::lock_guard<std::mutex> locationLock(locationMutex);
          location = intersection(entries);
          if (location) {
            std::ostringstream out;
            out << location->lat << " " << location->lon << "\n";
            std::cout << out.str();

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


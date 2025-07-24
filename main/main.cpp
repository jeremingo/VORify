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

std::string generateRMC(double lat, double lon);
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
          if (location) {
            std::cout << "UPDATING STATIONS" << std::endl;

            std::unique_lock<std::shared_mutex> entriesLock(entriesMutex);

            updateStationsWithinRange(entries, std::stod(location->lat), std::stod(location->lon), 400);
          }
        }
    }).detach();
}

int main() {
    FILE* bluetoothPipe = startBluetoothServer();
    if (!bluetoothPipe) return 1;

    std::vector<std::shared_ptr<Entry>> entries = getStationsWithinRange(35.0, 128.0, 400);
    std::optional<Location> location = std::nullopt;
    bool running = true;

    startJSONOutput(entries, running);
    startBearingUpdater(entries, running);
    startStationsUpdater(entries, location, running);

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
          location = intersection(entries);
          if (location) {
            std::ostringstream out;
            out << location->lat << " " << location->lon << "\n";
            std::cout << out.str();
            sendToBluetooth(bluetoothPipe, generateRMC(std::stod(location->lat), std::stod(location->lon)));
          }
        }
    }

    return 0;
}


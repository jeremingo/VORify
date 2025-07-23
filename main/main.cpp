#include "entry.h"
#include <iostream>
#include <vector>
#include <optional>
#include <sstream>
#include <cstdio>
#include <thread>
#include <mutex>
#include <chrono>

std::string generateRMC(double lat, double lon);
std::vector<Entry> getStationsWithinRange(const double lat, const double lon, const int range);
std::optional<double> calculateBearing(double frequency);
std::optional<Location> runCommandAndGetOutput(const std::vector<Entry>& entries);

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

std::mutex entryMutex;

void startBearingUpdater(std::vector<Entry>& entries, bool& running) {
    std::thread([&entries, &running]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            auto now = std::chrono::steady_clock::now();

            for (auto& entry : entries) {
                if (entry.is_identified &&
                    (!entry.bearing.has_value() ||
                     std::chrono::duration_cast<std::chrono::seconds>(now - entry.bearing->timestamp).count() >= 8)) {
                  std::cout << "getting bearing " << entry.id << std::endl;
                    std::optional<double> bearing = calculateBearing(entry.frequency);

                    if (bearing) {
                        std::lock_guard<std::mutex> lock(entryMutex);
                        entry.bearing = BearingInfo{*bearing, std::chrono::steady_clock::now()};
                    }
                }
            }
        }
    }).detach();
}

int main() {
    FILE* bluetoothPipe = startBluetoothServer();
    if (!bluetoothPipe) return 1;

    std::vector<Entry> entries = getStationsWithinRange(35.0, 128.0, 400);
    bool running = true;

    startBearingUpdater(entries, running);

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(4));

        std::lock_guard<std::mutex> lock(entryMutex);

        int count = 0;
        for (const auto& entry : entries) {
            if (entry.is_identified && entry.bearing.has_value() && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - entry.bearing->timestamp).count() <= 8)
                ++count;
        }

        if (count >= 2) {
          std::cout << "intersecting " << count << std::endl;
          std::optional<Location> location = runCommandAndGetOutput(entries);
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


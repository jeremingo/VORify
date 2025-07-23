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
std::vector<Entry> runExtractorWithPopen(const double lat, const double lon, const int range);
std::optional<double> calculateBearing(double frequency);

std::optional<Location> runCommandAndGetOutput(const std::string& cmd) {
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

FILE* startBluetoothServer() {
    FILE* pipe = popen("../bluetooth-server/bluetooth-server", "w");
    if (!pipe) {
        std::cerr << "Failed to start bluetooth server\n";
        return nullptr;
    }
    return pipe;
}

std::string buildIntersectionCommand(const std::vector<Entry>& entries) {
    std::string cmd = "../intersection/intersection ";
    for (const auto& entry : entries) {
        if (entry.is_identified && entry.bearing.has_value() && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()  - entry.bearing->timestamp).count() <= 8) {
            cmd += entry.location.lat + "," + entry.location.lon + "," + std::to_string(entry.bearing->value) + " ";
        }
    }
    return cmd;
}

void printEntries(const std::vector<Entry>& entries) {
    for (const auto& e : entries) {
        std::cout << "ID: " << e.id << " Freq: " << e.frequency
                  << " Bearing: " << (e.bearing ? std::to_string(e.bearing->value) : "N/A") << "\n";
    }
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

    std::vector<Entry> entries = runExtractorWithPopen(35.0, 128.0, 400);
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
          std::string intersectionCmd = buildIntersectionCommand(entries);
          std::optional<Location> location = runCommandAndGetOutput(intersectionCmd);
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


#include "entry.h"
#include <iostream>
#include <vector>
#include <optional>
#include <sstream>
#include <cstdio>
#include <thread>
#include <mutex>
#include <chrono>

std::vector<Entry> runExtractorWithPopen(const std::string& cmd);
std::optional<double> runExecutableAndGetDouble(double frequency);
std::string runCommandAndGetOutput(const std::string& cmd);

std::optional<double> runExecutableAndGetDouble(double frequency) {
    std::string cmd = "../wrap-bearing-calculator/wrap-bearing-calculator ../bearing-calculator/vorify " + std::to_string(frequency);

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to run command\n";
        return std::nullopt;
    }

    char buffer[128];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    int status = pclose(pipe);

    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) != 0) {
            std::cerr << "Command exited with code " << WEXITSTATUS(status) << '\n';
            return std::nullopt;
        }
    } else if (WIFSIGNALED(status)) {
        std::cerr << "Command terminated by signal " << WTERMSIG(status) << '\n';
        return std::nullopt;
    }

    std::istringstream iss(result);
    double value;
    if (iss >> value) {
        return value;
    } else {
        std::cerr << "Failed to parse output as double: " << result << '\n';
        return std::nullopt;
    }
}

std::string runCommandAndGetOutput(const std::string& cmd) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to run command: " << cmd << '\n';
        return "";
    }

    char buffer[128];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    int status = pclose(pipe);
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        std::cerr << "Command exited with code " << WEXITSTATUS(status) << '\n';
    }

    return result;
}

FILE* startBluetoothServer() {
    FILE* pipe = popen("../bluetooth-server/bluetooth-server", "w");
    if (!pipe) {
        std::cerr << "Failed to start bluetooth server\n";
        return nullptr;
    }
    return pipe;
}

std::vector<Entry> processEntries() {
    std::string extractCmd = "../stations-within-range/stations-within-range 35 128 400 ../VOR.CSV";
    return runExtractorWithPopen(extractCmd);
}

std::string buildIntersectionCommand(const std::vector<Entry>& entries) {
    std::string cmd = "../intersection/intersection ";
    for (const auto& entry : entries) {
        if (entry.is_identified && entry.bearing.has_value() && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()  - entry.bearing->timestamp).count() <= 8) {
            cmd += entry.lat + "," + entry.lon + "," + std::to_string(entry.bearing->value) + " ";
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
                    std::optional<double> bearing = runExecutableAndGetDouble(entry.frequency);

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

    std::vector<Entry> entries = processEntries();
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
            std::string result = runCommandAndGetOutput(intersectionCmd);
            std::cout << result << std::endl;
            sendToBluetooth(bluetoothPipe, result);
        }
    }

    return 0;
}


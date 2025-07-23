#include "entry.h"
#include <iostream>
#include <vector>
#include <optional>
#include <sstream>
#include <cstdio>

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

// 1. Start Bluetooth server and return writable pipe
FILE* startBluetoothServer() {
    FILE* pipe = popen("../bluetooth-server/bluetooth-server", "w");
    if (!pipe) {
        std::cerr << "Failed to start bluetooth server\n";
        return nullptr;
    }
    return pipe;
}

// 2. Extract entries and compute bearings
std::vector<Entry> processEntries() {
    std::string extractCmd = "../stations-within-range/stations-within-range 35 128 400 ../VOR.CSV";
    std::vector<Entry> entries = runExtractorWithPopen(extractCmd);

    for (auto& entry : entries) {
        if (entry.is_identified) {
            std::optional<double> bearing = runExecutableAndGetDouble(entry.frequency);
            if (bearing) {
                entry.bearing = bearing;
            }
        }
    }

    return entries;
}

// 3. Build intersection command from valid entries
std::string buildIntersectionCommand(const std::vector<Entry>& entries) {
    std::string cmd = "../intersection/intersection ";
    for (const auto& entry : entries) {
        if (entry.is_identified && entry.bearing.has_value()) {
            cmd += entry.lat + "," + entry.lon + "," + std::to_string(entry.bearing.value()) + " ";
        }
    }
    return cmd;
}

// 4. Print entries to stdout
void printEntries(const std::vector<Entry>& entries) {
    for (const auto& e : entries) {
        std::cout << "ID: " << e.id << " Freq: " << e.frequency
                  << " Bearing: " << e.bearing.value_or(0) << "\n";
    }
}

// 5. Send result to bluetooth server
void sendToBluetooth(FILE* pipe, const std::string& data) {
    if (!pipe) return;
    fputs(data.c_str(), pipe);
    fflush(pipe);
    pclose(pipe);
}

// 6. Main
int main() {
    FILE* bluetoothPipe = startBluetoothServer();
    if (!bluetoothPipe) return 1;

    std::vector<Entry> entries = processEntries();

    std::string intersectionCmd = buildIntersectionCommand(entries);

    printEntries(entries);

    std::string result = runCommandAndGetOutput(intersectionCmd);
    std::cout << result;

    sendToBluetooth(bluetoothPipe, result);

    return 0;
}


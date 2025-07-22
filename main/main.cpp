#include "entry.h"
#include <iostream>
#include <vector>
#include <optional>
#include <memory>
#include <sstream>

std::vector<Entry> runExtractorWithPopen(const std::string& cmd);

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


int main() {
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

    std::string intersectionCmd = "../intersection/intersection ";

    for (auto& entry : entries) {
      if (entry.is_identified && entry.bearing.has_value()) {
        intersectionCmd += entry.lat + "," + entry.lon + "," + std::to_string(entry.bearing.value()) + " ";
      }
    }

    for (const auto& e : entries) {
        std::cout << "ID: " << e.id << " Freq: " << e.frequency << " Bearing: " << e.bearing.value_or(0) << "\n";
    }

    std::cout << intersectionCmd;
}


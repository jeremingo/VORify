#include "entry.h"
#include <iostream>
#include <vector>
#include <optional>
#include <sstream>
#include <cstdio>
#include <thread>
#include <mutex>
#include <chrono>

std::optional<double> calculateBearing(std::string id, double frequency) {
    std::string cmd = "../mock-bearing/mock-bearing " + id + " " + std::to_string(frequency);

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

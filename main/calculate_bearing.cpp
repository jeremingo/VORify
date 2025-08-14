#include "entry.h"
#include <iostream>
#include <vector>
#include <optional>
#include <sstream>
#include <cstdio>
#include <thread>
#include <mutex>
#include <chrono>

using namespace std;

optional<double> calculateBearing(string id, double frequency) {
  string cmd = "../mock-bearing/mock-bearing " + id + " " + to_string(frequency);

  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    cerr << "Failed to run command\n";
    return nullopt;
  }

  char buffer[128];
  string result;
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result += buffer;
  }

  int status = pclose(pipe);

  if (WIFEXITED(status)) {
    if (WEXITSTATUS(status) != 0) {
      cerr << "Command exited with code " << WEXITSTATUS(status) << '\n';
      return nullopt;
    }
  } else if (WIFSIGNALED(status)) {
    cerr << "Command terminated by signal " << WTERMSIG(status) << '\n';
    return nullopt;
  }

  istringstream iss(result);
  double value;
  if (iss >> value) {
    return value;
  } else {
    cerr << "Failed to parse output as double: " << result << '\n';
    return nullopt;
  }
}

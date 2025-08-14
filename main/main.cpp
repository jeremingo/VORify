#include "entry.h"
#include <iostream>
#include <vector>
#include <optional>
#include <sstream>
#include <cstdio>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <memory>
#include <GeographicLib/Geodesic.hpp>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <boost/process.hpp>
#include <boost/asio.hpp>

using namespace GeographicLib;
using namespace std;

string generateNMEA(double lat, double lon);
vector<shared_ptr<Entry>> getStationsWithinRange(const double lat, const double lon, const int range);
optional<double> calculateBearing(string id, double frequency);
optional<Location> intersection(const vector<shared_ptr<Entry>>& entries);
string entriesToJson(const vector<shared_ptr<Entry>>& entries, const optional<Location>& location);
void updateStationsWithinRange(vector<shared_ptr<Entry>>& entries1, double lat, double lon, int range);

FILE* startBluetoothServer() {
  FILE* pipe = popen("../bluetooth-server/bluetooth-server", "w");
  if (!pipe) {
    cerr << "Failed to start bluetooth server\n";
    return nullptr;
  }
  return pipe;
}

void sendToBluetooth(FILE* pipe, const string& data) {
  if (!pipe) return;
  fputs(data.c_str(), pipe);
  fflush(pipe);
  // Pipe stays open for reuse
}

mutex entriesMutex;
mutex locationMutex;
atomic<bool> stationChangeNeeded(false);

void startBluetoothServer(optional<Location>& location, bool& running) {
  thread([&location, &running]() {
    FILE* bluetoothPipe = startBluetoothServer();
    if (!bluetoothPipe) {
      return 1;
    }

    while (running) {
      this_thread::sleep_for(chrono::seconds(1));
      lock_guard<mutex> locationLock(locationMutex);
      if (location) {
        sendToBluetooth(bluetoothPipe, generateNMEA(stod(location->lat), stod(location->lon)));
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
  vector<shared_ptr<Entry>> entries; 
  optional<Location> location = nullopt;
  bool running = true;

  startBluetoothServer(location, running);

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
  thread reader([&entries, &location, &child_stdout, &child_stdin]() {
    string line;
    while (getline(child_stdout, line)) {
      double lat, lon;
      if (sscanf(line.c_str(), "%lf %lf", &lat, &lon) == 2) {
        stationChangeNeeded.store(true);

        cout << "UPDATING STATIONS" << endl;

        lock_guard<mutex> entriesLock(entriesMutex);

        lock_guard<mutex> locationLock(locationMutex);
        location = nullopt;
        cout << "Updated origin_location to: " << lat << ", " << lon << endl;

        updateStationsWithinRange(entries, lat, lon, 200);
        stationChangeNeeded.store(false);
        string json = entriesToJson(entries, location);
        child_stdin << json << endl;
      }
    }
  });

  string json = entriesToJson(entries, location);
  child_stdin << json << endl;
  while (running) {
    this_thread::sleep_for(chrono::milliseconds(300));

    lock_guard<mutex> entriesLock(entriesMutex);

    if (entries.size() == 0) {
      continue;
    }

    auto now = chrono::steady_clock::now();

    auto it = find_if(entries.begin(), entries.end(), [now](const shared_ptr<Entry>& entry) {
        return entry->is_identified &&
        (!entry->bearing.has_value() ||
         chrono::duration_cast<chrono::seconds>(now - entry->bearing->timestamp).count() >= 15);
        });

    if (it != entries.end()) {
      optional<double> bearing = calculateBearing((*it)->id, (*it)->frequency);

      if(bearing) {
        (*it)->bearing = BearingInfo{*bearing, chrono::steady_clock::now()};
      }
      else {
        entries.erase(it);
      }
    }
    else {
      continue;
    }

    int count = 0;
    for (auto& entry : entries) {
      if (entry->is_identified && entry->bearing.has_value() && chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - entry->bearing->timestamp).count() <= 15)
        ++count;
    }

    if (count >= 2) {
      cout << "intersecting " << count << endl;
      lock_guard<mutex> locationLock(locationMutex);
      location = intersection(entries);
      if (location) {
        cout << location->lat << " " << location->lon << "\n";

        updateStationsWithinRange(entries, stod(location->lat), stod(location->lon), 200);

        for (auto& entry : entries) {
          entry->distance = computeDistance(
              stod(location->lat),
              stod(location->lon),
              stod(entry->location.lat),
              stod(entry->location.lon));
        }
      }
      else {
        for (auto& entry : entries) {
          entry->distance = nullopt;
        }
      }
    }

    if (!stationChangeNeeded.load()) {
      string json = entriesToJson(entries, location);
      child_stdin << json << endl;
    }
  }

  child_stdin.pipe().close();
  python_process.wait();
  reader.join();

  return 0;
}


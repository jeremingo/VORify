#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <ctime>
#include <iomanip>

using namespace std;

const string BEARINGS_FILE = "../create-mock-data/output.txt";

// Helper to split a string by a delimiter
vector<string> split(const string& str, char delimiter) {
    vector<string> tokens;
    string token;
    istringstream iss(str);
    while (getline(iss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        //cerr << "Usage: " << argv[0] << " <VOR_ID> <FREQUENCY>\n";
        return 1;
    }

    string targetId = argv[1];
    string targetFreq = argv[2];

    // Read all lines from bearings file
    ifstream file(BEARINGS_FILE);
    if (!file.is_open()) {
        //cerr << "Error: Could not open " << BEARINGS_FILE << endl;
        return 1;
    }

    vector<string> lines;
    string line;
    while (getline(file, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    file.close();

    if (lines.empty()) {
        //cerr << "Error: Empty bearings file.\n";
        return 1;
    }

    // Get time now and compute segment
    auto now = chrono::system_clock::now();
    time_t now_c = chrono::system_clock::to_time_t(now);
    tm* local_tm = localtime(&now_c);

    int secondsSinceHour = local_tm->tm_min * 60 + local_tm->tm_sec;
    int segment = secondsSinceHour / 30;
    int lineIndex = segment % lines.size();

    // Extract that line
    string targetLine = lines[lineIndex];

    // Find matching station
    vector<string> stations = split(targetLine, ';');
    for (const auto& stationData : stations) {
        vector<string> parts = split(stationData, ',');
        if (parts.size() == 3) {
            string id = parts[0];
            string freq = parts[1];
            string bearing = parts[2];

            if (id == targetId && stod(freq) == stod(targetFreq)) {
                this_thread::sleep_for(chrono::seconds(5));
                cout << bearing << endl;
                return 0;
            }
        }
    }

    //cerr << "Station not found: " << targetId << " " << targetFreq << endl;
    this_thread::sleep_for(chrono::seconds(3));
    return 1;
}


#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cmath>

using namespace std;

const string BEARINGS_FILE = "../create-mock-data/output.txt";

vector<string> split(const string& str, char delimiter) {
    vector<string> tokens;
    string token;
    istringstream iss(str);
    while (getline(iss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

double interpolateBearing(double b1, double b2, double t) {
    // Normalize to 0..360
    b1 = fmod((b1 + 360.0), 360.0);
    b2 = fmod((b2 + 360.0), 360.0);

    // Handle wrap-around shortest direction
    double diff = b2 - b1;
    if (diff > 180.0) diff -= 360.0;
    if (diff < -180.0) diff += 360.0;

    double result = b1 + diff * t;
    if (result < 0.0) result += 360.0;
    if (result >= 360.0) result -= 360.0;

    return result;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        return 1;
    }

    string targetId = argv[1];
    string targetFreq = argv[2];

    ifstream file(BEARINGS_FILE);
    if (!file.is_open()) {
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
        return 1;
    }

    auto now = chrono::system_clock::now();
    time_t now_c = chrono::system_clock::to_time_t(now);
    tm* local_tm = localtime(&now_c);

    int secondsSinceHour = local_tm->tm_min * 60 + local_tm->tm_sec;
    int segment = secondsSinceHour / 30;
    int nextSegment = (segment + 1) % lines.size();

    double frac = (secondsSinceHour % 30) / 30.0; // fraction between current and next

    string line1 = lines[segment % lines.size()];
    string line2 = lines[nextSegment];

    auto getBearing = [&](const string& l) -> double {
        vector<string> stations = split(l, ';');
        for (const auto& stationData : stations) {
            vector<string> parts = split(stationData, ',');
            if (parts.size() == 3) {
                if (parts[0] == targetId && stod(parts[1]) == stod(targetFreq)) {
                    return stod(parts[2]);
                }
            }
        }
        return NAN;
    };

    double b1 = getBearing(line1);
    double b2 = getBearing(line2);

    if (!isnan(b1) && !isnan(b2)) {
        this_thread::sleep_for(chrono::seconds(5));
        double interpolated = interpolateBearing(b1, b2, frac);
        cout << fixed << setprecision(2) << interpolated << endl;
        return 0;
    }

    this_thread::sleep_for(chrono::seconds(3));
    return 1;
}


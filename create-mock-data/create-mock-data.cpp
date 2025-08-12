#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <GeographicLib/Geodesic.hpp>

using namespace std;
using namespace GeographicLib;

struct Coordinate {
    double lat;
    double lon;
};

struct Station {
    string id;
    double lat;
    double lon;
    string frequency;
};

// Parses a line like "32.123,-117.456"
bool parseCoordinateLine(const string& line, Coordinate& coord) {
    istringstream iss(line);
    string token;

    if (getline(iss, token, ',')) {
        coord.lat = stod(token);
        if (getline(iss, token, ',')) {
            coord.lon = stod(token);
            return true;
        }
    }
    return false;
}

// Parses a line like "ABC,32.123,-117.456,112.30"
bool parseStationLine(const string& line, Station& station) {
    istringstream iss(line);
    string token;

    if (getline(iss, station.id, ',') &&
        getline(iss, token, ',')) {
        station.lat = stod(token);
        if (getline(iss, token, ',')) {
            station.lon = stod(token);
            if (getline(iss, station.frequency, ',')) {
                return true;
            }
        }
    }
    return false;
}

double calculateBearing(double lat1, double lon1, double lat2, double lon2) {
    const Geodesic& geod = Geodesic::WGS84();
    double azimuth1, azimuth2, distance;
    geod.Inverse(lat1, lon1, lat2, lon2, distance, azimuth1, azimuth2);
    return azimuth1;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " <coordinates.txt> <stations.txt> <output.txt>\n";
        return 1;
    }

    ifstream coordFile(argv[1]);
    ifstream stationFile(argv[2]);
    ofstream outputFile(argv[3]);

    if (!coordFile || !stationFile || !outputFile) {
        cerr << "Error opening one of the files.\n";
        return 1;
    }

    vector<Coordinate> coordinates;
    vector<Station> stations;

    // Read coordinates
    string line;
    while (getline(coordFile, line)) {
        Coordinate coord;
        if (parseCoordinateLine(line, coord)) {
            coordinates.push_back(coord);
        }
    }

    // Read stations
    while (getline(stationFile, line)) {
        Station station;
        if (parseStationLine(line, station)) {
            stations.push_back(station);
        }
    }

    // For each coordinate, calculate bearing from each station
    for (const auto& coord : coordinates) {
        ostringstream oss;
        for (size_t i = 0; i < stations.size(); ++i) {
            const auto& station = stations[i];
            double bearing = calculateBearing(station.lat, station.lon, coord.lat, coord.lon);
            if (bearing < 0)
                bearing += 360.0;

            oss << station.id << "," << station.frequency << "," << fixed << setprecision(2) << bearing;
            if (i < stations.size() - 1)
                oss << ";";
        }
        outputFile << oss.str() << "\n";
    }

    cout << "Output written to " << argv[3] << "\n";
    return 0;
}


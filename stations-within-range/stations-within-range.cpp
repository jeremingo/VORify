#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>

constexpr double EARTH_RADIUS_KM = 6371.0;

// Structure to store VOR station data
struct VORStation {
    std::string type;
    std::string country;
    std::string name;
    std::string id;
    double lat;
    double lon;
    double elev;
    std::string mft;
    std::string ref;
    double freq;
    std::string unit;
    std::string chan;
    double decl;
    std::string north;
    std::string range;
    std::string kmm;
};

// Convert degrees to radians
double deg2rad(double deg) {
    return deg * M_PI / 180.0;
}

// Compute the distance using Haversine formula
double haversine(double lat1, double lon1, double lat2, double lon2) {
    double dlat = deg2rad(lat2 - lat1);
    double dlon = deg2rad(lon2 - lon1);

    lat1 = deg2rad(lat1);
    lat2 = deg2rad(lat2);

    double a = sin(dlat/2) * sin(dlat/2) +
               cos(lat1) * cos(lat2) *
               sin(dlon/2) * sin(dlon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    return EARTH_RADIUS_KM * c;
}

// Parse the CSV and return a vector of stations
std::vector<VORStation> readCSV(const std::string& filename) {
    std::ifstream file(filename);
    std::vector<VORStation> stations;

    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << "\n";
        return stations;
    }

    std::string line;
    std::getline(file, line); // Skip header

	// CSV file columns are "type, country, name, id, lat, lon, elev, mft, ref, freq, unit, chan, decl, north, range, kmm"
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        VORStation station;

        std::getline(ss, station.type, ',');
        std::getline(ss, station.country, ',');
        std::getline(ss, station.name, ',');
        std::getline(ss, station.id, ',');
        std::getline(ss, token, ','); station.lat = std::stod(token);
        std::getline(ss, token, ','); station.lon = std::stod(token);
        std::getline(ss, token, ','); station.elev = std::stod(token);
        std::getline(ss, station.mft, ',');
        std::getline(ss, station.ref, ',');
        std::getline(ss, token, ','); station.freq = std::stod(token);
        std::getline(ss, station.unit, ',');
        std::getline(ss, station.chan, ',');
        std::getline(ss, token, ','); station.decl = std::stod(token);
        std::getline(ss, station.north, ',');
        std::getline(ss, station.range, ',');
        std::getline(ss, station.kmm, ',');

        stations.push_back(station);
    }

    return stations;
}

int main() {
    // Change to your actual CSV file path
    std::string filename = "VOR.CSV";
    double target_lat = 32.6;
    double target_lon = 34.2;
    double range_km = 400.0;

    auto stations = readCSV(filename);

    std::vector<VORStation> nearby;
    for (const auto& station : stations) {
        double dist = haversine(target_lat, target_lon, station.lat, station.lon);
        if (dist <= range_km) {
            nearby.push_back(station);
        }
    }

    std::cout << "Stations within " << range_km << " km:\n";
    std::cout << "Count: " << nearby.size() << "\n\n";
    for (const auto& s : nearby) {
        std::cout << "Name: " << s.name
                  << ", ID: " << s.id
                  << ", Lat: " << s.lat
                  << ", Lon: " << s.lon
                  << ", Freq: " << s.freq
                  << ", Decl: " << s.decl << "\n";
    }

    return 0;
}


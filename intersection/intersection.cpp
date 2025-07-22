#include <iostream>
#include <vector>
#include <sstream>
#include <string>
#include <optional>
#include <cmath>

constexpr double DEG2RAD = M_PI / 180.0;
constexpr double RAD2DEG = 180.0 / M_PI;

struct Station {
    double lat, lon, bearing;
};

struct LatLon {
    double lat, lon;
};

// Vector 3D on unit sphere
struct Vec3 {
    double x, y, z;

    Vec3 operator+(const Vec3& b) const { return {x + b.x, y + b.y, z + b.z}; }
    Vec3 operator-(const Vec3& b) const { return {x - b.x, y - b.y, z - b.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    Vec3 cross(const Vec3& b) const {
        return {y * b.z - z * b.y,
                z * b.x - x * b.z,
                x * b.y - y * b.x};
    }
    double dot(const Vec3& b) const { return x * b.x + y * b.y + z * b.z; }
    double norm() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 normalize() const {
        double n = norm();
        return {x / n, y / n, z / n};
    }
};

// Parse input string "lat,lon,bearing"
std::optional<Station> parseStation(const std::string& input) {
    std::stringstream ss(input);
    std::string token;
    Station s;
    int count = 0;
    while (std::getline(ss, token, ',')) {
        try {
            double val = std::stod(token);
            if (count == 0) s.lat = val;
            else if (count == 1) s.lon = val;
            else if (count == 2) s.bearing = val;
            else break;
        } catch (...) {
            return std::nullopt;
        }
        ++count;
    }
    return (count == 3) ? std::optional<Station>{s} : std::nullopt;
}

Vec3 latLonToVector(double lat_deg, double lon_deg) {
    double lat = lat_deg * DEG2RAD;
    double lon = lon_deg * DEG2RAD;
    return {
        std::cos(lat) * std::cos(lon),
        std::cos(lat) * std::sin(lon),
        std::sin(lat)};
}

Vec3 bearingToDirectionVector(double lat_deg, double lon_deg, double bearing_deg) {
    double lat = lat_deg * DEG2RAD;
    double lon = lon_deg * DEG2RAD;
    double bearing = bearing_deg * DEG2RAD;

    // Local east and north vectors
    Vec3 east = {-std::sin(lon), std::cos(lon), 0};
    Vec3 north = {-std::sin(lat) * std::cos(lon),
                  -std::sin(lat) * std::sin(lon),
                   std::cos(lat)};
    // Direction vector is north rotated clockwise by bearing
    Vec3 dir = north * std::cos(bearing) + east * std::sin(bearing);
    return dir.normalize();
}

// Find intersection of two great circles defined by stations and bearings
std::optional<LatLon> intersectGreatCircles(const Station& s1, const Station& s2) {
    Vec3 p1 = latLonToVector(s1.lat, s1.lon);
    Vec3 d1 = bearingToDirectionVector(s1.lat, s1.lon, s1.bearing);
    Vec3 n1 = p1.cross(d1).normalize();

    Vec3 p2 = latLonToVector(s2.lat, s2.lon);
    Vec3 d2 = bearingToDirectionVector(s2.lat, s2.lon, s2.bearing);
    Vec3 n2 = p2.cross(d2).normalize();

    Vec3 i = n1.cross(n2);

    double norm_i = i.norm();
    if (norm_i < 1e-15) {
        // Lines are parallel or coincident - no unique intersection
        return std::nullopt;
    }

    Vec3 i1 = i * (1.0 / norm_i);
    Vec3 i2 = i1 * -1.0;

    // Choose the intersection point closer to both stations
    auto distToStations = [&](const Vec3& v) {
        return (v - p1).norm() + (v - p2).norm();
    };

    Vec3 intersectionVec = (distToStations(i1) < distToStations(i2)) ? i1 : i2;

    double lat = std::asin(intersectionVec.z) * RAD2DEG;
    double lon = std::atan2(intersectionVec.y, intersectionVec.x) * RAD2DEG;

    return LatLon{lat, lon};
}

LatLon averageIntersections(const std::vector<Station>& stations) {
    std::vector<LatLon> points;

    for (size_t i = 0; i < stations.size(); ++i) {
        for (size_t j = i + 1; j < stations.size(); ++j) {
            auto inter = intersectGreatCircles(stations[i], stations[j]);
            if (inter) points.push_back(*inter);
        }
    }

    LatLon avg{0, 0};
    if (!points.empty()) {
        for (const auto& p : points) {
            avg.lat += p.lat;
            avg.lon += p.lon;
        }
        avg.lat /= points.size();
        avg.lon /= points.size();
    }
    return avg;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " lat,lon,bearing lat,lon,bearing ...\n";
        return 1;
    }

    std::vector<Station> stations;
    for (int i = 1; i < argc; ++i) {
        auto s = parseStation(argv[i]);
        if (!s) {
            std::cerr << "Invalid input: " << argv[i] << "\n";
            return 1;
        }
        stations.push_back(*s);
    }

    LatLon result = averageIntersections(stations);

    if (result.lat == 0 && result.lon == 0) {
        std::cout << "No valid intersection found.\n";
    } else {
        std::cout << "Estimated Position: Lat = " << result.lat << ", Lon = " << result.lon << "\n";
    }

    return 0;
}


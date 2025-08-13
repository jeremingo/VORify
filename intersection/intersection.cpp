#include <iostream>
#include <vector>
#include <sstream>
#include <string>
#include <optional>
#include <cmath>
#include <tuple>
#include <GeographicLib/Geodesic.hpp>
#include <GeographicLib/GeodesicLine.hpp>


using namespace GeographicLib;

struct Station {
  double lat, lon, bearing;
};

struct LatLon {
  double lat, lon;
};


const double DEG2RAD = M_PI / 180.0;
const double RAD2DEG = 180.0 / M_PI;
const double EARTH_RADIUS = 6371000.0; // meters

// Convert lat/lon to 3D Cartesian
void latLonToXYZ(const LatLon& ll, double& x, double& y, double& z) {
  double latRad = ll.lat * DEG2RAD;
  double lonRad = ll.lon * DEG2RAD;
  x = cos(latRad) * cos(lonRad);
  y = cos(latRad) * sin(lonRad);
  z = sin(latRad);
}

// Convert 3D Cartesian to lat/lon
LatLon xyzToPosition(double x, double y, double z) {
  double hyp = sqrt(x * x + y * y);
  double lat = atan2(z, hyp) * RAD2DEG;
  double lon = atan2(y, x) * RAD2DEG;
  return {lat, lon};
}

// Compute the average position minimizing distance to all points
LatLon geographicMedian(const std::vector<LatLon>& points, int maxIterations = 100, double tolerance = 1e-12) {
  // Convert all to Cartesian
  std::vector<std::tuple<double, double, double>> xyzList;
  for (const auto& p : points) {
    double x, y, z;
    latLonToXYZ(p, x, y, z);
    xyzList.emplace_back(x, y, z);
  }

  // Initialize to centroid
  double x = 0, y = 0, z = 0;
  for (const auto& [xi, yi, zi] : xyzList) {
    x += xi; y += yi; z += zi;
  }
  x /= xyzList.size(); y /= xyzList.size(); z /= xyzList.size();

  for (int iter = 0; iter < maxIterations; ++iter) {
    double numX = 0, numY = 0, numZ = 0;
    double denom = 0;

    for (const auto& [xi, yi, zi] : xyzList) {
      double dx = x - xi, dy = y - yi, dz = z - zi;
      double dist = sqrt(dx * dx + dy * dy + dz * dz);
      if (dist == 0) continue; // skip to avoid division by 0

      double weight = 1.0 / dist;
      numX += xi * weight;
      numY += yi * weight;
      numZ += zi * weight;
      denom += weight;
    }

    double newX = numX / denom;
    double newY = numY / denom;
    double newZ = numZ / denom;

    double delta = sqrt((x - newX)*(x - newX) + (y - newY)*(y - newY) + (z - newZ)*(z - newZ));
    x = newX; y = newY; z = newZ;

    if (delta < tolerance)
      break;
  }

  return xyzToPosition(x, y, z);
}


// Normalize angle to [-180, 180)
double normalizeAngle(double angle) {
  return std::fmod(angle + 540.0, 360.0) - 180.0;
}

// Check if two azimuths are close enough
bool azimuthMatch(double a1, double a2, double threshold = 1e-6) {
  return std::abs(normalizeAngle(a1 - a2)) < threshold;
}

// Step along line1 and see when line2 points *to* that step point with the correct azimuth
bool findIntersection(
    const Geodesic& geod,
    double lat1, double lon1, double az1,
    double lat2, double lon2, double az2,
    double& latInt, double& lonInt)
{
  GeodesicLine line1 = geod.Line(lat1, lon1, az1);
  const double step = 10.0;         // meters
  const double maxDist = 400000.0; // meters (VOR range)

  for (double s = 0.0; s <= maxDist; s += step) {
    double plat, plon, dummyazi;
    line1.Position(s, plat, plon, dummyazi);  // point on line 1

    double invDist, az2ToPoint, unusedRevAz;
    geod.Inverse(lat2, lon2, plat, plon, invDist, az2ToPoint, unusedRevAz);  // FROM line2 point TO current point

    double azErr = normalizeAngle(az2ToPoint - az2);

    if (std::abs(azErr) < 1e-2) {  // 0.00001 deg ~ 1.1 meters precision
      latInt = plat;
      lonInt = plon;
      return true;
    }
  }

  return false;
}

void calc_position(const std::vector<Station>& stations, double& outLat, double& outLon) {

  const Geodesic& geod = Geodesic::WGS84();
  if (stations.size() < 2) { // If we received just one VOR, position cannot ne calculated
    outLat = 99.0;
    outLon = 99.0;
  }

  else { // we received more than one VOR. create a vector of possible posisions from every pair of VORs.
    std::vector<LatLon> results;
    LatLon position;

    for (auto it1 = stations.begin(); it1 != stations.end(); ++it1) {
      for (auto it2 = std::next(it1); it2 != stations.end(); ++it2) {
        if (findIntersection(geod,
              it1->lat, it1->lon, it1->bearing,
              it2->lat, it2->lon, it2->bearing,
              outLat, outLon)) {
          results.push_back({outLat, outLon});
        }
      }
    }

    if (results.size() == 0) {
      return;
    }
    else {
      if (results.size() == 1) { //just one intersection
        outLat = results[0].lat;
        outLon = results[0].lon;
      }
      else { //more than one intersection, compute median
        position = geographicMedian(results, 100, 1e-12);
        outLat = position.lat;
        outLon = position.lon;
      }
      std::cout << outLat << " " << outLon << "\n";
    }
  }
  return ;
}

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

  double outLat, outLon;

  calc_position(stations, outLat, outLon);

  return 0;
}


#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstdlib>
#include <GeographicLib/Geodesic.hpp>

using namespace std;
using namespace GeographicLib;

struct Station {
  string id;
  string freq;
  double lat;
  double lon;
  double distance;
};

double computeDistance(double lat1, double lon1, double lat2, double lon2) {
  const Geodesic& geod = Geodesic::WGS84();
  double s12;
  geod.Inverse(lat1, lon1, lat2, lon2, s12);
  return s12 / 1000.0; // meters to km
}

int main(int argc, char* argv[]) {
  if (argc < 7 || ((argc - 3) % 4 != 0)) {
    cerr << "Usage: " << argv[0] << " <origin_lat> <origin_lon> <id1> <freq1> <lat1> <lon1> [<id2> <freq2> <lat2> <lon2> ...]\n";
    return 1;
  }

  double origin_lat = atof(argv[1]);
  double origin_lon = atof(argv[2]);

  vector<Station> stations;

  for (int i = 3; i + 3 < argc; i += 4) {
    Station s;
    s.id = argv[i];
    s.freq = argv[i + 1];
    s.lat = atof(argv[i + 2]);
    s.lon = atof(argv[i + 3]);
    s.distance = computeDistance(origin_lat, origin_lon, s.lat, s.lon);
    stations.push_back(s);
  }

  for (const auto& s : stations) {
    cout << s.id << " " << s.freq << " " << fixed << setprecision(3) << s.distance << "\n";
  }

  return 0;
}


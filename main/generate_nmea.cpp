#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <ctime>

using namespace std;

string generateNMEA(double lat, double lon) {
  double speedKnots = 0.0;
  double courseDeg = 0.0;
  ostringstream sentence;

  // Time and date
  time_t now = time(nullptr);
  tm *utc = gmtime(&now);

  // Latitude
  char latHemi = (lat >= 0) ? 'N' : 'S';
  lat = fabs(lat);
  int latDeg = static_cast<int>(lat);
  double latMin = (lat - latDeg) * 60.0;

  // Longitude
  char lonHemi = (lon >= 0) ? 'E' : 'W';
  lon = fabs(lon);
  int lonDeg = static_cast<int>(lon);
  double lonMin = (lon - lonDeg) * 60.0;

  // Build body of the sentence
  ostringstream body;
  body << "GPGGA,"
    << setfill('0') << setw(2) << utc->tm_hour
    << setw(2) << utc->tm_min
    << setw(2) << utc->tm_sec << ".00,"
    << setw(2) << latDeg << fixed << setprecision(6)
    << setw(7) << latMin << "," << latHemi << ","
    << setw(3) << lonDeg << fixed << setprecision(6)
    << setw(7) << lonMin << "," << lonHemi << ","
    << "1,10,1.0,33.6,M,19.0,M,,";

  string bodyStr = body.str();

  // Calculate checksum
  unsigned char checksum = 0;
  for (char ch : bodyStr) {
    checksum ^= ch;
  }

  // Assemble final sentence
  sentence << "$" << bodyStr << "*" << uppercase << hex
    << setw(2) << setfill('0') << static_cast<int>(checksum) << "\n";

  return sentence.str();
}

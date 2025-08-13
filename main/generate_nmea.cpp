#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <ctime>

std::string generateNMEA(double lat, double lon) {
  double speedKnots = 0.0;
  double courseDeg = 0.0;
  std::ostringstream sentence;

  // Time and date
  std::time_t now = std::time(nullptr);
  std::tm *utc = std::gmtime(&now);

  // Latitude
  char latHemi = (lat >= 0) ? 'N' : 'S';
  lat = std::fabs(lat);
  int latDeg = static_cast<int>(lat);
  double latMin = (lat - latDeg) * 60.0;

  // Longitude
  char lonHemi = (lon >= 0) ? 'E' : 'W';
  lon = std::fabs(lon);
  int lonDeg = static_cast<int>(lon);
  double lonMin = (lon - lonDeg) * 60.0;

  // Build body of the sentence
  std::ostringstream body;
  body << "GPGGA,"
    << std::setfill('0') << std::setw(2) << utc->tm_hour
    << std::setw(2) << utc->tm_min
    << std::setw(2) << utc->tm_sec << ".00,"
    << std::setw(2) << latDeg << std::fixed << std::setprecision(6)
    << std::setw(7) << latMin << "," << latHemi << ","
    << std::setw(3) << lonDeg << std::fixed << std::setprecision(6)
    << std::setw(7) << lonMin << "," << lonHemi << ","
    << "1,10,1.0,33.6,M,19.0,M,,";

  std::string bodyStr = body.str();

  // Calculate checksum
  unsigned char checksum = 0;
  for (char ch : bodyStr) {
    checksum ^= ch;
  }

  // Assemble final sentence
  sentence << "$" << bodyStr << "*" << std::uppercase << std::hex
    << std::setw(2) << std::setfill('0') << static_cast<int>(checksum) << "\n";

  return sentence.str();
}

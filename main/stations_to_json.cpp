#include "entry.h"
#include <sstream>
#include <iomanip>
#include <vector>
#include <memory>

using namespace std;

string entriesToJson(const vector<shared_ptr<Entry>>& entries, const optional<Location>& location) {
  ostringstream oss;
  oss << "{ \"location\": ";
  if (location.has_value()) {
    oss << "{ \"lat\": " << location->lat << ", \"lon\": " << location->lon << " }, ";
  }
  else {
    oss << "null, ";
  }

  auto now = chrono::steady_clock::now();

  oss << "\"stations\": [";
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& e = entries[i];
    oss << "{";
    oss << "\"name\":\"" << e->name << "\",";
    oss << "\"id\":\"" << e->id << "\",";
    oss << "\"frequency\":" << e->frequency << ",";
    oss << "\"location\":{\"lat\":\"" << e->location.lat << "\",\"lon\":\"" << e->location.lon << "\"},";
    oss << "\"is_identified\":" << (e->is_identified ? "true" : "false") << ",";
    if (e->bearing.has_value() && 
         chrono::duration_cast<chrono::seconds>(now - e->bearing->timestamp).count() <= 17) {
      auto seconds = chrono::duration_cast<chrono::duration<double>>(
          e->bearing->timestamp.time_since_epoch()).count();
      oss << "\"bearing\":{\"value\":" << e->bearing->value << ",\"timestamp\":" << fixed << setprecision(6) << seconds << "},";
    } else {
      oss << "\"bearing\":null,";
    }
    if (e->distance.has_value()) {
      oss << "\"distance\":" << fixed << setprecision(3) << e->distance.value();
    } else {
      oss << "\"distance\":null";
    }
    oss << "}";
    if (i < entries.size() - 1) {
      oss << ",";
    }
  }
  oss << "] }";

  return oss.str();
}

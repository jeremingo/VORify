#include "entry.h"
#include <sstream>
#include <iomanip>
#include <vector>
#include <memory>

std::string entriesToJson(const std::vector<std::shared_ptr<Entry>>& entries) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        oss << "{";
        oss << "\"id\":\"" << e->id << "\",";
        oss << "\"frequency\":" << e->frequency << ",";
        oss << "\"location\":{\"lat\":\"" << e->location.lat << "\",\"lon\":\"" << e->location.lon << "\"},";
        oss << "\"is_identified\":" << (e->is_identified ? "true" : "false") << ",";
        std::lock_guard<std::mutex> entryLock(e->mutex);
        if (e->bearing.has_value()) {
            auto seconds = std::chrono::duration_cast<std::chrono::duration<double>>(
                e->bearing->timestamp.time_since_epoch()).count();
            oss << "\"bearing\":{\"value\":" << e->bearing->value << ",\"timestamp\":" << std::fixed << std::setprecision(6) << seconds << "}";
        } else {
            oss << "\"bearing\":null";
        }
        oss << "}";
        if (i < entries.size() - 1) {
            oss << ",";
        }
    }
    oss << "]";
    return oss.str();
}

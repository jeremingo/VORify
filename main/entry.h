#pragma once
#include <string>
#include <optional>

struct Entry {
      std::string id;
          double frequency;
          bool is_identified = true;
          std::optional<double> bearing;
};


#pragma once
#include <string>
#include <optional>

struct Entry {
      std::string id;
      std::string lat;
      std::string lon;
          double frequency;
          bool is_identified = true;
          std::optional<double> bearing;
};


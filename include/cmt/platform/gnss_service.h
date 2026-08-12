#pragma once

#include "cmt/core/types.h"

#include <TinyGPSPlus.h>

#include <cstdint>

namespace cmt {

struct GnssSnapshot {
  GeoPoint point{};
  std::uint8_t satellites = 0;
  float hdop = 0.0F;
  std::uint32_t unix_time = 0;
  std::uint32_t day_key = 0;
  std::uint32_t updated_at_ms = 0;
};

class GnssService {
 public:
  bool begin();
  void update(std::uint32_t now_ms);
  const GnssSnapshot& snapshot() const;
  bool ready() const;

 private:
  TinyGPSPlus gps_;
  GnssSnapshot snapshot_{};
  bool ready_ = false;
};

}  // namespace cmt

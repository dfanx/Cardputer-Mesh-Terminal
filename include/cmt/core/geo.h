#pragma once

#include "cmt/core/types.h"

#include <cstdint>

namespace cmt {

struct RelativePosition {
  double distance_m = 0.0;
  double bearing_deg = 0.0;
  std::int16_t altitude_delta_m = 0;
  const char* direction = "--";
  bool valid = false;
};

bool isValidCoordinate(double latitude, double longitude);
double distanceMeters(const GeoPoint& from, const GeoPoint& to);
double bearingDegrees(const GeoPoint& from, const GeoPoint& to);
const char* cardinalDirection(double bearing_deg);
RelativePosition relativePosition(const GeoPoint& from, const GeoPoint& to);

}  // namespace cmt

#include "cmt/core/geo.h"

#include <cmath>

namespace cmt {
namespace {

constexpr double kEarthRadiusM = 6371008.8;
constexpr double kPi = 3.14159265358979323846;

double toRadians(const double degrees) { return degrees * kPi / 180.0; }

}  // namespace

bool isValidCoordinate(const double latitude, const double longitude) {
  return std::isfinite(latitude) && std::isfinite(longitude) &&
         latitude >= -90.0 && latitude <= 90.0 && longitude >= -180.0 &&
         longitude <= 180.0;
}

double distanceMeters(const GeoPoint& from, const GeoPoint& to) {
  if (!from.valid || !to.valid ||
      !isValidCoordinate(from.latitude, from.longitude) ||
      !isValidCoordinate(to.latitude, to.longitude)) {
    return -1.0;
  }
  const double lat1 = toRadians(from.latitude);
  const double lat2 = toRadians(to.latitude);
  const double delta_lat = lat2 - lat1;
  const double delta_lon = toRadians(to.longitude - from.longitude);
  const double sin_lat = std::sin(delta_lat / 2.0);
  const double sin_lon = std::sin(delta_lon / 2.0);
  const double a = sin_lat * sin_lat +
                   std::cos(lat1) * std::cos(lat2) * sin_lon * sin_lon;
  return 2.0 * kEarthRadiusM *
         std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
}

double bearingDegrees(const GeoPoint& from, const GeoPoint& to) {
  if (!from.valid || !to.valid ||
      !isValidCoordinate(from.latitude, from.longitude) ||
      !isValidCoordinate(to.latitude, to.longitude)) {
    return -1.0;
  }
  const double lat1 = toRadians(from.latitude);
  const double lat2 = toRadians(to.latitude);
  const double delta_lon = toRadians(to.longitude - from.longitude);
  const double y = std::sin(delta_lon) * std::cos(lat2);
  const double x = std::cos(lat1) * std::sin(lat2) -
                   std::sin(lat1) * std::cos(lat2) * std::cos(delta_lon);
  double bearing = std::atan2(y, x) * 180.0 / kPi;
  if (bearing < 0.0) {
    bearing += 360.0;
  }
  return bearing;
}

const char* cardinalDirection(const double bearing_deg) {
  if (!std::isfinite(bearing_deg) || bearing_deg < 0.0) {
    return "--";
  }
  static const char* kDirections[] = {"N", "NE", "E", "SE",
                                      "S", "SW", "W", "NW"};
  const int index =
      static_cast<int>(std::floor((bearing_deg + 22.5) / 45.0)) % 8;
  return kDirections[index];
}

RelativePosition relativePosition(const GeoPoint& from, const GeoPoint& to) {
  RelativePosition result{};
  result.distance_m = distanceMeters(from, to);
  result.bearing_deg = bearingDegrees(from, to);
  if (result.distance_m < 0.0 || result.bearing_deg < 0.0) {
    return result;
  }
  result.altitude_delta_m =
      static_cast<std::int16_t>(to.altitude_m - from.altitude_m);
  result.direction = cardinalDirection(result.bearing_deg);
  result.valid = true;
  return result;
}

}  // namespace cmt

#include "cmt/platform/gnss_service.h"

#include <Arduino.h>

#include <algorithm>
#include <cmath>

namespace cmt {
namespace {

constexpr int kGnssRxPin = 15;
constexpr int kGnssTxPin = 13;
constexpr std::uint32_t kGnssBaud = 115200;

std::int64_t daysFromCivil(int year, const unsigned month,
                          const unsigned day) {
  year -= month <= 2U;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
  const unsigned adjusted_month = month > 2U ? month - 3U : month + 9U;
  const unsigned day_of_year =
      (153U * adjusted_month + 2U) / 5U + day - 1U;
  const unsigned day_of_era =
      year_of_era * 365U + year_of_era / 4U - year_of_era / 100U + day_of_year;
  return era * 146097LL + static_cast<std::int64_t>(day_of_era) - 719468LL;
}

std::uint32_t unixTimestamp(TinyGPSDate& date, TinyGPSTime& time) {
  if (!date.isValid() || !time.isValid() || date.year() < 2020U) {
    return 0;
  }
  const std::int64_t days =
      daysFromCivil(date.year(), date.month(), date.day());
  const std::int64_t seconds =
      days * 86400LL + static_cast<std::int64_t>(time.hour()) * 3600LL +
      static_cast<std::int64_t>(time.minute()) * 60LL + time.second();
  if (seconds < 0 || seconds > 0xFFFFFFFFLL) {
    return 0;
  }
  return static_cast<std::uint32_t>(seconds);
}

}  // namespace

bool GnssService::begin() {
  Serial1.begin(kGnssBaud, SERIAL_8N1, kGnssRxPin, kGnssTxPin);
  ready_ = true;
  return true;
}

void GnssService::update(const std::uint32_t now_ms) {
  if (!ready_) {
    return;
  }
  while (Serial1.available() > 0) {
    gps_.encode(static_cast<char>(Serial1.read()));
  }

  if (gps_.location.isUpdated() && gps_.location.isValid()) {
    snapshot_.point.latitude = gps_.location.lat();
    snapshot_.point.longitude = gps_.location.lng();
    snapshot_.point.valid = gps_.location.age() <= 5000UL;
    if (gps_.altitude.isValid()) {
      const long altitude = std::lround(gps_.altitude.meters());
      snapshot_.point.altitude_m = static_cast<std::int16_t>(
          std::max<long>(-500L, std::min<long>(12000L, altitude)));
    }
    snapshot_.updated_at_ms = now_ms;
  } else if (snapshot_.point.valid &&
             now_ms - snapshot_.updated_at_ms > 10000UL) {
    snapshot_.point.valid = false;
  }

  if (gps_.satellites.isValid()) {
    snapshot_.satellites = static_cast<std::uint8_t>(
        std::min<unsigned long>(99UL, gps_.satellites.value()));
  }
  if (gps_.hdop.isValid()) {
    snapshot_.hdop = static_cast<float>(gps_.hdop.hdop());
  }
  snapshot_.unix_time = unixTimestamp(gps_.date, gps_.time);
  if (gps_.date.isValid()) {
    snapshot_.day_key = gps_.date.year() * 10000UL + gps_.date.month() * 100UL +
                        gps_.date.day();
  }
}

const GnssSnapshot& GnssService::snapshot() const { return snapshot_; }

bool GnssService::ready() const { return ready_; }

}  // namespace cmt

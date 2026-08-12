#include "cmt/platform/storage_service.h"

#include "cmt/core/geo.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include <cstdio>

namespace cmt {
namespace {

constexpr int kSdCsPin = 12;
constexpr int kSpiSckPin = 40;
constexpr int kSpiMisoPin = 39;
constexpr int kSpiMosiPin = 14;
constexpr std::size_t kMaxMenuItems = 9;
constexpr std::size_t kMaxMenuItemBytes = 80;
constexpr std::size_t kMaxTrackPoints = 128;

const char* kDefaultMessages[] = {
    "一切平安",          "我們在休息用餐", "已登頂",
    "返回中",            "處理狀況中",     "請求支援 / 原地等待",
};

}  // namespace

bool StorageService::begin() {
  SPI.begin(kSpiSckPin, kSpiMisoPin, kSpiMosiPin, kSdCsPin);
  ready_ = SD.begin(kSdCsPin, SPI, 10000000U);
  loadCannedMessages();
  if (ready_ && !SD.exists("/tracks")) {
    SD.mkdir("/tracks");
  }
  track_.reserve(kMaxTrackPoints);
  return ready_;
}

bool StorageService::ready() const { return ready_; }

void StorageService::loadDefaultMessages() {
  canned_messages_.clear();
  for (const char* message : kDefaultMessages) {
    canned_messages_.emplace_back(message);
  }
}

void StorageService::loadCannedMessages() {
  loadDefaultMessages();
  if (!ready_) {
    return;
  }
  File file = SD.open("/message.txt", FILE_READ);
  if (!file) {
    return;
  }

  std::vector<std::string> loaded;
  loaded.reserve(kMaxMenuItems);
  while (file.available() && loaded.size() < kMaxMenuItems) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0U && line.length() <= kMaxMenuItemBytes) {
      loaded.emplace_back(line.c_str(), line.length());
    }
  }
  file.close();
  if (!loaded.empty()) {
    canned_messages_ = std::move(loaded);
  }
}

const std::vector<std::string>& StorageService::cannedMessages() const {
  return canned_messages_;
}

const std::vector<GeoPoint>& StorageService::track() const { return track_; }

bool StorageService::maybeAppendTrack(const GeoPoint& point,
                                      const std::uint32_t day_key,
                                      const std::uint32_t unix_time,
                                      const std::uint32_t now_ms) {
  if (!point.valid || day_key == 0U) {
    return false;
  }
  if (!track_.empty() && now_ms - last_track_ms_ < 30000UL &&
      distanceMeters(track_.back(), point) < 20.0) {
    return false;
  }

  if (track_.size() >= kMaxTrackPoints) {
    track_.erase(track_.begin());
  }
  track_.push_back(point);
  last_track_ms_ = now_ms;
  if (!ready_) {
    return true;
  }

  char path[32]{};
  std::snprintf(path, sizeof(path), "/tracks/%08lu.csv",
                static_cast<unsigned long>(day_key));
  const bool new_file = !SD.exists(path);
  File file = SD.open(path, FILE_APPEND);
  if (!file) {
    return true;
  }
  if (new_file) {
    file.println("unix_time,latitude,longitude,altitude_m");
  }
  file.printf("%lu,%.7f,%.7f,%d\n", static_cast<unsigned long>(unix_time),
              point.latitude, point.longitude, point.altitude_m);
  file.close();
  return true;
}

}  // namespace cmt

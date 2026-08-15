#include "cmt/platform/storage_service.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

namespace cmt {
namespace {

constexpr int kSdCsPin = 12;
constexpr int kSpiSckPin = 40;
constexpr int kSpiMisoPin = 39;
constexpr int kSpiMosiPin = 14;
constexpr std::size_t kMaxMenuItems = 9;
constexpr std::size_t kMaxMenuItemBytes = 80;

const char* kDefaultMessages[] = {
    "一切平安",          "我們在休息用餐", "已登頂",
    "返回中",            "處理狀況中",     "請求支援 / 原地等待",
};

}  // namespace

bool StorageService::begin() {
  SPI.begin(kSpiSckPin, kSpiMisoPin, kSpiMosiPin, kSdCsPin);
  ready_ = SD.begin(kSdCsPin, SPI, 10000000U);
  loadCannedMessages();
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

}  // namespace cmt

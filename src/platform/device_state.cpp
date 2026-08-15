#include "cmt/platform/device_state.h"

#include "cmt/core/channel_plan.h"

#include <Preferences.h>
#include <esp_system.h>

#include <cstdio>

namespace cmt {
namespace {

void writeU32(std::array<std::uint8_t, 12>& output, const std::size_t offset,
              const std::uint32_t value) {
  output[offset] = static_cast<std::uint8_t>(value >> 24U);
  output[offset + 1U] = static_cast<std::uint8_t>(value >> 16U);
  output[offset + 2U] = static_cast<std::uint8_t>(value >> 8U);
  output[offset + 3U] = static_cast<std::uint8_t>(value);
}

}  // namespace

bool isValidUserId(const std::string& user_id) {
  if (user_id.empty() || user_id.size() > kMaxUserIdBytes) {
    return false;
  }
  for (const char value : user_id) {
    const bool allowed = (value >= 'A' && value <= 'Z') ||
                         (value >= '0' && value <= '9') || value == '-' ||
                         value == '_';
    if (!allowed) {
      return false;
    }
  }
  return true;
}

bool DeviceState::begin() {
  Preferences preferences;
  if (!preferences.begin("cmt", false)) {
    return false;
  }
  const String stored_user_id = preferences.getString("uid", "");
  user_id_.assign(stored_user_id.c_str());
  if (!isValidUserId(user_id_)) {
    user_id_.clear();
  }
  const String stored_pin = preferences.getString("pin", "");
  pin_.assign(stored_pin.c_str());
  if (!isValidPin(pin_)) {
    pin_.clear();
  }
  node_id_ = preferences.getUInt("node", 0);
  if (node_id_ == 0U || node_id_ == 0xFFFFFFFFUL) {
    do {
      node_id_ = esp_random();
    } while (node_id_ == 0U || node_id_ == 0xFFFFFFFFUL);
    preferences.putUInt("node", node_id_);
  }
  message_id_ = preferences.getUInt("msg", esp_random());
  sequence_ = preferences.getUInt("seq", 0);
  sequence_day_ = preferences.getUInt("seqday", 0);
  preferences.end();
  return true;
}

std::uint32_t DeviceState::nodeId() const { return node_id_; }

std::string DeviceState::userId() const { return user_id_; }

bool DeviceState::setUserId(const std::string& user_id) {
  if (!isValidUserId(user_id)) {
    return false;
  }
  user_id_ = user_id;
  Preferences preferences;
  if (preferences.begin("cmt", false)) {
    preferences.putString("uid", user_id_.c_str());
    preferences.end();
  }
  return true;
}

std::string DeviceState::pin() const { return pin_; }

bool DeviceState::setPin(const std::string& pin) {
  if (!isValidPin(pin)) {
    return false;
  }
  pin_ = pin;
  Preferences preferences;
  if (preferences.begin("cmt", false)) {
    preferences.putString("pin", pin_.c_str());
    preferences.end();
  }
  return true;
}

std::string DeviceState::callsign() const {
  if (!user_id_.empty()) {
    return user_id_;
  }
  char value[9]{};
  std::snprintf(value, sizeof(value), "N%04lX",
                static_cast<unsigned long>(node_id_ & 0xFFFFUL));
  return value;
}

std::uint32_t DeviceState::nextMessageId() {
  ++message_id_;
  if (message_id_ == 0U) {
    ++message_id_;
  }
  Preferences preferences;
  if (preferences.begin("cmt", false)) {
    preferences.putUInt("msg", message_id_);
    preferences.end();
  }
  return message_id_;
}

std::uint32_t DeviceState::nextSequence(const std::uint32_t day_key) {
  if (day_key != 0U && day_key != sequence_day_) {
    sequence_day_ = day_key;
    sequence_ = 0;
  }
  ++sequence_;
  Preferences preferences;
  if (preferences.begin("cmt", false)) {
    preferences.putUInt("seq", sequence_);
    preferences.putUInt("seqday", sequence_day_);
    preferences.end();
  }
  return sequence_;
}

void NonceGenerator::begin(const std::uint32_t source_id) {
  source_id_ = source_id;
  boot_random_ = esp_random();
  counter_ = esp_random();
}

std::array<std::uint8_t, 12> NonceGenerator::next() {
  std::array<std::uint8_t, 12> nonce{};
  writeU32(nonce, 0, source_id_);
  writeU32(nonce, 4, boot_random_);
  writeU32(nonce, 8, ++counter_);
  return nonce;
}

}  // namespace cmt

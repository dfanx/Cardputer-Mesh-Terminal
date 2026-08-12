#include "cmt/platform/device_state.h"

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

bool DeviceState::begin() {
  Preferences preferences;
  if (!preferences.begin("cmt", false)) {
    return false;
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

std::string DeviceState::callsign() const {
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

#include "cmt/core/channel_plan.h"

namespace cmt {
namespace {

constexpr std::uint8_t kChannelCount = 14;
constexpr float kBaseFrequencyMhz = 920.125F;
constexpr float kChannelSpacingMhz = 0.200F;

std::uint32_t fnv1a(const char* data, const std::size_t length) {
  std::uint32_t hash = 2166136261UL;
  for (std::size_t index = 0; index < length; ++index) {
    hash ^= static_cast<std::uint8_t>(data[index]);
    hash *= 16777619UL;
  }
  return hash;
}

}  // namespace

bool isValidPin(const std::string& pin) {
  if (pin.size() != 4U) {
    return false;
  }
  for (const char value : pin) {
    if (value < '0' || value > '9') {
      return false;
    }
  }
  return true;
}

bool deriveGroupProfile(const std::string& pin, GroupProfile& profile) {
  if (!isValidPin(pin)) {
    return false;
  }

  const std::string material = std::string("CMT-v1:") + pin;
  const std::uint32_t hash = fnv1a(material.data(), material.size());
  profile.group_id = hash;
  profile.channel_index = static_cast<std::uint8_t>(hash % kChannelCount);
  profile.frequency_mhz =
      kBaseFrequencyMhz + kChannelSpacingMhz * profile.channel_index;

  std::uint8_t sync = static_cast<std::uint8_t>((hash >> 16U) ^ hash);
  if (sync == 0x00U || sync == 0xFFU || sync == 0x34U) {
    sync ^= 0x5AU;
  }
  profile.sync_word = sync;
  return true;
}

}  // namespace cmt

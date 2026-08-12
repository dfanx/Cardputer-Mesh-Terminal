#pragma once

#include <cstdint>
#include <string>

namespace cmt {

struct GroupProfile {
  std::uint32_t group_id = 0;
  std::uint8_t channel_index = 0;
  float frequency_mhz = 0.0F;
  std::uint8_t sync_word = 0;
};

bool isValidPin(const std::string& pin);
bool deriveGroupProfile(const std::string& pin, GroupProfile& profile);

}  // namespace cmt

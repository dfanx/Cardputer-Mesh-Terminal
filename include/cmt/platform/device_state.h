#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace cmt {

class DeviceState {
 public:
  bool begin();
  std::uint32_t nodeId() const;
  std::string callsign() const;
  std::uint32_t nextMessageId();
  std::uint32_t nextSequence(std::uint32_t day_key);

 private:
  std::uint32_t node_id_ = 0;
  std::uint32_t message_id_ = 0;
  std::uint32_t sequence_ = 0;
  std::uint32_t sequence_day_ = 0;
};

class NonceGenerator {
 public:
  void begin(std::uint32_t source_id);
  std::array<std::uint8_t, 12> next();

 private:
  std::uint32_t source_id_ = 0;
  std::uint32_t boot_random_ = 0;
  std::uint32_t counter_ = 0;
};

}  // namespace cmt

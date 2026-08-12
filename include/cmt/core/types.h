#pragma once

#include <cstddef>
#include <cstdint>

namespace cmt {

constexpr std::size_t kMaxRadioPacketBytes = 255;
constexpr std::size_t kWireHeaderBytes = 42;
constexpr std::size_t kAuthTagBytes = 16;
constexpr std::size_t kMaxFragmentPayloadBytes =
    kMaxRadioPacketBytes - kWireHeaderBytes - kAuthTagBytes;
constexpr std::uint8_t kMaxFragmentCount = 32;
constexpr std::size_t kMaxMessageBytes =
    kMaxFragmentPayloadBytes * kMaxFragmentCount;

enum class MessageType : std::uint8_t {
  Voice = 0x01,
  Text = 0x02,
  Beacon = 0x03,
};

struct GeoPoint {
  double latitude = 0.0;
  double longitude = 0.0;
  std::int16_t altitude_m = 0;
  bool valid = false;
};

bool isKnownMessageType(MessageType type);
std::uint8_t initialTtl(MessageType type);

}  // namespace cmt

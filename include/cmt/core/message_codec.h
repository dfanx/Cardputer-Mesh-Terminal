#pragma once

#include "cmt/core/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cmt {

constexpr std::size_t kMaxTextBytes = 160;
constexpr std::size_t kMaxCallsignBytes = 12;
constexpr std::uint16_t kVoiceSampleRateHz = 8000;
constexpr std::uint16_t kVoiceSamplesPerFrame = 320;
constexpr std::uint8_t kVoiceBytesPerFrame = 7;
constexpr std::uint8_t kMaxVoiceFrames = 75;

enum class VoiceCodec : std::uint8_t {
  Codec2_1300 = 1,
};

struct VoiceMessage {
  VoiceCodec codec = VoiceCodec::Codec2_1300;
  std::uint16_t sample_rate_hz = kVoiceSampleRateHz;
  std::uint16_t samples_per_frame = kVoiceSamplesPerFrame;
  std::uint8_t bytes_per_frame = kVoiceBytesPerFrame;
  std::vector<std::uint8_t> frames;
};

struct BeaconMessage {
  GeoPoint point{};
  std::uint8_t battery_percent = 0;
  std::string callsign;
};

bool encodeTextMessage(const std::string& text,
                       std::vector<std::uint8_t>& output);
bool decodeTextMessage(const std::vector<std::uint8_t>& input,
                       std::string& text);
bool encodeBeaconMessage(const BeaconMessage& beacon,
                         std::vector<std::uint8_t>& output);
bool decodeBeaconMessage(const std::vector<std::uint8_t>& input,
                         BeaconMessage& beacon);
bool encodeVoiceMessage(const VoiceMessage& voice,
                        std::vector<std::uint8_t>& output);
bool decodeVoiceMessage(const std::vector<std::uint8_t>& input,
                        VoiceMessage& voice);

}  // namespace cmt

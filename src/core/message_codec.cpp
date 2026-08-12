#include "cmt/core/message_codec.h"

#include "cmt/core/geo.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace cmt {
namespace {

constexpr std::uint8_t kBeaconVersion = 1;
constexpr std::size_t kBeaconFixedBytes = 13;
constexpr std::uint8_t kVoiceVersion = 1;
constexpr std::size_t kVoiceFixedBytes = 8;

void writeU32(std::vector<std::uint8_t>& output, const std::uint32_t value) {
  output.push_back(static_cast<std::uint8_t>(value >> 24U));
  output.push_back(static_cast<std::uint8_t>(value >> 16U));
  output.push_back(static_cast<std::uint8_t>(value >> 8U));
  output.push_back(static_cast<std::uint8_t>(value));
}

std::uint32_t readU32(const std::uint8_t* input) {
  return (static_cast<std::uint32_t>(input[0]) << 24U) |
         (static_cast<std::uint32_t>(input[1]) << 16U) |
         (static_cast<std::uint32_t>(input[2]) << 8U) |
         static_cast<std::uint32_t>(input[3]);
}

void writeU16(std::vector<std::uint8_t>& output, const std::uint16_t value) {
  output.push_back(static_cast<std::uint8_t>(value >> 8U));
  output.push_back(static_cast<std::uint8_t>(value));
}

std::uint16_t readU16(const std::uint8_t* input) {
  return static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(input[0]) << 8U) |
      static_cast<std::uint16_t>(input[1]));
}

bool isValidVoiceMetadata(const VoiceMessage& voice) {
  if (voice.codec != VoiceCodec::Codec2_1300 ||
      voice.sample_rate_hz != kVoiceSampleRateHz ||
      voice.samples_per_frame != kVoiceSamplesPerFrame ||
      voice.bytes_per_frame != kVoiceBytesPerFrame || voice.frames.empty() ||
      voice.frames.size() % voice.bytes_per_frame != 0U) {
    return false;
  }
  const std::size_t frame_count =
      voice.frames.size() / voice.bytes_per_frame;
  return frame_count <= kMaxVoiceFrames;
}

}  // namespace

bool encodeTextMessage(const std::string& text,
                       std::vector<std::uint8_t>& output) {
  if (text.empty() || text.size() > kMaxTextBytes ||
      text.find('\0') != std::string::npos) {
    return false;
  }
  output.assign(text.begin(), text.end());
  return true;
}

bool decodeTextMessage(const std::vector<std::uint8_t>& input,
                       std::string& text) {
  if (input.empty() || input.size() > kMaxTextBytes ||
      std::find(input.begin(), input.end(), 0U) != input.end()) {
    return false;
  }
  text.assign(input.begin(), input.end());
  return true;
}

bool encodeBeaconMessage(const BeaconMessage& beacon,
                         std::vector<std::uint8_t>& output) {
  if (!beacon.point.valid ||
      !isValidCoordinate(beacon.point.latitude, beacon.point.longitude) ||
      beacon.battery_percent > 100U || beacon.callsign.empty() ||
      beacon.callsign.size() > kMaxCallsignBytes) {
    return false;
  }
  const auto latitude_e7 = static_cast<std::int32_t>(
      std::lround(beacon.point.latitude * 10000000.0));
  const auto longitude_e7 = static_cast<std::int32_t>(
      std::lround(beacon.point.longitude * 10000000.0));

  output.clear();
  output.reserve(kBeaconFixedBytes + beacon.callsign.size());
  output.push_back(kBeaconVersion);
  writeU32(output, static_cast<std::uint32_t>(latitude_e7));
  writeU32(output, static_cast<std::uint32_t>(longitude_e7));
  output.push_back(static_cast<std::uint8_t>(
      static_cast<std::uint16_t>(beacon.point.altitude_m) >> 8U));
  output.push_back(static_cast<std::uint8_t>(beacon.point.altitude_m));
  output.push_back(beacon.battery_percent);
  output.push_back(static_cast<std::uint8_t>(beacon.callsign.size()));
  output.insert(output.end(), beacon.callsign.begin(), beacon.callsign.end());
  return true;
}

bool decodeBeaconMessage(const std::vector<std::uint8_t>& input,
                         BeaconMessage& beacon) {
  if (input.size() < kBeaconFixedBytes || input[0] != kBeaconVersion) {
    return false;
  }
  const std::size_t callsign_size = input[12];
  if (callsign_size == 0U || callsign_size > kMaxCallsignBytes ||
      input.size() != kBeaconFixedBytes + callsign_size) {
    return false;
  }

  const auto latitude_e7 = static_cast<std::int32_t>(readU32(input.data() + 1));
  const auto longitude_e7 =
      static_cast<std::int32_t>(readU32(input.data() + 5));
  const auto altitude = static_cast<std::int16_t>(
      (static_cast<std::uint16_t>(input[9]) << 8U) |
      static_cast<std::uint16_t>(input[10]));
  if (input[11] > 100U) {
    return false;
  }

  beacon.point.latitude = static_cast<double>(latitude_e7) / 10000000.0;
  beacon.point.longitude = static_cast<double>(longitude_e7) / 10000000.0;
  beacon.point.altitude_m = altitude;
  beacon.point.valid =
      isValidCoordinate(beacon.point.latitude, beacon.point.longitude);
  beacon.battery_percent = input[11];
  beacon.callsign.assign(input.begin() + kBeaconFixedBytes, input.end());
  return beacon.point.valid;
}

bool encodeVoiceMessage(const VoiceMessage& voice,
                        std::vector<std::uint8_t>& output) {
  if (!isValidVoiceMetadata(voice)) {
    return false;
  }
  const auto frame_count = static_cast<std::uint8_t>(
      voice.frames.size() / voice.bytes_per_frame);
  output.clear();
  output.reserve(kVoiceFixedBytes + voice.frames.size());
  output.push_back(kVoiceVersion);
  output.push_back(static_cast<std::uint8_t>(voice.codec));
  writeU16(output, voice.sample_rate_hz);
  writeU16(output, voice.samples_per_frame);
  output.push_back(voice.bytes_per_frame);
  output.push_back(frame_count);
  output.insert(output.end(), voice.frames.begin(), voice.frames.end());
  return true;
}

bool decodeVoiceMessage(const std::vector<std::uint8_t>& input,
                        VoiceMessage& voice) {
  if (input.size() <= kVoiceFixedBytes || input[0] != kVoiceVersion) {
    return false;
  }
  VoiceMessage decoded{};
  decoded.codec = static_cast<VoiceCodec>(input[1]);
  decoded.sample_rate_hz = readU16(input.data() + 2U);
  decoded.samples_per_frame = readU16(input.data() + 4U);
  decoded.bytes_per_frame = input[6];
  const std::size_t frame_count = input[7];
  const std::size_t expected_size =
      kVoiceFixedBytes + frame_count * decoded.bytes_per_frame;
  if (frame_count == 0U || input.size() != expected_size) {
    return false;
  }
  decoded.frames.assign(input.begin() + kVoiceFixedBytes, input.end());
  if (!isValidVoiceMetadata(decoded)) {
    return false;
  }
  voice = std::move(decoded);
  return true;
}

}  // namespace cmt

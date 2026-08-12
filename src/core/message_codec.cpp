#include "cmt/core/message_codec.h"

#include "cmt/core/geo.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace cmt {
namespace {

constexpr std::uint8_t kBeaconVersion = 1;
constexpr std::size_t kBeaconFixedBytes = 13;
// v2 起改用 Codec2 700C 加跨幀位元打包，並移除可由 codec id 推得的欄位。
constexpr std::uint8_t kVoiceVersion = 2;

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

bool isValidVoiceMetadata(const VoiceMessage& voice) {
  if (voice.codec != VoiceCodec::Codec2_700C || voice.frames.empty() ||
      voice.frames.size() % kVoiceCodedBytesPerFrame != 0U) {
    return false;
  }
  const std::size_t frame_count =
      voice.frames.size() / kVoiceCodedBytesPerFrame;
  return frame_count <= kMaxVoiceFrames;
}

bool readBit(const std::uint8_t* buffer, const std::size_t bit_index) {
  return ((buffer[bit_index / 8U] >> (7U - bit_index % 8U)) & 1U) != 0U;
}

void writeBit(std::uint8_t* buffer, const std::size_t bit_index) {
  buffer[bit_index / 8U] |=
      static_cast<std::uint8_t>(1U << (7U - bit_index % 8U));
}

// codec2 以 MSB-first 由 byte 0 起填入 28 個有效位元，其餘為 padding。這裡把
// 每幀的有效位元接續成連續位元流。
void packVoiceFrames(const std::vector<std::uint8_t>& coded,
                     const std::size_t frame_count,
                     std::vector<std::uint8_t>& packed) {
  packed.assign(packedVoiceBytes(frame_count), 0U);
  std::size_t out_bit = 0;
  for (std::size_t frame = 0; frame < frame_count; ++frame) {
    const std::uint8_t* source =
        coded.data() + frame * kVoiceCodedBytesPerFrame;
    for (std::size_t bit = 0; bit < kVoiceBitsPerFrame; ++bit, ++out_bit) {
      if (readBit(source, bit)) {
        writeBit(packed.data(), out_bit);
      }
    }
  }
}

void unpackVoiceFrames(const std::uint8_t* packed,
                       const std::size_t frame_count,
                       std::vector<std::uint8_t>& coded) {
  coded.assign(frame_count * kVoiceCodedBytesPerFrame, 0U);
  std::size_t in_bit = 0;
  for (std::size_t frame = 0; frame < frame_count; ++frame) {
    std::uint8_t* target = coded.data() + frame * kVoiceCodedBytesPerFrame;
    for (std::size_t bit = 0; bit < kVoiceBitsPerFrame; ++bit, ++in_bit) {
      if (readBit(packed, in_bit)) {
        writeBit(target, bit);
      }
    }
  }
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
  const std::size_t frame_count =
      voice.frames.size() / kVoiceCodedBytesPerFrame;
  std::vector<std::uint8_t> packed;
  packVoiceFrames(voice.frames, frame_count, packed);

  output.clear();
  output.reserve(kVoiceHeaderBytes + packed.size());
  output.push_back(kVoiceVersion);
  output.push_back(static_cast<std::uint8_t>(voice.codec));
  output.push_back(static_cast<std::uint8_t>(frame_count));
  output.insert(output.end(), packed.begin(), packed.end());
  return true;
}

bool decodeVoiceMessage(const std::vector<std::uint8_t>& input,
                        VoiceMessage& voice) {
  if (input.size() <= kVoiceHeaderBytes || input[0] != kVoiceVersion) {
    return false;
  }
  const std::size_t frame_count = input[2];
  if (frame_count == 0U || frame_count > kMaxVoiceFrames ||
      input.size() != kVoiceHeaderBytes + packedVoiceBytes(frame_count)) {
    return false;
  }
  // 末位元組的 padding 位元必須為零，避免同一段語音有多種合法編碼。
  const std::size_t padding_bits =
      packedVoiceBytes(frame_count) * 8U - frame_count * kVoiceBitsPerFrame;
  if (padding_bits > 0U &&
      (input.back() & ((1U << padding_bits) - 1U)) != 0U) {
    return false;
  }

  VoiceMessage decoded{};
  decoded.codec = static_cast<VoiceCodec>(input[1]);
  unpackVoiceFrames(input.data() + kVoiceHeaderBytes, frame_count,
                    decoded.frames);
  if (!isValidVoiceMetadata(decoded)) {
    return false;
  }
  voice = std::move(decoded);
  return true;
}

}  // namespace cmt

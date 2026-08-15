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
constexpr std::uint16_t kVoiceFrameDurationMs = 40;

// Codec2 700C：每幀 28 bits。`codec2_encode()` 一律以位元組對齊方式輸出，所以
// 記憶體中每幀佔 4 bytes、末 4 bits 是 padding；上線前必須跨幀位元打包，否則
// 14% 的空中時間會浪費在 padding 上。
constexpr std::uint8_t kVoiceBitsPerFrame = 28;
constexpr std::uint8_t kVoiceCodedBytesPerFrame = 4;

// version + codec + frame_count。取樣率、幀長與每幀位元數都由 codec id 決定，
// 不重複放進 wire format。
constexpr std::size_t kVoiceHeaderBytes = 3;

// 語音刻意壓在單一 fragment 內：語音 TTL=1 且沒有重傳，多一片就是把整段訊息
// 的到達率再乘一次單包到達率。
constexpr std::uint8_t kMaxVoiceFrames = static_cast<std::uint8_t>(
    (kMaxFragmentPayloadBytes - kVoiceHeaderBytes) * 8U / kVoiceBitsPerFrame);
constexpr std::uint32_t kMaxVoiceDurationMs =
    static_cast<std::uint32_t>(kMaxVoiceFrames) * kVoiceFrameDurationMs;

enum class VoiceCodec : std::uint8_t {
  Codec2_700C = 2,
};

// `frames` 存的是 codec2 原生的位元組對齊輸出（kVoiceCodedBytesPerFrame/幀），
// 位元打包只發生在 wire format 上。
struct VoiceMessage {
  VoiceCodec codec = VoiceCodec::Codec2_700C;
  std::vector<std::uint8_t> frames;
};

// 打包後的位元組數，不含 header。
constexpr std::size_t packedVoiceBytes(const std::size_t frame_count) {
  return (frame_count * kVoiceBitsPerFrame + 7U) / 8U;
}

static_assert(kMaxVoiceFrames > 0U, "voice frame budget underflowed");
static_assert(kVoiceHeaderBytes + packedVoiceBytes(kMaxVoiceFrames) <=
                  kMaxFragmentPayloadBytes,
              "max voice message must fit in one fragment");

// `point.valid` 同時是「有沒有定位」的旗標：v2 起 Beacon 在沒有 fix 時照樣發送，
// 只是不帶座標。隊友名單因此不再依賴 GNSS，室內或收不到訊號也看得到彼此。
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

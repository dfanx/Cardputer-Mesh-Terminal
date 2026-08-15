#pragma once

#include "cmt/core/types.h"

// TinyGPSPlus 只出現在 .cpp 的 Impl 裡：這個標頭會被 UI 層引用，不該把驅動函式庫
// 拖進去。
#include <cstdint>
#include <memory>

namespace cmt {

// GPS 顯示 0 有三種完全不同的原因，畫面上必須分得出來，否則使用者無從判斷是要
// 走到空曠處、還是模組根本沒接上。
enum class GnssLink : std::uint8_t {
  NoData,   // UART 上沒有任何 NMEA：接線、供電或模組問題
  Garbled,  // 有資料但 checksum 全錯：鮑率或干擾
  NoFix,    // 正常收到 NMEA，只是還沒定位（室內的正常狀態）
  Fixed,
};

struct GnssSnapshot {
  GeoPoint point{};
  // 定位使用的衛星數（GGA）。
  std::uint8_t satellites = 0;
  // 視野內的衛星數（GSV）。還沒定位時這個值會先動起來，是「天線有在收」的證據。
  std::uint8_t satellites_in_view = 0;
  float hdop = 0.0F;
  std::uint32_t unix_time = 0;
  std::uint32_t day_key = 0;
  std::uint32_t updated_at_ms = 0;
  std::uint32_t sentences_ok = 0;
  std::uint32_t checksum_errors = 0;
  std::uint32_t last_sentence_ms = 0;
  GnssLink link = GnssLink::NoData;
};

class GnssService {
 public:
  GnssService();
  ~GnssService();

  GnssService(const GnssService&) = delete;
  GnssService& operator=(const GnssService&) = delete;

  bool begin();
  void update(std::uint32_t now_ms);
  const GnssSnapshot& snapshot() const;
  bool ready() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

const char* gnssLinkLabel(GnssLink link);

}  // namespace cmt

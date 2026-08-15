#include "cmt/platform/gnss_service.h"

#include <Arduino.h>
#include <TinyGPSPlus.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace cmt {
namespace {

constexpr int kGnssRxPin = 15;
constexpr int kGnssTxPin = 13;
constexpr std::uint32_t kGnssBaud = 115200;
// 超過這段時間沒有任何合法 NMEA 就視為鏈路斷了。模組正常時每秒都有句子。
constexpr std::uint32_t kLinkTimeoutMs = 5000;
// 定位比句子容易掉，放寬到 10 秒才把 fix 判定為過期。
constexpr std::uint32_t kFixTimeoutMs = 10000;
constexpr std::uint32_t kDiagnosticIntervalMs = 10000;

std::int64_t daysFromCivil(int year, const unsigned month,
                          const unsigned day) {
  year -= month <= 2U;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
  const unsigned adjusted_month = month > 2U ? month - 3U : month + 9U;
  const unsigned day_of_year =
      (153U * adjusted_month + 2U) / 5U + day - 1U;
  const unsigned day_of_era =
      year_of_era * 365U + year_of_era / 4U - year_of_era / 100U + day_of_year;
  return era * 146097LL + static_cast<std::int64_t>(day_of_era) - 719468LL;
}

std::uint32_t unixTimestamp(TinyGPSDate& date, TinyGPSTime& time) {
  if (!date.isValid() || !time.isValid() || date.year() < 2020U) {
    return 0;
  }
  const std::int64_t days =
      daysFromCivil(date.year(), date.month(), date.day());
  const std::int64_t seconds =
      days * 86400LL + static_cast<std::int64_t>(time.hour()) * 3600LL +
      static_cast<std::int64_t>(time.minute()) * 60LL + time.second();
  if (seconds < 0 || seconds > 0xFFFFFFFFLL) {
    return 0;
  }
  return static_cast<std::uint32_t>(seconds);
}

}  // namespace

// GSV 的第 3 個欄位是「視野內衛星總數」，TinyGPSPlus 本身不解析，用 custom 欄位
// 取出來。定位之前這個數字就會動，是判斷天線有沒有在收訊的第一手證據。
class GnssService::Impl {
 public:
  Impl() : satellites_in_view_(gps_, "GPGSV", 3),
           satellites_in_view_gn_(gps_, "GNGSV", 3) {}

  bool begin() {
    Serial1.begin(kGnssBaud, SERIAL_8N1, kGnssRxPin, kGnssTxPin);
    ready_ = true;
    return true;
  }

  void update(const std::uint32_t now_ms) {
    if (!ready_) {
      return;
    }
    while (Serial1.available() > 0) {
      gps_.encode(static_cast<char>(Serial1.read()));
    }

    snapshot_.sentences_ok =
        static_cast<std::uint32_t>(gps_.passedChecksum());
    snapshot_.checksum_errors =
        static_cast<std::uint32_t>(gps_.failedChecksum());
    if (snapshot_.sentences_ok != last_sentences_ok_) {
      last_sentences_ok_ = snapshot_.sentences_ok;
      snapshot_.last_sentence_ms = now_ms;
      have_sentence_ = true;
    }

    if (gps_.location.isUpdated() && gps_.location.isValid()) {
      snapshot_.point.latitude = gps_.location.lat();
      snapshot_.point.longitude = gps_.location.lng();
      snapshot_.point.valid = gps_.location.age() <= 5000UL;
      if (gps_.altitude.isValid()) {
        const long altitude = std::lround(gps_.altitude.meters());
        snapshot_.point.altitude_m = static_cast<std::int16_t>(
            std::max<long>(-500L, std::min<long>(12000L, altitude)));
      }
      snapshot_.updated_at_ms = now_ms;
    } else if (snapshot_.point.valid &&
               now_ms - snapshot_.updated_at_ms > kFixTimeoutMs) {
      snapshot_.point.valid = false;
    }

    if (gps_.satellites.isValid()) {
      snapshot_.satellites = static_cast<std::uint8_t>(
          std::min<unsigned long>(99UL, gps_.satellites.value()));
    }
    updateSatellitesInView();
    if (gps_.hdop.isValid()) {
      snapshot_.hdop = static_cast<float>(gps_.hdop.hdop());
    }
    snapshot_.unix_time = unixTimestamp(gps_.date, gps_.time);
    if (gps_.date.isValid()) {
      snapshot_.day_key = gps_.date.year() * 10000UL +
                          gps_.date.month() * 100UL + gps_.date.day();
    }
    snapshot_.link = classifyLink(now_ms);
    logDiagnostics(now_ms);
  }

  const GnssSnapshot& snapshot() const { return snapshot_; }
  bool ready() const { return ready_; }

 private:
  // 只輸出計數與鏈路狀態，不輸出座標——精確位置不得進 Serial 日誌。這是判斷
  // 「室內收不到」與「模組沒接上」的第一手資料。
  void logDiagnostics(const std::uint32_t now_ms) {
    if (now_ms - last_log_ms_ < kDiagnosticIntervalMs && last_log_ms_ != 0U) {
      return;
    }
    last_log_ms_ = now_ms == 0U ? 1U : now_ms;
    // 序列埠用 ASCII 標籤，不受終端機編碼影響。
    static const char* kTags[] = {"no-data", "garbled", "no-fix", "fixed"};
    Serial.printf("[gnss] %s ok=%lu bad=%lu used=%u view=%u\n",
                  kTags[static_cast<std::uint8_t>(snapshot_.link)],
                  static_cast<unsigned long>(snapshot_.sentences_ok),
                  static_cast<unsigned long>(snapshot_.checksum_errors),
                  snapshot_.satellites, snapshot_.satellites_in_view);
  }

  void updateSatellitesInView() {
    const char* value = satellites_in_view_.isValid()
                            ? satellites_in_view_.value()
                            : (satellites_in_view_gn_.isValid()
                                   ? satellites_in_view_gn_.value()
                                   : nullptr);
    if (value == nullptr || value[0] == '\0') {
      return;
    }
    const unsigned long parsed = std::strtoul(value, nullptr, 10);
    snapshot_.satellites_in_view =
        static_cast<std::uint8_t>(std::min<unsigned long>(99UL, parsed));
  }

  GnssLink classifyLink(const std::uint32_t now_ms) const {
    const bool link_alive =
        have_sentence_ && now_ms - snapshot_.last_sentence_ms <= kLinkTimeoutMs;
    if (!link_alive) {
      // 完全沒有合法句子，但 checksum 錯誤一直在累加 = 收得到位元、解不出句子。
      return snapshot_.checksum_errors > 0U ? GnssLink::Garbled
                                            : GnssLink::NoData;
    }
    return snapshot_.point.valid ? GnssLink::Fixed : GnssLink::NoFix;
  }

  TinyGPSPlus gps_;
  TinyGPSCustom satellites_in_view_;
  TinyGPSCustom satellites_in_view_gn_;
  GnssSnapshot snapshot_{};
  std::uint32_t last_sentences_ok_ = 0;
  std::uint32_t last_log_ms_ = 0;
  bool have_sentence_ = false;
  bool ready_ = false;
};

GnssService::GnssService() : impl_(new Impl()) {}
GnssService::~GnssService() = default;

bool GnssService::begin() { return impl_->begin(); }

void GnssService::update(const std::uint32_t now_ms) { impl_->update(now_ms); }

const GnssSnapshot& GnssService::snapshot() const { return impl_->snapshot(); }

bool GnssService::ready() const { return impl_->ready(); }

const char* gnssLinkLabel(const GnssLink link) {
  switch (link) {
    case GnssLink::NoData:
      return "無訊號源";
    case GnssLink::Garbled:
      return "資料異常";
    case GnssLink::NoFix:
      return "搜尋中";
    case GnssLink::Fixed:
      return "已定位";
  }
  return "--";
}

}  // namespace cmt

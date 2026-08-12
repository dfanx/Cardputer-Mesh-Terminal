#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace cmt {

// 使用者代號會塞進 Beacon 的 callsign 欄位（上限 kMaxCallsignBytes = 12），
// 這裡再收緊到 8 是為了讓主畫面標題列與隊友欄位放得下。
constexpr std::size_t kMaxUserIdBytes = 8;

// 只允許大寫英數與 - _：ASCII、長度固定可預期，也不會在標題列產生半形/全形
// 混排。空字串代表使用者還沒設定。
bool isValidUserId(const std::string& user_id);

class DeviceState {
 public:
  bool begin();
  std::uint32_t nodeId() const;
  // 使用者代號；未設定時回傳空字串。
  std::string userId() const;
  // 驗證通過才寫入 NVS 並回傳 true；失敗時保留原值。
  bool setUserId(const std::string& user_id);
  // 有使用者代號時就用它，否則退回 node id 衍生的 N#### 名稱。
  std::string callsign() const;
  std::uint32_t nextMessageId();
  std::uint32_t nextSequence(std::uint32_t day_key);

 private:
  std::string user_id_;
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

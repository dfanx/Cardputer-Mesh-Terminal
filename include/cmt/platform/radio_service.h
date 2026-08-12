#pragma once

#include "cmt/core/channel_plan.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace cmt {

struct RadioRxPacket {
  std::vector<std::uint8_t> bytes;
  float rssi_dbm = 0.0F;
  float snr_db = 0.0F;
};

class RadioService {
 public:
  RadioService();
  ~RadioService();

  RadioService(const RadioService&) = delete;
  RadioService& operator=(const RadioService&) = delete;

  bool begin(const GroupProfile& profile);
  void update(std::uint32_t now_ms);
  bool queueTransmit(const std::vector<std::uint8_t>& bytes);
  bool queueTransmitBatch(
      const std::vector<std::vector<std::uint8_t>>& packets);
  bool pollReceived(RadioRxPacket& packet);
  bool ready() const;
  int lastError() const;
  std::size_t queuedCount() const;

  // LoRa cap 模組是否有回應（I2C 擴充器與 SX1262 都認得）。這只證明模組存在，
  // 不能證明天線已接上——SX1262 沒有 VSWR 或反射功率量測，天線是否安裝在硬體
  // 上無法偵測，必須由使用者確認，見 setTransmitInhibited()。
  bool moduleDetected() const;

  // 發射禁止閘。無天線發射會把功率反射回 PA 而燒毀模組，所以這道閘設在真正
  // 驅動 PA 的這一層：禁止時佇列會拒收，且 update() 不會啟動任何發射。
  // 接收不受影響，無天線接收不會損壞硬體。
  void setTransmitInhibited(bool inhibited);
  bool transmitInhibited() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace cmt

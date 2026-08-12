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

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace cmt

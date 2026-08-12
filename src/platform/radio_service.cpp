#include "cmt/platform/radio_service.h"

#include "cmt/core/types.h"

#include <M5Unified.h>
#include <RadioLib.h>
#include <esp_system.h>
#include <utility/PI4IOE5V6408_Class.hpp>

#include <algorithm>
#include <deque>

namespace cmt {
namespace {

constexpr int kLoRaCsPin = 5;
constexpr int kLoRaIrqPin = 4;
constexpr int kLoRaResetPin = 3;
constexpr int kLoRaBusyPin = 6;
constexpr float kBandwidthKhz = 125.0F;
constexpr std::uint8_t kSpreadingFactor = 9;
constexpr std::uint8_t kCodingRate = 7;
constexpr std::int8_t kTxPowerDbm = 14;
constexpr std::uint16_t kPreambleSymbols = 12;
constexpr std::size_t kMaxTxQueue = 24;
constexpr std::size_t kMaxRxQueue = 12;

}  // namespace

class RadioService::Impl {
 public:
  Impl()
      : module_(kLoRaCsPin, kLoRaIrqPin, kLoRaResetPin, kLoRaBusyPin),
        radio_(&module_),
        ioe_(0x43, 400000, &m5::In_I2C) {}

  ~Impl() {
    if (active_ == this) {
      active_ = nullptr;
    }
  }

  bool begin(const GroupProfile& profile) {
    ready_ = false;
    module_detected_ = false;
    last_error_ = RADIOLIB_ERR_NONE;

    if (!m5::In_I2C.begin()) {
      last_error_ = RADIOLIB_ERR_CHIP_NOT_FOUND;
      return false;
    }
    if (!ioe_.begin()) {
      last_error_ = RADIOLIB_ERR_CHIP_NOT_FOUND;
      return false;
    }
    ioe_.setDirection(0, true);
    ioe_.setHighImpedance(0, false);
    ioe_.digitalWrite(0, true);

    last_error_ = radio_.begin(profile.frequency_mhz, kBandwidthKhz,
                               kSpreadingFactor, kCodingRate,
                               profile.sync_word, kTxPowerDbm,
                               kPreambleSymbols, 3.0F, true);
    if (last_error_ != RADIOLIB_ERR_NONE) {
      return false;
    }
    module_detected_ = true;
    last_error_ = radio_.setCurrentLimit(140.0F);
    if (last_error_ != RADIOLIB_ERR_NONE) {
      return false;
    }
    active_ = this;
    ready_ = startReceive();
    return ready_;
  }

  void update(const std::uint32_t now_ms) {
    if (!ready_) {
      return;
    }
    if (irq_) {
      irq_ = false;
      if (mode_ == Mode::Transmit) {
        last_error_ = radio_.finishTransmit();
        active_tx_.clear();
        startReceive();
      } else {
        readReceived();
        startReceive();
      }
    }

    if (mode_ == Mode::Receive && !tx_inhibited_ && !tx_queue_.empty() &&
        static_cast<std::int32_t>(now_ms - next_tx_at_ms_) >= 0) {
      tryStartTransmit(now_ms);
    }
  }

  bool queueTransmit(const std::vector<std::uint8_t>& bytes) {
    if (!ready_ || tx_inhibited_ || bytes.empty() ||
        bytes.size() > kMaxRadioPacketBytes ||
        tx_queue_.size() >= kMaxTxQueue) {
      return false;
    }
    tx_queue_.push_back(bytes);
    return true;
  }

  bool queueTransmitBatch(
      const std::vector<std::vector<std::uint8_t>>& packets) {
    if (!ready_ || tx_inhibited_ || packets.empty() ||
        packets.size() > kMaxTxQueue - tx_queue_.size()) {
      return false;
    }
    for (const auto& packet : packets) {
      if (packet.empty() || packet.size() > kMaxRadioPacketBytes) {
        return false;
      }
    }
    for (const auto& packet : packets) {
      tx_queue_.push_back(packet);
    }
    return true;
  }

  bool pollReceived(RadioRxPacket& packet) {
    if (rx_queue_.empty()) {
      return false;
    }
    packet = std::move(rx_queue_.front());
    rx_queue_.pop_front();
    return true;
  }

  bool ready() const { return ready_; }
  int lastError() const { return last_error_; }
  std::size_t queuedCount() const {
    return tx_queue_.size() + (active_tx_.empty() ? 0U : 1U);
  }
  bool moduleDetected() const { return module_detected_; }

  void setTransmitInhibited(const bool inhibited) {
    if (inhibited == tx_inhibited_) {
      return;
    }
    tx_inhibited_ = inhibited;
    if (inhibited) {
      // 已排隊但尚未上空的封包直接丟棄，避免天線狀態改變後才突然補發。
      // 進行中的那一包不中止：它已經在輻射，中途拉掉 PA 沒有比較安全。
      tx_queue_.clear();
    }
  }
  bool transmitInhibited() const { return tx_inhibited_; }

 private:
  enum class Mode { Receive, Transmit };

  static void onIrq() {
    if (active_ != nullptr) {
      active_->irq_ = true;
    }
  }

  bool startReceive() {
    radio_.clearPacketSentAction();
    radio_.setPacketReceivedAction(onIrq);
    mode_ = Mode::Receive;
    last_error_ = radio_.startReceive();
    return last_error_ == RADIOLIB_ERR_NONE;
  }

  void readReceived() {
    const std::size_t length = radio_.getPacketLength();
    if (length == 0U || length > kMaxRadioPacketBytes) {
      last_error_ = RADIOLIB_ERR_PACKET_TOO_LONG;
      return;
    }
    RadioRxPacket packet{};
    packet.bytes.resize(length);
    last_error_ = radio_.readData(packet.bytes.data(), length);
    if (last_error_ != RADIOLIB_ERR_NONE) {
      return;
    }
    packet.rssi_dbm = radio_.getRSSI();
    packet.snr_db = radio_.getSNR();
    if (rx_queue_.size() >= kMaxRxQueue) {
      rx_queue_.pop_front();
    }
    rx_queue_.push_back(std::move(packet));
  }

  void tryStartTransmit(const std::uint32_t now_ms) {
    radio_.clearPacketReceivedAction();
    radio_.standby();
    const int16_t channel = radio_.scanChannel();
    if (channel == RADIOLIB_LORA_DETECTED) {
      next_tx_at_ms_ = now_ms + 50U + (esp_random() % 151U);
      startReceive();
      return;
    }
    if (channel != RADIOLIB_CHANNEL_FREE) {
      last_error_ = channel;
      next_tx_at_ms_ = now_ms + 200U;
      startReceive();
      return;
    }

    active_tx_ = std::move(tx_queue_.front());
    tx_queue_.pop_front();
    radio_.setPacketSentAction(onIrq);
    mode_ = Mode::Transmit;
    last_error_ =
        radio_.startTransmit(active_tx_.data(), active_tx_.size());
    if (last_error_ != RADIOLIB_ERR_NONE) {
      active_tx_.clear();
      startReceive();
    }
  }

  Module module_;
  SX1262 radio_;
  m5::PI4IOE5V6408_Class ioe_;
  volatile bool irq_ = false;
  bool ready_ = false;
  bool module_detected_ = false;
  // Fail-safe 預設：在使用者確認天線已安裝之前一律禁止發射。
  bool tx_inhibited_ = true;
  Mode mode_ = Mode::Receive;
  int last_error_ = RADIOLIB_ERR_NONE;
  std::uint32_t next_tx_at_ms_ = 0;
  std::deque<std::vector<std::uint8_t>> tx_queue_;
  std::deque<RadioRxPacket> rx_queue_;
  std::vector<std::uint8_t> active_tx_;

  static Impl* active_;
};

RadioService::Impl* RadioService::Impl::active_ = nullptr;

RadioService::RadioService() : impl_(new Impl()) {}
RadioService::~RadioService() = default;
bool RadioService::begin(const GroupProfile& profile) {
  return impl_->begin(profile);
}
void RadioService::update(const std::uint32_t now_ms) { impl_->update(now_ms); }
bool RadioService::queueTransmit(const std::vector<std::uint8_t>& bytes) {
  return impl_->queueTransmit(bytes);
}
bool RadioService::queueTransmitBatch(
    const std::vector<std::vector<std::uint8_t>>& packets) {
  return impl_->queueTransmitBatch(packets);
}
bool RadioService::pollReceived(RadioRxPacket& packet) {
  return impl_->pollReceived(packet);
}
bool RadioService::ready() const { return impl_->ready(); }
int RadioService::lastError() const { return impl_->lastError(); }
std::size_t RadioService::queuedCount() const { return impl_->queuedCount(); }
bool RadioService::moduleDetected() const { return impl_->moduleDetected(); }
void RadioService::setTransmitInhibited(const bool inhibited) {
  impl_->setTransmitInhibited(inhibited);
}
bool RadioService::transmitInhibited() const {
  return impl_->transmitInhibited();
}

}  // namespace cmt

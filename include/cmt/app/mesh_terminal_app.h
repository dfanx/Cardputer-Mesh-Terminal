#pragma once

#include "cmt/core/channel_plan.h"
#include "cmt/core/fragmentation.h"
#include "cmt/core/mesh_router.h"
#include "cmt/core/message_codec.h"
#include "cmt/core/sequence_tracker.h"
#include "cmt/platform/audio_service.h"
#include "cmt/platform/device_state.h"
#include "cmt/platform/gnss_service.h"
#include "cmt/platform/mbedtls_crypto.h"
#include "cmt/platform/radio_service.h"
#include "cmt/platform/storage_service.h"
#include "cmt/platform/terminal_ui.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cmt {

class MeshTerminalApp {
 public:
  void setup();
  void loop();

 private:
  enum class Screen {
    UserId,
    Pairing,
    AntennaCheck,
    Home,
    MessageMenu,
    TextInput,
    History,
    Recording,
    Notice,
  };

  struct PeerState {
    std::uint32_t node_id = 0;
    BeaconMessage beacon{};
    std::uint32_t last_seen_ms = 0;
    float rssi_dbm = 0.0F;
  };

  void handleInput(std::uint32_t now_ms);
  void handleUserIdInput();
  void handlePairingInput();
  void handleAntennaCheckInput(std::uint32_t now_ms);
  // 單一權威開關：同時更新應用層旗標與 RadioService 的發射閘。
  void setAntennaConfirmed(bool confirmed, std::uint32_t now_ms);
  // 未確認天線時顯示明確原因並回傳 true，讓呼叫端停止送出。
  bool blockedByAntenna();
  void handleHomeInput(std::uint32_t now_ms, bool space_held);
  void handleMenuInput();
  void handleTextInput();
  void handleHistoryInput();
  void handleNoticeInput();
  void handleRadio(std::uint32_t now_ms);
  void handleDecodedMessage(const ReassemblyResult& result, float rssi_dbm,
                            std::uint32_t now_ms);
  void updateTrackAndBeacon(std::uint32_t now_ms);
  void render(std::uint32_t now_ms, bool force = false);

  bool joinGroup();
  bool sendText(const std::string& text);
  bool sendBeacon();
  bool sendVoice(const std::vector<std::uint8_t>& encoded);
  bool sendPayload(MessageType type, const std::vector<std::uint8_t>& payload);

  // sendPayload() 失敗時是哪一道閘門擋的，可直接顯示給使用者；沒有任何閘門
  // 成立時回傳 nullptr（代表失敗發生在封裝或佇列，不是前置條件）。
  const char* txBlockReason() const;
  PacketHeader makeBaseHeader(MessageType type);
  void addHistory(const std::string& entry);
  void showNotice(const std::string& title, const std::string& message,
                  std::uint16_t color);
  UiHomeModel buildHomeModel(std::uint32_t now_ms) const;

  TerminalUi ui_;
  DeviceState device_state_;
  NonceGenerator nonce_generator_;
  GnssService gnss_;
  StorageService storage_;
  RadioService radio_;
  AudioService audio_;
  MbedTlsCrypto crypto_;
  MeshRouter router_;
  Reassembler reassembler_;
  SequenceTracker sequence_tracker_;

  Screen screen_ = Screen::UserId;
  GroupProfile group_{};
  std::string user_id_;
  std::string user_id_error_;
  std::string pin_;
  std::string pairing_error_;
  std::string text_input_;
  std::vector<PeerState> peers_;
  std::vector<std::string> history_;
  std::size_t menu_selected_ = 0;
  std::size_t peer_selected_ = 0;
  std::size_t history_selected_ = 0;
  std::string notice_title_;
  std::string notice_message_;
  std::uint16_t notice_color_ = 0;
  bool paired_ = false;
  // 天線無法用硬體偵測，只能由使用者每次開機確認。預設 false = 禁止發射。
  bool antenna_confirmed_ = false;
  bool dirty_ = true;
  bool previous_space_held_ = false;
  std::uint32_t last_render_ms_ = 0;
  std::uint32_t next_beacon_ms_ = 0;
};

}  // namespace cmt

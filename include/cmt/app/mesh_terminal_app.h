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
#include "cmt/platform/message_store.h"
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
    Inbox,
    Notice,
  };

  struct PeerState {
    std::uint32_t node_id = 0;
    // 任何通過認證的封包都會建立 / 更新隊友，不必等到 Beacon。callsign 要等對方的
    // Beacon 才有，之前先用 node id 顯示。
    std::string callsign;
    GeoPoint position{};
    std::uint8_t battery_percent = 0;
    bool had_beacon = false;
    std::uint32_t last_seen_ms = 0;
    float rssi_dbm = 0.0F;
  };

  // 一則等待使用者確認的來訊。語音在確認之前不會播放。
  struct InboxItem {
    MessageType type = MessageType::Text;
    std::uint32_t source_id = 0;
    std::string sender;
    std::string label;
    std::string text;
    std::uint32_t clip_id = 0;
    // 只有在音檔存不進 logfs 時才用到：這一則至少還能聽一次，關掉就沒了。
    std::vector<std::uint8_t> frames;
    bool played = false;
  };

  struct HistoryEntry {
    std::string text;
    std::uint32_t clip_id = 0;
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
  void handleInboxInput();
  void handleNoticeInput();
  void handleRadio(std::uint32_t now_ms);
  void handleDecodedMessage(const ReassemblyResult& result, float rssi_dbm,
                            std::uint32_t now_ms);
  void updateBeacon(std::uint32_t now_ms);
  void render(std::uint32_t now_ms, bool force = false);

  bool joinGroup();
  bool sendText(const std::string& text);
  bool sendBeacon();
  // 罐頭訊息選單的「分享目前位置」項目：立刻送一次 Beacon 並重設下一次排程。
  void shareLocation();
  bool sendVoice(const std::vector<std::uint8_t>& encoded);
  bool sendPayload(MessageType type, const std::vector<std::uint8_t>& payload);

  // sendPayload() 失敗時是哪一道閘門擋的，可直接顯示給使用者；沒有任何閘門
  // 成立時回傳 nullptr（代表失敗發生在封裝或佇列，不是前置條件）。
  const char* txBlockReason() const;
  PacketHeader makeBaseHeader(MessageType type);

  // 建立或更新隊友紀錄，回傳該筆的參考。
  PeerState& touchPeer(std::uint32_t node_id, float rssi_dbm,
                       std::uint32_t now_ms);
  std::string peerName(std::uint32_t node_id) const;
  // 收到新隊友時把自己的 Beacon 提前，讓雙方位置盡快對上。
  void expediteBeacon(std::uint32_t now_ms);

  // 只進 RAM 的顯示用紀錄，給 store 尚未就緒或不值得落地的訊息。
  void addHistory(const std::string& entry, std::uint32_t clip_id = 0);
  // 同時進 RAM 與 logfs 的系統事件（天線、隊友上線、漏訊）。
  void recordSystem(const std::string& entry);
  // 寫入持久紀錄並同步 RAM 內的顯示用清單。
  void recordText(LogKind kind, bool outgoing, const std::string& sender,
                  const std::string& display_text, const std::string& body,
                  std::uint32_t source_id);
  // 回傳可重播的 clip id，0 表示沒能存下音檔（歷史紀錄仍會寫入）。
  std::uint32_t recordVoice(bool outgoing, const std::string& sender,
                            const std::string& display_text,
                            const std::vector<std::uint8_t>& frames,
                            std::uint32_t source_id);
  void loadHistoryFromStore();
  void enqueueInbox(InboxItem item);
  bool playClip(std::uint32_t clip_id);
  bool playInboxVoice(const InboxItem& item);

  void showNotice(const std::string& title, const std::string& message,
                  std::uint16_t color);
  UiHomeModel buildHomeModel(std::uint32_t now_ms) const;
  std::vector<UiHistoryEntry> buildHistoryModel() const;
  UiInboxItem buildInboxModel() const;

  TerminalUi ui_;
  DeviceState device_state_;
  NonceGenerator nonce_generator_;
  GnssService gnss_;
  StorageService storage_;
  MessageStore store_;
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
  std::vector<HistoryEntry> history_;
  std::vector<InboxItem> inbox_;
  std::size_t menu_selected_ = 0;
  std::size_t peer_selected_ = 0;
  std::size_t history_selected_ = 0;
  // 歷史畫面按 D 觸發，等待 Y/N 確認整批刪除。獨立旗標，不重用 Notice 畫面：
  // 這裡需要攔截 Y/N 兩種鍵，Notice 是任意鍵即返回。
  bool history_confirm_clear_ = false;
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

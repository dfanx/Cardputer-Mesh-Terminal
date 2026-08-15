#pragma once

#include "cmt/core/geo.h"
#include "cmt/core/types.h"
#include "cmt/platform/gnss_service.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cmt {

struct UiPeer {
  std::string callsign;
  RelativePosition relative{};
  std::uint8_t battery_percent = 0;
  std::uint32_t age_seconds = 0;
  float rssi_dbm = 0.0F;
  // 對方回報自己還沒定位時為 false：名單上看得到人，只是位置未知。
  bool has_position = false;
};

struct UiHomeModel {
  std::string callsign;
  std::uint32_t group_id = 0;
  float frequency_mhz = 0.0F;
  int battery_percent = -1;
  bool radio_ready = false;
  bool sd_ready = false;
  bool voice_ready = false;
  std::uint8_t volume_percent = 0;
  std::uint8_t satellites = 0;
  std::uint8_t satellites_in_view = 0;
  GnssLink gnss_link = GnssLink::NoData;
  GeoPoint own_position{};
  std::vector<GeoPoint> track;
  std::vector<UiPeer> peers;
  std::size_t selected_peer = 0;
  std::size_t tx_queued = 0;
  // 尚未確認讀取的來訊數量。大於 0 時主畫面必須明顯提示。
  std::size_t unread_count = 0;
  // 天線未確認安裝時為 true：發射全面停用，接收仍正常。
  bool tx_inhibited = true;
};

struct UiHistoryEntry {
  std::string text;
  // 有存下可重播的語音音檔。
  bool has_clip = false;
};

// 一則待確認的來訊。語音要等使用者按下確認才播放，不會自己出聲。
struct UiInboxItem {
  std::string sender;
  std::string label;
  std::string text;
  bool is_voice = false;
  bool played = false;
  bool clip_available = false;
  std::size_t index = 0;
  std::size_t total = 0;
};

class TerminalUi {
 public:
  void begin();
  // 配置 240x135x16bpp 的離螢幕緩衝（約 64 KB），整頁畫完再一次推送以消除閃爍。
  // 這塊記憶體要跟 Codec2 搶同一份 320 KB DRAM，所以刻意獨立於 begin()：由
  // 呼叫端在音訊初始化完成後才要，配置失敗就退回直接畫，只是會閃，功能不受影響。
  bool enableFrameBuffer();
  void renderBoot(const char* status);
  void renderUserId(const std::string& user_id, const char* error);
  // PIN 以明碼顯示。4 位 PIN 的安全目標只有群組辨識與基本內容遮蔽，遮成 * 擋不住
  // 任何實際威脅，卻讓使用者看不出自己按錯哪一位。
  void renderPairing(const std::string& pin, const char* error);
  void renderAntennaCheck(bool module_detected);
  void renderHome(const UiHomeModel& model);
  void renderMessageMenu(const std::vector<std::string>& messages,
                         std::size_t selected);
  void renderTextInput(const std::string& text);
  void renderRecording(float progress);
  void renderHistory(const std::vector<UiHistoryEntry>& history,
                     std::size_t selected);
  void renderInbox(const UiInboxItem& item);
  void renderNotice(const std::string& title, const std::string& message,
                    std::uint16_t color);

 private:
  void header(const char* title, std::uint16_t color);
  void drawTrack(const std::vector<GeoPoint>& track, int x, int y, int width,
                 int height);
  void drawHomeHints(const UiHomeModel& model);
  void drawWrapped(const std::string& text, int x, int y, int line_height,
                   int max_width_px, int max_lines);
};

}  // namespace cmt

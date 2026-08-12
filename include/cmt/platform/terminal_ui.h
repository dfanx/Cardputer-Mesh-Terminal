#pragma once

#include "cmt/core/geo.h"
#include "cmt/core/types.h"

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
  GeoPoint own_position{};
  std::vector<GeoPoint> track;
  std::vector<UiPeer> peers;
  std::size_t selected_peer = 0;
  std::size_t tx_queued = 0;
  // 天線未確認安裝時為 true：發射全面停用，接收仍正常。
  bool tx_inhibited = true;
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
  void renderPairing(const std::string& masked_pin, const char* error);
  void renderAntennaCheck(bool module_detected);
  void renderHome(const UiHomeModel& model);
  void renderMessageMenu(const std::vector<std::string>& messages,
                         std::size_t selected);
  void renderTextInput(const std::string& text);
  void renderRecording(float progress);
  void renderHistory(const std::vector<std::string>& history,
                     std::size_t selected);
  void renderNotice(const std::string& title, const std::string& message,
                    std::uint16_t color);

 private:
  void header(const char* title, std::uint16_t color);
  void drawTrack(const std::vector<GeoPoint>& track, int x, int y, int width,
                 int height);
  void drawHomeHints(bool tx_inhibited);
};

}  // namespace cmt

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
};

class TerminalUi {
 public:
  void begin();
  void renderBoot(const char* status);
  void renderPairing(const std::string& masked_pin, const char* error);
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
};

}  // namespace cmt

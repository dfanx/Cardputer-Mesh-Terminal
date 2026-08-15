#include "cmt/platform/terminal_ui.h"

#include <M5Cardputer.h>

#include <algorithm>
#include <cstdio>

namespace cmt {
namespace {

constexpr std::uint16_t kBackground = 0x0000;
constexpr std::uint16_t kPanel = 0x18C3;
constexpr std::uint16_t kBlue = 0x041F;
constexpr std::uint16_t kGreen = 0x07E0;
constexpr std::uint16_t kYellow = 0xFFE0;
constexpr std::uint16_t kRed = 0xF800;
constexpr std::uint16_t kOrange = 0xFC00;
constexpr std::uint16_t kWhite = 0xFFFF;
constexpr std::uint16_t kMuted = 0x8410;

// 只有一個 UI 實例，離螢幕緩衝就放在這裡。整頁在記憶體裡畫完再一次推到面板，
// 是消除閃爍的關鍵：先前每次更新都直接對面板 fillScreen 再逐項重畫，肉眼會看到
// 「整頁變黑 → 內容浮現」。240x135x16bpp = 64,800 bytes，配置失敗時退回直接畫，
// 功能不受影響（只是會閃）。
M5Canvas g_canvas;
bool g_buffered = false;

lgfx::LovyanGFX& gfx() {
  if (g_buffered) {
    return g_canvas;
  }
  return M5Cardputer.Display;
}

void endFrame() {
  if (g_buffered) {
    g_canvas.pushSprite(&M5Cardputer.Display, 0, 0);
  }
}

// efontTW_12 下 ASCII 約 6 px、CJK 約 12 px。文字含中文時不能用位元組數當寬度，
// 也不能從位元組中間截斷，否則畫面會出現亂碼方塊。
int glyphWidth(const unsigned char lead) {
  return lead >= 0xE0U ? 12 : 6;
}

std::size_t sequenceBytes(const unsigned char lead) {
  if (lead < 0x80U) {
    return 1U;
  }
  if (lead >= 0xF0U) {
    return 4U;
  }
  if (lead >= 0xE0U) {
    return 3U;
  }
  if (lead >= 0xC0U) {
    return 2U;
  }
  return 1U;  // 落單的接續位元組，當成單一字元跳過
}

// 回傳從 `start` 起、寬度不超過 max_width_px 的位元組數，永遠停在字元邊界上。
std::size_t bytesWithinWidth(const std::string& text, const std::size_t start,
                             const int max_width_px) {
  int width = 0;
  std::size_t index = start;
  while (index < text.size()) {
    const auto lead = static_cast<unsigned char>(text[index]);
    const int advance = glyphWidth(lead);
    if (width + advance > max_width_px) {
      break;
    }
    width += advance;
    index += std::min(sequenceBytes(lead), text.size() - index);
  }
  return index - start;
}

std::string clipToWidth(const std::string& text, const int max_width_px) {
  const std::size_t bytes = bytesWithinWidth(text, 0U, max_width_px);
  return bytes >= text.size() ? text : text.substr(0U, bytes);
}

// Beacon 每 10 分鐘一次，所以隊友的位置本來就會是舊的。畫面上必須看得出來有多舊，
// 否則使用者會把半小時前的方位當成現在的方位。
void formatAge(const std::uint32_t seconds, char* out, const std::size_t size) {
  if (seconds < 60UL) {
    std::snprintf(out, size, "%us", static_cast<unsigned>(seconds));
  } else if (seconds < 3600UL) {
    std::snprintf(out, size, "%um", static_cast<unsigned>(seconds / 60UL));
  } else if (seconds < 86400UL) {
    std::snprintf(out, size, "%uh", static_cast<unsigned>(seconds / 3600UL));
  } else {
    std::snprintf(out, size, ">1d");
  }
}

}  // namespace

void TerminalUi::begin() {
  auto& display = M5Cardputer.Display;
  display.setRotation(1);
  display.setTextWrap(false);
  display.setFont(&fonts::efontTW_12);
  display.setTextColor(kWhite, kBackground);
  display.fillScreen(kBackground);
}

bool TerminalUi::enableFrameBuffer() {
  auto& display = M5Cardputer.Display;
  if (g_buffered) {
    return true;
  }
  g_canvas.setColorDepth(16);
  g_buffered =
      g_canvas.createSprite(display.width(), display.height()) != nullptr;
  if (g_buffered) {
    g_canvas.setTextWrap(false);
    g_canvas.setFont(&fonts::efontTW_12);
    g_canvas.setTextColor(kWhite, kBackground);
    g_canvas.fillScreen(kBackground);
  } else {
    Serial.println("[ui] frame buffer alloc failed, direct draw fallback");
  }
  return g_buffered;
}

void TerminalUi::header(const char* title, const std::uint16_t color) {
  auto& display = gfx();
  display.fillScreen(kBackground);
  display.fillRect(0, 0, display.width(), 19, color);
  display.setTextColor(kWhite, color);
  display.setCursor(5, 3);
  display.print(title);
  display.setTextColor(kWhite, kBackground);
}

void TerminalUi::renderBoot(const char* status) {
  header("Cardputer Mesh Terminal", kBlue);
  auto& display = gfx();
  display.setCursor(8, 38);
  display.print("安裝天線後才可使用 LoRa");
  display.setTextColor(kYellow, kBackground);
  display.setCursor(8, 66);
  display.print(status);
  display.setTextColor(kMuted, kBackground);
  display.setCursor(8, 102);
  display.print("不是正式救援設備");
  endFrame();
}

void TerminalUi::renderUserId(const std::string& user_id, const char* error) {
  header("使用者代號", kBlue);
  auto& display = gfx();
  display.setCursor(8, 26);
  display.print("這台機器是誰？");
  display.setTextColor(kMuted, kBackground);
  display.setCursor(8, 42);
  display.print("大寫英數與 - _，最多 8 字");
  display.drawRoundRect(48, 58, 144, 30, 4, kWhite);
  display.setTextColor(kWhite, kBackground);
  display.setTextSize(2);
  display.setCursor(56, 64);
  display.print(user_id.c_str());
  display.setTextSize(1);
  display.setCursor(8, 96);
  display.setTextColor(kMuted, kBackground);
  display.print("Enter 確認 / Backspace 刪除");
  if (error != nullptr && error[0] != '\0') {
    display.setTextColor(kRed, kBackground);
    display.setCursor(8, 115);
    display.print(error);
  } else {
    display.setCursor(8, 115);
    display.print("代號會顯示在隊友的雷達與訊息上");
  }
  display.setTextColor(kWhite, kBackground);
  endFrame();
}

void TerminalUi::renderPairing(const std::string& pin,
                               const char* error) {
  header("群組配對", kBlue);
  auto& display = gfx();
  display.setCursor(8, 31);
  display.print("輸入 4 位群組 PIN");
  display.drawRoundRect(48, 53, 144, 34, 4, kWhite);
  display.setTextSize(2);
  display.setCursor(78, 59);
  display.print(pin.c_str());
  display.setTextSize(1);
  display.setCursor(8, 96);
  display.setTextColor(kMuted, kBackground);
  display.print("Enter 確認 / Backspace 刪除");
  display.setCursor(8, 115);
  if (error != nullptr && error[0] != '\0') {
    display.setTextColor(kRed, kBackground);
    display.print(error);
  } else {
    display.print("Esc 回上一步改代號");
  }
  display.setTextColor(kWhite, kBackground);
  endFrame();
}

void TerminalUi::renderAntennaCheck(const bool module_detected) {
  header("天線安全檢查", kRed);
  auto& display = gfx();
  if (module_detected) {
    display.setTextColor(kWhite, kBackground);
    display.setCursor(8, 26);
    display.print("已偵測到 LoRa 模組");
  } else {
    display.setTextColor(kYellow, kBackground);
    display.setCursor(8, 26);
    display.print("偵測不到 LoRa 模組");
  }
  display.setTextColor(kRed, kBackground);
  display.setCursor(8, 46);
  display.print("無天線發射會燒毀功率放大器");
  display.setTextColor(kWhite, kBackground);
  display.setCursor(8, 66);
  display.print("天線是否已鎖上？");
  display.setTextColor(kGreen, kBackground);
  display.setCursor(8, 90);
  display.print("Y = 已安裝，啟用發射");
  display.setTextColor(kYellow, kBackground);
  display.setCursor(8, 110);
  display.print("N = 未安裝，只接收不發射");
  display.setTextColor(kWhite, kBackground);
  endFrame();
}

void TerminalUi::drawTrack(const std::vector<GeoPoint>& track, const int x,
                           const int y, const int width, const int height) {
  auto& display = gfx();
  display.drawRect(x, y, width, height, kMuted);
  if (track.empty()) {
    display.setTextColor(kMuted, kBackground);
    display.setCursor(x + 8, y + height / 2 - 6);
    display.print("等待軌跡");
    return;
  }

  double min_lat = track.front().latitude;
  double max_lat = min_lat;
  double min_lon = track.front().longitude;
  double max_lon = min_lon;
  for (const auto& point : track) {
    min_lat = std::min(min_lat, point.latitude);
    max_lat = std::max(max_lat, point.latitude);
    min_lon = std::min(min_lon, point.longitude);
    max_lon = std::max(max_lon, point.longitude);
  }
  const double lat_range = std::max(0.00001, max_lat - min_lat);
  const double lon_range = std::max(0.00001, max_lon - min_lon);
  int previous_x = 0;
  int previous_y = 0;
  bool have_previous = false;
  for (const auto& point : track) {
    const int px = x + 3 + static_cast<int>(
                               (point.longitude - min_lon) / lon_range *
                               static_cast<double>(width - 7));
    const int py = y + height - 4 - static_cast<int>(
                                      (point.latitude - min_lat) / lat_range *
                                      static_cast<double>(height - 7));
    if (have_previous) {
      display.drawLine(previous_x, previous_y, px, py, kGreen);
    }
    previous_x = px;
    previous_y = py;
    have_previous = true;
  }
  display.fillCircle(previous_x, previous_y, 2, kYellow);
}

void TerminalUi::drawWrapped(const std::string& text, const int x, const int y,
                             const int line_height, const int max_width_px,
                             const int max_lines) {
  auto& display = gfx();
  std::size_t offset = 0;
  for (int line = 0; line < max_lines && offset < text.size(); ++line) {
    std::size_t bytes = bytesWithinWidth(text, offset, max_width_px);
    if (bytes == 0U) {
      break;
    }
    display.setCursor(x, y + line * line_height);
    display.print(text.substr(offset, bytes).c_str());
    offset += bytes;
  }
}

void TerminalUi::drawHomeHints(const UiHomeModel& model) {
  auto& display = gfx();
  // 主畫面下緣固定保留兩行。第一行讓給當下最該被看到的事：未讀優先於天線警告，
  // 因為未讀是需要立刻操作的，天線狀態在雷達區也看得到。
  if (model.unread_count > 0U) {
    display.fillRect(0, 100, display.width(), 17, kRed);
    display.setTextColor(kWhite, kRed);
    display.setCursor(4, 102);
    display.printf("● %u 則未讀  Enter 讀取",
                   static_cast<unsigned>(model.unread_count));
    display.fillRect(0, 117, display.width(), 18, kPanel);
    display.setTextColor(kWhite, kPanel);
    display.setCursor(4, 119);
    display.print(model.tx_inhibited ? "無天線！A=確認  ↑=歷史"
                                     : "T=訊息 ↑=歷史 SPACE=語音");
  } else if (model.tx_inhibited) {
    display.fillRect(0, 100, display.width(), 17, kRed);
    display.setTextColor(kWhite, kRed);
    display.setCursor(4, 102);
    display.print("無天線！發射停用 A=確認");
    display.fillRect(0, 117, display.width(), 18, kPanel);
    display.setTextColor(kWhite, kPanel);
    display.setCursor(4, 119);
    display.print("T=訊息 ↑=歷史 -/= 音量 SPACE=語音");
  } else {
    display.fillRect(0, 100, display.width(), 35, kPanel);
    display.setTextColor(kWhite, kPanel);
    display.setCursor(4, 102);
    display.print("T=訊息  ↑=歷史  ←→=隊友");
    display.setCursor(4, 119);
    display.print("-/= 音量  SPACE=語音  A=天線");
  }
  display.setTextColor(kWhite, kBackground);
}

void TerminalUi::renderHome(const UiHomeModel& model) {
  auto& display = gfx();
  display.fillScreen(kBackground);
  display.fillRect(0, 0, display.width(), 18, kBlue);
  display.setTextColor(kWhite, kBlue);
  display.setCursor(3, 2);
  display.printf("%s G:%04lX", model.callsign.c_str(),
                 static_cast<unsigned long>(model.group_id & 0xFFFFUL));
  display.setCursor(164, 2);
  if (model.battery_percent >= 0) {
    display.printf("B%02d L%02u", model.battery_percent,
                   static_cast<unsigned>(model.volume_percent));
  } else {
    display.printf("B-- L%02u", static_cast<unsigned>(model.volume_percent));
  }

  drawTrack(model.track, 2, 21, 145, 77);
  // GPS 只顯示一個 0 沒辦法判斷該走到室外還是該檢查模組，所以定位數/可見數與
  // 鏈路狀態一起給。
  const bool gnss_alive = model.gnss_link == GnssLink::Fixed ||
                          model.gnss_link == GnssLink::NoFix;
  display.setTextColor(model.gnss_link == GnssLink::Fixed ? kGreen
                       : gnss_alive                       ? kYellow
                                                          : kRed,
                       kBackground);
  display.setCursor(151, 23);
  display.printf("G %u/%u Q%u", model.satellites, model.satellites_in_view,
                 static_cast<unsigned>(model.tx_queued));
  display.setTextColor(kWhite, kBackground);
  display.setCursor(151, 39);
  display.printf("%.3fM", model.frequency_mhz);
  display.setCursor(151, 55);
  display.printf("R:%s S:%s V:%s", model.radio_ready ? "Y" : "N",
                 model.sd_ready ? "Y" : "N", model.voice_ready ? "Y" : "N");

  if (!model.peers.empty()) {
    const UiPeer& peer =
        model.peers[model.selected_peer % model.peers.size()];
    char age[8]{};
    formatAge(peer.age_seconds, age, sizeof(age));
    display.setTextColor(kYellow, kBackground);
    display.setCursor(151, 71);
    // 代號與「多久前聽到」放同一行，位置有沒有解出來都看得到隊友的時效。
    display.printf("%u/%u %s %s",
                   static_cast<unsigned>(
                       (model.selected_peer % model.peers.size()) + 1U),
                   static_cast<unsigned>(model.peers.size()),
                   clipToWidth(peer.callsign, 36).c_str(), age);
    display.setCursor(151, 87);
    if (peer.relative.valid) {
      display.printf("%s %.1fkm", peer.relative.direction,
                     peer.relative.distance_m / 1000.0);
    } else if (!peer.has_position) {
      // 隊友自己回報沒有定位，跟「我方沒定位所以算不出相對位置」是兩件事。
      display.print("對方無定位");
    } else if (!model.own_position.valid) {
      display.print("本機無定位");
    } else {
      display.print("位置未知");
    }
  } else {
    display.setTextColor(kMuted, kBackground);
    display.setCursor(151, 71);
    display.print("尚無隊友");
    display.setCursor(151, 87);
    display.print(gnss_alive ? gnssLinkLabel(model.gnss_link) : "檢查 GNSS");
  }

  drawHomeHints(model);
  endFrame();
}

void TerminalUi::renderMessageMenu(
    const std::vector<std::string>& messages, const std::size_t selected) {
  header("罐頭訊息選單", kBlue);
  auto& display = gfx();
  const std::size_t total = messages.size() + 1U;
  const std::size_t first = selected > 3U ? selected - 3U : 0U;
  const std::size_t last = std::min(total, first + 5U);
  for (std::size_t index = first; index < last; ++index) {
    const int y = 22 + static_cast<int>(index - first) * 20;
    const bool active = index == selected;
    display.fillRect(2, y, 236, 19, active ? kPanel : kBackground);
    display.setTextColor(active ? kYellow : kWhite,
                         active ? kPanel : kBackground);
    display.setCursor(5, y + 2);
    if (index < messages.size()) {
      display.printf("%u. %s", static_cast<unsigned>(index + 1U),
                     messages[index].c_str());
    } else {
      display.print("自由輸入英數訊息...");
    }
  }
  display.setTextColor(kMuted, kBackground);
  display.setCursor(4, 120);
  display.print("↑↓選擇 Enter發送 1-9直送 Esc返回");
  endFrame();
}

void TerminalUi::renderTextInput(const std::string& text) {
  header("自由訊息", kBlue);
  auto& display = gfx();
  display.setCursor(5, 29);
  display.setTextColor(kMuted, kBackground);
  display.print("限 160 bytes；Enter 發送");
  display.drawRoundRect(4, 50, 232, 48, 3, kWhite);
  display.setTextColor(kWhite, kBackground);
  display.setCursor(9, 59);
  const std::string visible = text.size() > 29U
                                  ? text.substr(text.size() - 29U)
                                  : text;
  display.print(visible.c_str());
  display.setCursor(5, 112);
  // 這一頁的 ` 是可輸入字元，返回鍵只能是 Fn+Esc。
  display.printf("%u/160  Fn+Esc 返回", static_cast<unsigned>(text.size()));
  endFrame();
}

void TerminalUi::renderRecording(const float progress) {
  header("PTT 語音", kRed);
  auto& display = gfx();
  display.setTextColor(kWhite, kBackground);
  display.setCursor(63, 36);
  display.print("錄音中，最長 2.2 秒");
  display.drawRect(20, 67, 200, 20, kWhite);
  const int filled = static_cast<int>(198.0F * std::max(0.0F, 1.0F - progress));
  display.fillRect(21, 68, filled, 18, kRed);
  display.setCursor(49, 103);
  display.print("放開 Space 即發送");
  endFrame();
}

void TerminalUi::renderHistory(const std::vector<UiHistoryEntry>& history,
                               const std::size_t selected) {
  header("訊息歷史", kBlue);
  auto& display = gfx();
  bool selected_has_clip = false;
  if (history.empty()) {
    display.setTextColor(kMuted, kBackground);
    display.setCursor(74, 62);
    display.print("尚無訊息");
  } else {
    const std::size_t first = selected > 4U ? selected - 4U : 0U;
    const std::size_t last = std::min(history.size(), first + 5U);
    selected_has_clip =
        selected < history.size() && history[selected].has_clip;
    for (std::size_t index = first; index < last; ++index) {
      const int y = 22 + static_cast<int>(index - first) * 19;
      const bool active = index == selected;
      display.setTextColor(active ? kYellow : kWhite, kBackground);
      if (history[index].has_clip) {
        // 有音檔的那幾筆要一眼看得出來可以重播。
        display.setTextColor(active ? kYellow : kGreen, kBackground);
        display.setCursor(4, y);
        display.print("▶");
        display.setCursor(16, y);
      } else {
        display.setCursor(4, y);
      }
      display.print(clipToWidth(history[index].text,
                                history[index].has_clip ? 220 : 232)
                        .c_str());
    }
  }
  display.setTextColor(kMuted, kBackground);
  display.setCursor(4, 120);
  display.print(selected_has_clip ? "↑↓ 瀏覽  Enter 重播  Esc 返回"
                                  : "↑↓ 瀏覽 / Esc 返回");
  endFrame();
}

void TerminalUi::renderInbox(const UiInboxItem& item) {
  header(item.is_voice ? "收到語音" : "收到訊息",
         item.is_voice ? kOrange : kBlue);
  auto& display = gfx();
  display.setTextColor(kMuted, kBackground);
  display.setCursor(5, 24);
  display.printf("%u/%u  %s  %s", static_cast<unsigned>(item.index + 1U),
                 static_cast<unsigned>(item.total),
                 clipToWidth(item.sender, 84).c_str(), item.label.c_str());

  display.setTextColor(kWhite, kBackground);
  if (item.is_voice) {
    display.setCursor(5, 48);
    if (!item.clip_available) {
      display.setTextColor(kRed, kBackground);
      display.print("語音已不在儲存區");
    } else if (item.played) {
      display.print("已播放");
      display.setTextColor(kMuted, kBackground);
      display.setCursor(5, 68);
      display.print("之後可在歷史紀錄重播");
    } else {
      display.print("按 Enter 播放");
    }
  } else {
    drawWrapped(item.text, 5, 44, 17, 230, 4);
  }

  display.fillRect(0, 117, display.width(), 18, kPanel);
  display.setTextColor(kWhite, kPanel);
  display.setCursor(4, 119);
  if (item.is_voice && item.clip_available && !item.played) {
    display.print("Enter=播放  Esc=稍後  Del=標記已讀");
  } else {
    display.print("Enter=已讀下一則  Esc=稍後");
  }
  display.setTextColor(kWhite, kBackground);
  endFrame();
}

void TerminalUi::renderNotice(const std::string& title,
                              const std::string& message,
                              const std::uint16_t color) {
  header(title.c_str(), color);
  auto& display = gfx();
  display.setTextColor(kWhite, kBackground);
  display.setCursor(8, 45);
  display.print(message.c_str());
  display.setTextColor(kMuted, kBackground);
  display.setCursor(8, 112);
  display.print("按任意鍵返回");
  endFrame();
}

}  // namespace cmt

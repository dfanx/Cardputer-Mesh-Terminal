#include "cmt/app/mesh_terminal_app.h"

#include <M5Cardputer.h>

#include <algorithm>
#include <cstdio>

namespace cmt {
namespace {

constexpr std::uint32_t kBeaconIntervalMs = 10UL * 60UL * 1000UL;
// 收到沒見過的隊友時，把自己的下一次 Beacon 提前到這個範圍內（加隨機抖動避免
// 兩機同時搶同一個時槽），彼此的位置才不用等滿十分鐘才對上。
constexpr std::uint32_t kBeaconExpediteMinMs = 4000;
constexpr std::uint32_t kBeaconExpediteJitterMs = 6000;
constexpr std::size_t kMaxPeers = 12;
constexpr std::size_t kMaxHistory = 60;
constexpr std::size_t kMaxInbox = 16;
constexpr std::uint16_t kNoticeGreen = 0x07E0;
constexpr std::uint16_t kNoticeYellow = 0xFFE0;
constexpr std::uint16_t kNoticeRed = 0xF800;

std::string nodeLabel(const std::uint32_t node_id) {
  char value[12]{};
  std::snprintf(value, sizeof(value), "N%04lX",
                static_cast<unsigned long>(node_id & 0xFFFFUL));
  return value;
}

bool hasWordCharacter(const Keyboard_Class::KeysState& keys, const char value) {
  if (std::find(keys.word.begin(), keys.word.end(), value) != keys.word.end()) {
    return true;
  }
  if (value < 'a' || value > 'z') {
    return false;
  }
  return std::find(keys.word.begin(), keys.word.end(),
                   static_cast<char>(value - 'a' + 'A')) != keys.word.end();
}

// Cardputer 的方向鍵與 Esc 印在 ; . , / ` 這五顆鍵上，鍵盤驅動只有在 Fn 按下時
// 才把它們當方向鍵，但 keys.word 兩種情況都會帶出原字元。沒有文字輸入的畫面直接
// 接受單按，讓導覽不必用組合鍵；Fn + 同鍵仍然有效。
bool isEscape(const Keyboard_Class::KeysState& keys) {
  return hasWordCharacter(keys, '`') || hasWordCharacter(keys, '~');
}

// 自由訊息輸入頁的 ` 是可打字的字元，那一頁只能認 Fn+Esc。
bool isFnEscape(const Keyboard_Class::KeysState& keys) {
  return keys.fn && isEscape(keys);
}

bool isUp(const Keyboard_Class::KeysState& keys) {
  return hasWordCharacter(keys, ';');
}

bool isDown(const Keyboard_Class::KeysState& keys) {
  return hasWordCharacter(keys, '.');
}

bool isLeft(const Keyboard_Class::KeysState& keys) {
  return hasWordCharacter(keys, ',');
}

bool isRight(const Keyboard_Class::KeysState& keys) {
  return hasWordCharacter(keys, '/');
}

// 實體鍵是 - 與 =；shift 後會變成 _ 與 +，兩種字元都收下，使用者不必在意 shift。
bool isVolumeUp(const Keyboard_Class::KeysState& keys) {
  return hasWordCharacter(keys, '=') || hasWordCharacter(keys, '+');
}

bool isVolumeDown(const Keyboard_Class::KeysState& keys) {
  return hasWordCharacter(keys, '-') || hasWordCharacter(keys, '_');
}

std::string sequenceLabel(const PacketHeader& header) {
  std::uint32_t month_day = 0;
  if (header.timestamp != 0U) {
    std::int64_t days = static_cast<std::int64_t>(header.timestamp / 86400UL);
    days += 719468LL;
    const std::int64_t era =
        (days >= 0 ? days : days - 146096LL) / 146097LL;
    const unsigned day_of_era =
        static_cast<unsigned>(days - era * 146097LL);
    const unsigned year_of_era =
        (day_of_era - day_of_era / 1460U + day_of_era / 36524U -
         day_of_era / 146096U) /
        365U;
    const unsigned day_of_year =
        day_of_era -
        (365U * year_of_era + year_of_era / 4U - year_of_era / 100U);
    const unsigned month_prime = (5U * day_of_year + 2U) / 153U;
    const unsigned day =
        day_of_year - (153U * month_prime + 2U) / 5U + 1U;
    const unsigned month = month_prime < 10U ? month_prime + 3U
                                             : month_prime - 9U;
    month_day = month * 100U + day;
  }
  char value[24]{};
  std::snprintf(value, sizeof(value), "%04lu-%04lu",
                static_cast<unsigned long>(month_day),
                static_cast<unsigned long>(header.sequence % 10000UL));
  return value;
}

}  // namespace

void MeshTerminalApp::setup() {
  auto config = M5.config();
  config.clear_display = true;
  config.output_power = true;
  config.internal_spk = true;
  config.internal_mic = true;
  M5Cardputer.begin(config, true);
  Serial.begin(115200);

  ui_.begin();
  ui_.renderBoot("初始化核心與周邊...");
  device_state_.begin();
  nonce_generator_.begin(device_state_.nodeId());
  storage_.begin();
  store_.begin();
  gnss_.begin();
  audio_.begin();
  // 音訊之後才要畫面緩衝：記憶體不夠時寧可畫面閃爍，也不要犧牲 Codec2 語音。
  ui_.enableFrameBuffer();
  // 歷史存在 flash 的 logfs 分割區，重開機後照樣查得到，語音也還能重播。
  loadHistoryFromStore();
  if (!store_.ready()) {
    addHistory("[系統] 歷史儲存區不可用，本次不保存紀錄");
  }

  delay(350);
  // 第一步先問「這台機器是誰」，代號會進 Beacon 的 callsign，隊友才分得出來源。
  // 已設定過的代號直接帶入，回訪時按 Enter 就能過。群組 PIN 同理：重開機或重燒錄
  // 後常常就是要回到同一群組，先帶入上次成功配對的 PIN，使用者不必重打。
  user_id_ = device_state_.userId();
  pin_ = device_state_.pin();
  screen_ = Screen::UserId;
  dirty_ = true;
  render(millis(), true);
}

void MeshTerminalApp::loop() {
  const std::uint32_t now_ms = millis();
  M5Cardputer.update();
  gnss_.update(now_ms);
  audio_.update();
  if (paired_) {
    radio_.update(now_ms);
    handleRadio(now_ms);
    updateBeacon(now_ms);
  }
  handleInput(now_ms);
  render(now_ms);
  delay(2);
}

void MeshTerminalApp::handleInput(const std::uint32_t now_ms) {
  const bool space_held = M5Cardputer.Keyboard.isKeyPressed(' ');
  if (screen_ == Screen::Recording) {
    if (audio_.updateRecording(space_held, now_ms)) {
      // 錄音失敗與發送失敗是完全不同的問題，訊息必須分開：合成一則會讓使用者
      // 誤以為是自己按太短，實際上錄音好好的、被發射前置條件擋下。
      std::vector<std::uint8_t> encoded;
      if (!audio_.takeEncoded(encoded)) {
        showNotice("語音", "錄音過短，請按住 Space 說完再放開", kNoticeYellow);
      } else if (sendVoice(encoded)) {
        showNotice("語音", "語音已排入傳送", kNoticeGreen);
      } else {
        const char* reason = txBlockReason();
        showNotice("語音無法發送",
                   reason != nullptr ? reason : "發送佇列已滿，稍後再試",
                   kNoticeRed);
      }
    }
    // 錄音中不設 dirty_：進度條交給 render() 的 100 ms 週期更新就夠，否則會在
    // 每次主迴圈都重畫整頁，跟麥克風取樣搶 CPU。
    previous_space_held_ = space_held;
    return;
  }

  if (!M5Cardputer.Keyboard.isChange() ||
      !M5Cardputer.Keyboard.isPressed()) {
    previous_space_held_ = space_held;
    return;
  }

  switch (screen_) {
    case Screen::UserId:
      handleUserIdInput();
      break;
    case Screen::Pairing:
      handlePairingInput();
      break;
    case Screen::AntennaCheck:
      handleAntennaCheckInput(now_ms);
      break;
    case Screen::Home:
      handleHomeInput(now_ms, space_held);
      break;
    case Screen::MessageMenu:
      handleMenuInput();
      break;
    case Screen::TextInput:
      handleTextInput();
      break;
    case Screen::History:
      handleHistoryInput();
      break;
    case Screen::Inbox:
      handleInboxInput();
      break;
    case Screen::Notice:
      handleNoticeInput();
      break;
    case Screen::Recording:
      break;
  }
  previous_space_held_ = space_held;
}

void MeshTerminalApp::handleUserIdInput() {
  auto& keys = M5Cardputer.Keyboard.keysState();
  for (const char value : keys.word) {
    if (user_id_.size() >= kMaxUserIdBytes) {
      break;
    }
    // 一律存大寫，鍵盤是否按著 shift 都得到同一個代號。
    const char upper = (value >= 'a' && value <= 'z')
                           ? static_cast<char>(value - 'a' + 'A')
                           : value;
    if ((upper >= 'A' && upper <= 'Z') || (upper >= '0' && upper <= '9') ||
        upper == '-' || upper == '_') {
      user_id_.push_back(upper);
      user_id_error_.clear();
      dirty_ = true;
    }
  }
  if (keys.del && !user_id_.empty()) {
    user_id_.pop_back();
    user_id_error_.clear();
    dirty_ = true;
  }
  if (keys.enter) {
    if (device_state_.setUserId(user_id_)) {
      user_id_error_.clear();
      screen_ = Screen::Pairing;
    } else {
      user_id_error_ = "代號需 1-8 個大寫英數字元";
    }
    dirty_ = true;
  }
}

void MeshTerminalApp::handlePairingInput() {
  auto& keys = M5Cardputer.Keyboard.keysState();
  if (isEscape(keys)) {
    // 配對前都還沒發射，退回去改代號是安全的。PIN 不清空：不論是預帶入的上次
    // 記錄還是使用者正在打的內容，回到代號畫面再按 Enter 回來時應該還在。
    pairing_error_.clear();
    screen_ = Screen::UserId;
    dirty_ = true;
    return;
  }
  for (const char value : keys.word) {
    if (value >= '0' && value <= '9' && pin_.size() < 4U) {
      pin_.push_back(value);
      pairing_error_.clear();
      dirty_ = true;
    }
  }
  if (keys.del && !pin_.empty()) {
    pin_.pop_back();
    pairing_error_.clear();
    dirty_ = true;
  }
  if (keys.enter) {
    if (joinGroup()) {
      // 先過天線確認，確認前不排 Beacon，否則會在無天線狀態下自動發射。
      screen_ = Screen::AntennaCheck;
    }
    dirty_ = true;
  }
}

void MeshTerminalApp::setAntennaConfirmed(const bool confirmed,
                                          const std::uint32_t now_ms) {
  antenna_confirmed_ = confirmed;
  radio_.setTransmitInhibited(!confirmed);
  if (confirmed) {
    next_beacon_ms_ = now_ms + 10000UL;
    recordSystem("[系統] 已確認天線，發射啟用");
  } else {
    next_beacon_ms_ = 0U;
    recordSystem("[系統] 未確認天線，僅接收不發射");
  }
  dirty_ = true;
}

void MeshTerminalApp::handleAntennaCheckInput(const std::uint32_t now_ms) {
  auto& keys = M5Cardputer.Keyboard.keysState();
  if (hasWordCharacter(keys, 'y')) {
    setAntennaConfirmed(true, now_ms);
    screen_ = Screen::Home;
    return;
  }
  if (hasWordCharacter(keys, 'n') || isEscape(keys)) {
    setAntennaConfirmed(false, now_ms);
    screen_ = Screen::Home;
  }
}

void MeshTerminalApp::handleHomeInput(const std::uint32_t now_ms,
                                      const bool space_held) {
  auto& keys = M5Cardputer.Keyboard.keysState();
  if (isVolumeUp(keys)) {
    audio_.adjustVolume(10);
    // 調整後在新音量下發一聲，讓使用者靠聽的就知道現在多大聲，不必只看數字。
    audio_.playAlert(AlertTone::Confirm);
    dirty_ = true;
    return;
  }
  if (isVolumeDown(keys)) {
    audio_.adjustVolume(-10);
    audio_.playAlert(AlertTone::Confirm);
    dirty_ = true;
    return;
  }
  if (keys.enter && !inbox_.empty()) {
    screen_ = Screen::Inbox;
    dirty_ = true;
    return;
  }
  if (hasWordCharacter(keys, 'a')) {
    // 裝上或拆下天線後可隨時重新確認，不必重開機。
    screen_ = Screen::AntennaCheck;
    dirty_ = true;
    return;
  }
  if (hasWordCharacter(keys, 't')) {
    screen_ = Screen::MessageMenu;
    menu_selected_ = 0;
    dirty_ = true;
    return;
  }
  if (isUp(keys)) {
    screen_ = Screen::History;
    history_selected_ = history_.empty() ? 0U : history_.size() - 1U;
    history_confirm_clear_ = false;
    dirty_ = true;
    return;
  }
  if (isLeft(keys) && !peers_.empty()) {
    peer_selected_ =
        (peer_selected_ + peers_.size() - 1U) % peers_.size();
    dirty_ = true;
  }
  if (isRight(keys) && !peers_.empty()) {
    peer_selected_ = (peer_selected_ + 1U) % peers_.size();
    dirty_ = true;
  }
  if (space_held && !previous_space_held_) {
    if (!antenna_confirmed_) {
      showNotice("發射已停用", "未確認天線，按 A 確認後才能發送語音",
                 kNoticeRed);
    } else if (!audio_.available()) {
      showNotice("語音不可用", "Codec2 或音訊初始化失敗", kNoticeYellow);
    } else if (audio_.startRecording(now_ms)) {
      screen_ = Screen::Recording;
      dirty_ = true;
    } else {
      showNotice("語音", "音訊裝置目前不可用", kNoticeRed);
    }
  }
}

bool MeshTerminalApp::blockedByAntenna() {
  if (antenna_confirmed_) {
    return false;
  }
  showNotice("發射已停用", "未確認天線，按 A 確認後才能發送", kNoticeRed);
  return true;
}

void MeshTerminalApp::handleMenuInput() {
  auto& keys = M5Cardputer.Keyboard.keysState();
  const auto& messages = storage_.cannedMessages();
  // 罐頭訊息之後多兩格：分享目前位置、自由輸入，順序固定，自由輸入永遠最後一項。
  const std::size_t share_index = messages.size();
  const std::size_t free_text_index = messages.size() + 1U;
  const std::size_t total = messages.size() + 2U;
  if (isEscape(keys)) {
    screen_ = Screen::Home;
    dirty_ = true;
    return;
  }
  if (isUp(keys)) {
    menu_selected_ = (menu_selected_ + total - 1U) % total;
    dirty_ = true;
  }
  if (isDown(keys)) {
    menu_selected_ = (menu_selected_ + 1U) % total;
    dirty_ = true;
  }
  for (const char value : keys.word) {
    if (value >= '1' && value <= '9') {
      const std::size_t index = static_cast<std::size_t>(value - '1');
      if (index < messages.size()) {
        if (blockedByAntenna()) {
          return;
        }
        if (sendText(messages[index])) {
          showNotice("訊息", "罐頭訊息已排入傳送", kNoticeGreen);
        } else {
          showNotice("訊息", "傳送佇列不可用", kNoticeRed);
        }
        return;
      }
      if (index == share_index) {
        shareLocation();
        return;
      }
    }
  }
  if (keys.enter) {
    if (menu_selected_ < messages.size()) {
      if (blockedByAntenna()) {
        return;
      }
      if (sendText(messages[menu_selected_])) {
        showNotice("訊息", "罐頭訊息已排入傳送", kNoticeGreen);
      } else {
        showNotice("訊息", "傳送佇列不可用", kNoticeRed);
      }
    } else if (menu_selected_ == share_index) {
      shareLocation();
    } else if (menu_selected_ == free_text_index) {
      text_input_.clear();
      screen_ = Screen::TextInput;
      dirty_ = true;
    }
  }
}

void MeshTerminalApp::handleTextInput() {
  auto& keys = M5Cardputer.Keyboard.keysState();
  if (isFnEscape(keys)) {
    screen_ = Screen::MessageMenu;
    dirty_ = true;
    return;
  }
  if (keys.del && !text_input_.empty()) {
    text_input_.pop_back();
    dirty_ = true;
  }
  for (const char value : keys.word) {
    if (value >= 0x20 && value <= 0x7E && text_input_.size() < kMaxTextBytes) {
      text_input_.push_back(value);
      dirty_ = true;
    }
  }
  if (keys.enter && !text_input_.empty()) {
    if (blockedByAntenna()) {
      return;
    }
    if (sendText(text_input_)) {
      showNotice("訊息", "自由訊息已排入傳送", kNoticeGreen);
    } else {
      showNotice("訊息", "傳送佇列不可用", kNoticeRed);
    }
  }
}

void MeshTerminalApp::handleHistoryInput() {
  auto& keys = M5Cardputer.Keyboard.keysState();
  if (history_confirm_clear_) {
    // 清除是不可逆操作，獨立出一個確認狀態，任何鍵都不會被前一個畫面的殘留
    // 按鍵誤觸發——沒按到 Y 就一律當取消。
    if (hasWordCharacter(keys, 'y')) {
      const bool cleared = store_.clearAll();
      history_.clear();
      history_selected_ = 0U;
      history_confirm_clear_ = false;
      showNotice("歷史紀錄",
                 cleared ? "已刪除所有訊息與語音" : "刪除時發生錯誤，部分內容可能仍在",
                 cleared ? kNoticeGreen : kNoticeRed);
      return;
    }
    if (hasWordCharacter(keys, 'n') || isEscape(keys)) {
      history_confirm_clear_ = false;
      dirty_ = true;
    }
    return;
  }
  if (isEscape(keys)) {
    screen_ = Screen::Home;
    dirty_ = true;
    return;
  }
  if (!history_.empty() && hasWordCharacter(keys, 'd')) {
    history_confirm_clear_ = true;
    dirty_ = true;
    return;
  }
  if (history_.empty()) {
    return;
  }
  if (isUp(keys) && history_selected_ > 0U) {
    --history_selected_;
    dirty_ = true;
  }
  if (isDown(keys) && history_selected_ + 1U < history_.size()) {
    ++history_selected_;
    dirty_ = true;
  }
  if (keys.enter && history_selected_ < history_.size()) {
    const std::uint32_t clip_id = history_[history_selected_].clip_id;
    if (clip_id == 0U) {
      return;
    }
    if (!playClip(clip_id)) {
      showNotice("語音", "音檔已不在儲存區或音訊忙碌", kNoticeYellow);
    }
  }
}

void MeshTerminalApp::handleInboxInput() {
  auto& keys = M5Cardputer.Keyboard.keysState();
  if (inbox_.empty()) {
    screen_ = Screen::Home;
    dirty_ = true;
    return;
  }
  if (isEscape(keys)) {
    // 稍後再看：留在未讀，主畫面的紅色提示不會消失。
    screen_ = Screen::Home;
    dirty_ = true;
    return;
  }

  InboxItem& item = inbox_.front();
  const bool is_voice = item.type == MessageType::Voice;
  const bool playable =
      is_voice && (item.clip_id != 0U || !item.frames.empty());

  if (keys.enter) {
    // 語音的第一次 Enter 是播放，不是關掉：使用者按下確認鍵才出聲，而且要能重播。
    if (playable && !item.played) {
      if (playInboxVoice(item)) {
        item.played = true;
      } else {
        showNotice("語音", "音訊忙碌，稍後再按一次", kNoticeYellow);
      }
      dirty_ = true;
      return;
    }
    inbox_.erase(inbox_.begin());
    audio_.playAlert(AlertTone::Confirm);
    if (inbox_.empty()) {
      screen_ = Screen::Home;
    }
    dirty_ = true;
    return;
  }
  if (keys.del && playable && !item.played) {
    // 不想聽也要能清掉，否則未讀會永遠卡在第一則。
    inbox_.erase(inbox_.begin());
    if (inbox_.empty()) {
      screen_ = Screen::Home;
    }
    dirty_ = true;
  }
}

void MeshTerminalApp::handleNoticeInput() {
  screen_ = Screen::Home;
  dirty_ = true;
}

bool MeshTerminalApp::joinGroup() {
  if (!deriveGroupProfile(pin_, group_)) {
    pairing_error_ = "PIN 必須是 4 位數";
    return false;
  }
  if (!crypto_.begin(pin_, group_.group_id)) {
    pairing_error_ = "群組初始化失敗";
    return false;
  }
  // 記住這次成功配對的 PIN，下次開機直接帶入。PIN 本來就明碼顯示在這個畫面上，
  // 拿得到裝置就等於看得到 PIN，多存一份不擴大威脅模型（ADR-010）。
  device_state_.setPin(pin_);
  paired_ = true;
  pairing_error_.clear();
  const bool radio_ok = radio_.begin(group_);
  if (!radio_ok) {
    recordSystem("[系統] LoRa 初始化失敗，僅可離線操作");
  }
  pin_.assign(4U, '\0');
  pin_.clear();
  return true;
}

PacketHeader MeshTerminalApp::makeBaseHeader(const MessageType type) {
  const auto& fix = gnss_.snapshot();
  PacketHeader header{};
  header.type = type;
  header.ttl = initialTtl(type);
  header.group_id = group_.group_id;
  header.source_id = device_state_.nodeId();
  header.message_id = device_state_.nextMessageId();
  header.sequence = device_state_.nextSequence(fix.day_key);
  header.timestamp = fix.unix_time;
  return header;
}

bool MeshTerminalApp::sendPayload(const MessageType type,
                                  const std::vector<std::uint8_t>& payload) {
  // 天線未確認時完全不做封裝與排隊。RadioService 也會再擋一次，這裡先擋是為了
  // 不白做 AES-GCM 與分片，也不消耗 message_id / sequence。
  if (!paired_ || !antenna_confirmed_ || !radio_.ready() || !crypto_.ready()) {
    return false;
  }
  const PacketHeader base = makeBaseHeader(type);
  std::vector<PlainFragment> fragments;
  if (!fragmentMessage(base, payload, fragments)) {
    return false;
  }
  std::vector<std::vector<std::uint8_t>> packets;
  packets.reserve(fragments.size());
  for (auto& fragment : fragments) {
    fragment.header.nonce = nonce_generator_.next();
    std::vector<std::uint8_t> wire;
    if (sealFrame(fragment.header, fragment.payload, crypto_, wire) !=
        FrameError::None) {
      return false;
    }
    packets.push_back(std::move(wire));
  }
  return radio_.queueTransmitBatch(packets);
}

const char* MeshTerminalApp::txBlockReason() const {
  // 順序與 sendPayload() 的前置檢查一致。
  if (!paired_) {
    return "尚未加入群組";
  }
  if (!antenna_confirmed_) {
    return "未確認天線，按 A 確認";
  }
  if (!radio_.ready()) {
    return "LoRa 模組未就緒";
  }
  if (!crypto_.ready()) {
    return "群組金鑰未就緒";
  }
  if (radio_.transmitInhibited()) {
    return "發射閘門仍關閉";
  }
  return nullptr;
}

bool MeshTerminalApp::sendText(const std::string& text) {
  std::vector<std::uint8_t> payload;
  if (!encodeTextMessage(text, payload) ||
      !sendPayload(MessageType::Text, payload)) {
    return false;
  }
  recordText(LogKind::Text, true, device_state_.callsign(), "[我] " + text,
             text, device_state_.nodeId());
  return true;
}

bool MeshTerminalApp::sendBeacon() {
  const auto& fix = gnss_.snapshot();
  BeaconMessage beacon{};
  // 沒有 fix 也照樣廣播：Beacon 帶的是「我在這個群組、我還活著、電量多少」，
  // 座標只是其中一欄。先前沒定位就整個不發，隊友名單因此永遠是空的。
  if (fix.point.valid) {
    beacon.point = fix.point;
  }
  const int battery = M5Cardputer.Power.getBatteryLevel();
  beacon.battery_percent = static_cast<std::uint8_t>(
      battery < 0 ? 0 : std::min(100, battery));
  beacon.callsign = device_state_.callsign();
  std::vector<std::uint8_t> payload;
  return encodeBeaconMessage(beacon, payload) &&
         sendPayload(MessageType::Beacon, payload);
}

void MeshTerminalApp::shareLocation() {
  // 手動觸發的即時 Beacon：跟罐頭訊息長在同一個選單，但走的是 Beacon payload，
  // 不是 Text，這樣接收端會直接更新雷達上的隊友位置，而不是進未讀訊息佇列。
  if (blockedByAntenna()) {
    return;
  }
  const bool has_fix = gnss_.snapshot().point.valid;
  if (sendBeacon()) {
    next_beacon_ms_ = millis() + kBeaconIntervalMs;
    recordSystem(has_fix ? "[我] 已分享目前位置" : "[我] 已分享身分（尚無定位）");
    showNotice("位置", has_fix ? "已分享目前位置，隊友雷達會同步更新"
                               : "尚無定位，僅分享身分與電量",
               has_fix ? kNoticeGreen : kNoticeYellow);
  } else {
    const char* reason = txBlockReason();
    showNotice("位置無法分享", reason != nullptr ? reason : "傳送佇列不可用",
               kNoticeRed);
  }
}

bool MeshTerminalApp::sendVoice(
    const std::vector<std::uint8_t>& encoded) {
  VoiceMessage voice{};
  voice.frames = encoded;
  std::vector<std::uint8_t> payload;
  if (!encodeVoiceMessage(voice, payload) ||
      !sendPayload(MessageType::Voice, payload)) {
    return false;
  }
  // 自己送出的語音也留檔，之後可以在歷史裡重播確認當時說了什麼。
  recordVoice(true, device_state_.callsign(), "[我] 語音", encoded,
              device_state_.nodeId());
  return true;
}

void MeshTerminalApp::handleRadio(const std::uint32_t now_ms) {
  RadioRxPacket radio_packet{};
  std::size_t processed = 0;
  while (processed++ < 6U && radio_.pollReceived(radio_packet)) {
    DecodedFrame decoded{};
    if (openFrame(radio_packet.bytes.data(), radio_packet.bytes.size(),
                  group_.group_id, crypto_, decoded) != FrameError::None) {
      continue;
    }

    const RouteDecision route = router_.onAuthenticatedPacket(
        decoded.header, device_state_.nodeId(), now_ms);
    if (route.duplicate || !route.deliver) {
      continue;
    }
    // 中繼同樣是發射行為，天線未確認時只收不轉。
    if (route.relay && antenna_confirmed_) {
      PacketHeader relay_header = route.relay_header;
      relay_header.nonce = nonce_generator_.next();
      std::vector<std::uint8_t> relay_wire;
      if (sealFrame(relay_header, decoded.payload, crypto_, relay_wire) ==
          FrameError::None) {
        radio_.queueTransmit(relay_wire);
      }
    }

    const ReassemblyResult result =
        reassembler_.accept(decoded.header, decoded.payload, now_ms);
    if (result.state == ReassemblyState::Complete) {
      handleDecodedMessage(result, radio_packet.rssi_dbm, now_ms);
    }
  }
}

void MeshTerminalApp::handleDecodedMessage(const ReassemblyResult& result,
                                           const float rssi_dbm,
                                           const std::uint32_t now_ms) {
  const std::uint32_t day_key =
      result.header.timestamp == 0U ? 0U : result.header.timestamp / 86400UL;
  const SequenceObservation observation = sequence_tracker_.observe(
      result.header.source_id, day_key, result.header.sequence);
  if (observation.state == SequenceState::Gap) {
    const std::uint32_t count =
        std::min<std::uint32_t>(3U, observation.last_missing -
                                       observation.first_missing + 1U);
    for (std::uint32_t offset = 0; offset < count; ++offset) {
      PacketHeader missing_header = result.header;
      missing_header.sequence = observation.first_missing + offset;
      recordSystem("[漏訊 " + sequenceLabel(missing_header) + "]");
    }
  }

  // 任何通過認證的封包都足以證明對方在線。隊友名單因此不再依賴 Beacon，
  // 也不依賴任何一方有沒有 GNSS 定位。
  const bool first_contact =
      std::find_if(peers_.begin(), peers_.end(),
                   [&result](const PeerState& item) {
                     return item.node_id == result.header.source_id;
                   }) == peers_.end();
  PeerState& peer = touchPeer(result.header.source_id, rssi_dbm, now_ms);
  if (first_contact) {
    recordSystem("[系統] 隊友上線 " + peerName(result.header.source_id));
    expediteBeacon(now_ms);
  }
  dirty_ = true;

  if (result.header.type == MessageType::Text) {
    std::string text;
    if (!decodeTextMessage(result.message, text)) {
      return;
    }
    const std::string sender = peerName(result.header.source_id);
    const std::string label = sequenceLabel(result.header);
    recordText(LogKind::Text, false, sender,
               "[" + sender + " " + label + "] " + text, text,
               result.header.source_id);

    InboxItem item{};
    item.type = MessageType::Text;
    item.source_id = result.header.source_id;
    item.sender = sender;
    item.label = label;
    item.text = text;
    enqueueInbox(std::move(item));
    audio_.playAlert(AlertTone::Text);
  } else if (result.header.type == MessageType::Beacon) {
    BeaconMessage beacon{};
    if (!decodeBeaconMessage(result.message, beacon)) {
      return;
    }
    peer.callsign = std::move(beacon.callsign);
    peer.position = beacon.point;
    peer.battery_percent = beacon.battery_percent;
    peer.had_beacon = true;
  } else if (result.header.type == MessageType::Voice) {
    VoiceMessage voice{};
    if (!decodeVoiceMessage(result.message, voice)) {
      recordSystem("[語音] 格式錯誤，已捨棄");
      return;
    }
    // 收到就播會讓使用者錯過整段語音（人不一定在看機器）。改成先存檔、發提示音，
    // 等使用者按確認鍵才播，而且之後隨時能從歷史重播。
    const std::string sender = peerName(result.header.source_id);
    const std::string label = sequenceLabel(result.header);
    const std::uint32_t clip_id =
        recordVoice(false, sender, "[" + sender + " " + label + "] 語音",
                    voice.frames, result.header.source_id);

    InboxItem item{};
    item.type = MessageType::Voice;
    item.source_id = result.header.source_id;
    item.sender = sender;
    item.label = label;
    item.clip_id = clip_id;
    if (clip_id == 0U) {
      // 存不下就把音訊留在這一則裡：至少聽得到，只是關掉之後無法重播。
      item.frames = std::move(voice.frames);
    }
    enqueueInbox(std::move(item));
    audio_.playAlert(AlertTone::Voice);
  }
}

void MeshTerminalApp::updateBeacon(const std::uint32_t now_ms) {
  if (antenna_confirmed_ && next_beacon_ms_ != 0U &&
      static_cast<std::int32_t>(now_ms - next_beacon_ms_) >= 0) {
    if (sendBeacon()) {
      next_beacon_ms_ = now_ms + kBeaconIntervalMs;
    } else {
      next_beacon_ms_ = now_ms + 30000UL;
    }
  }
  reassembler_.expire(now_ms);
}

MeshTerminalApp::PeerState& MeshTerminalApp::touchPeer(
    const std::uint32_t node_id, const float rssi_dbm,
    const std::uint32_t now_ms) {
  auto peer = std::find_if(
      peers_.begin(), peers_.end(),
      [node_id](const PeerState& item) { return item.node_id == node_id; });
  if (peer == peers_.end()) {
    if (peers_.size() >= kMaxPeers) {
      // 淘汰最久沒出現的那一位，而不是最早加入的：長時間同行的隊友不該被
      // 一個路過的中繼節點擠掉。
      peer = std::min_element(peers_.begin(), peers_.end(),
                              [](const PeerState& a, const PeerState& b) {
                                return a.last_seen_ms < b.last_seen_ms;
                              });
      *peer = PeerState{};
    } else {
      peers_.push_back(PeerState{});
      peer = peers_.end() - 1;
    }
    peer->node_id = node_id;
  }
  peer->last_seen_ms = now_ms;
  peer->rssi_dbm = rssi_dbm;
  return *peer;
}

std::string MeshTerminalApp::peerName(const std::uint32_t node_id) const {
  const auto peer = std::find_if(
      peers_.begin(), peers_.end(),
      [node_id](const PeerState& item) { return item.node_id == node_id; });
  if (peer != peers_.end() && !peer->callsign.empty()) {
    return peer->callsign;
  }
  return nodeLabel(node_id);
}

void MeshTerminalApp::expediteBeacon(const std::uint32_t now_ms) {
  if (!antenna_confirmed_) {
    return;
  }
  const std::uint32_t soon =
      now_ms + kBeaconExpediteMinMs +
      (device_state_.nodeId() % kBeaconExpediteJitterMs);
  if (next_beacon_ms_ == 0U ||
      static_cast<std::int32_t>(soon - next_beacon_ms_) < 0) {
    next_beacon_ms_ = soon;
  }
}

void MeshTerminalApp::addHistory(const std::string& entry,
                                 const std::uint32_t clip_id) {
  if (history_.size() >= kMaxHistory) {
    history_.erase(history_.begin());
  }
  history_.push_back(HistoryEntry{entry, clip_id});
  history_selected_ = history_.size() - 1U;
}

void MeshTerminalApp::recordSystem(const std::string& entry) {
  addHistory(entry);
  store_.appendText(LogKind::System, false, std::string(), entry, 0U,
                    gnss_.snapshot().unix_time);
}

void MeshTerminalApp::recordText(const LogKind kind, const bool outgoing,
                                 const std::string& sender,
                                 const std::string& display_text,
                                 const std::string& body,
                                 const std::uint32_t source_id) {
  addHistory(display_text);
  store_.appendText(kind, outgoing, sender, body, source_id,
                    gnss_.snapshot().unix_time);
}

std::uint32_t MeshTerminalApp::recordVoice(
    const bool outgoing, const std::string& sender,
    const std::string& display_text, const std::vector<std::uint8_t>& frames,
    const std::uint32_t source_id) {
  std::uint32_t clip_id = 0;
  store_.appendVoice(outgoing, sender, display_text, frames, source_id,
                     gnss_.snapshot().unix_time, clip_id);
  addHistory(display_text, clip_id);
  return clip_id;
}

void MeshTerminalApp::loadHistoryFromStore() {
  const std::vector<LogRecord> records = store_.loadRecent(kMaxHistory);
  history_.clear();
  history_.reserve(records.size());
  for (const LogRecord& record : records) {
    HistoryEntry entry{};
    // 音檔可能已被空間回收刪掉，紀錄還在但不再可重播。
    entry.clip_id = store_.hasClip(record.clip_id) ? record.clip_id : 0U;
    const std::string who =
        record.outgoing ? std::string("我")
                        : (record.sender.empty() ? nodeLabel(record.source_id)
                                                 : record.sender);
    if (record.kind == LogKind::System) {
      entry.text = record.text;
    } else if (record.kind == LogKind::Voice) {
      entry.text = "[" + who + "] 語音";
    } else {
      entry.text = "[" + who + "] " + record.text;
    }
    history_.push_back(std::move(entry));
  }
  history_selected_ = history_.empty() ? 0U : history_.size() - 1U;
}

void MeshTerminalApp::enqueueInbox(InboxItem item) {
  if (inbox_.size() >= kMaxInbox) {
    // 未讀滿了就丟最舊的一則。紀錄仍在歷史裡，不會真的消失。
    inbox_.erase(inbox_.begin());
  }
  inbox_.push_back(std::move(item));
  // 使用者正停在主畫面時直接把訊息推到眼前；正在打字或操作選單時不打斷，
  // 靠主畫面的未讀提示與提示音即可。
  if (screen_ == Screen::Home) {
    screen_ = Screen::Inbox;
  }
  dirty_ = true;
}

bool MeshTerminalApp::playClip(const std::uint32_t clip_id) {
  std::vector<std::uint8_t> frames;
  if (!store_.loadClip(clip_id, frames)) {
    return false;
  }
  return audio_.playEncoded(frames);
}

bool MeshTerminalApp::playInboxVoice(const InboxItem& item) {
  if (item.clip_id != 0U) {
    return playClip(item.clip_id);
  }
  return !item.frames.empty() && audio_.playEncoded(item.frames);
}

void MeshTerminalApp::showNotice(const std::string& title,
                                 const std::string& message,
                                 const std::uint16_t color) {
  notice_title_ = title;
  notice_message_ = message;
  notice_color_ = color;
  screen_ = Screen::Notice;
  dirty_ = true;
}

UiHomeModel MeshTerminalApp::buildHomeModel(
    const std::uint32_t now_ms) const {
  UiHomeModel model{};
  model.callsign = device_state_.callsign();
  model.group_id = group_.group_id;
  model.frequency_mhz = group_.frequency_mhz;
  model.battery_percent = M5Cardputer.Power.getBatteryLevel();
  model.radio_ready = radio_.ready();
  model.sd_ready = storage_.ready();
  model.voice_ready = audio_.available();
  model.volume_percent = audio_.volumePercent();
  const GnssSnapshot& fix = gnss_.snapshot();
  model.satellites = fix.satellites;
  model.satellites_in_view = fix.satellites_in_view;
  model.gnss_link = fix.link;
  model.own_position = fix.point;
  model.tx_queued = radio_.queuedCount();
  model.tx_inhibited = radio_.transmitInhibited();
  model.unread_count = inbox_.size();
  model.selected_peer = peer_selected_;
  model.peers.reserve(peers_.size());
  for (const auto& peer : peers_) {
    UiPeer view{};
    view.callsign =
        peer.callsign.empty() ? nodeLabel(peer.node_id) : peer.callsign;
    view.relative = relativePosition(model.own_position, peer.position);
    view.battery_percent = peer.battery_percent;
    view.age_seconds = (now_ms - peer.last_seen_ms) / 1000UL;
    view.rssi_dbm = peer.rssi_dbm;
    view.has_position = peer.position.valid;
    model.peers.push_back(std::move(view));
  }
  return model;
}

std::vector<UiHistoryEntry> MeshTerminalApp::buildHistoryModel() const {
  std::vector<UiHistoryEntry> model;
  model.reserve(history_.size());
  for (const HistoryEntry& entry : history_) {
    model.push_back(UiHistoryEntry{entry.text, entry.clip_id != 0U});
  }
  return model;
}

UiInboxItem MeshTerminalApp::buildInboxModel() const {
  UiInboxItem model{};
  if (inbox_.empty()) {
    return model;
  }
  const InboxItem& item = inbox_.front();
  model.sender = item.sender;
  model.label = item.label;
  model.text = item.text;
  model.is_voice = item.type == MessageType::Voice;
  model.played = item.played;
  // 未讀期間音檔可能已被空間回收淘汰，要真的問儲存區，不能只看 id 有沒有值，
  // 否則畫面會顯示「按 Enter 播放」卻永遠播不出來。
  model.clip_available =
      !item.frames.empty() || store_.hasClip(item.clip_id);
  model.index = 0;
  model.total = inbox_.size();
  return model;
}

void MeshTerminalApp::render(const std::uint32_t now_ms, const bool force) {
  // 週期重畫只負責更新會自己變動的欄位（電量、隊友時效、佇列、錄音進度）。
  // 其餘更新一律由 dirty_ 觸發，並限制在 kMinRenderIntervalMs 以上，避免主迴圈
  // 每 2 ms 就重畫一次整頁。畫面本身由 TerminalUi 的離螢幕緩衝推送，不會閃爍。
  constexpr std::uint32_t kMinRenderIntervalMs = 50;
  const std::uint32_t periodic_interval =
      screen_ == Screen::Recording ? 100UL : 1000UL;
  const std::uint32_t elapsed = now_ms - last_render_ms_;
  if (!force && elapsed < kMinRenderIntervalMs) {
    return;
  }
  if (!force && !dirty_ && elapsed < periodic_interval) {
    return;
  }
  last_render_ms_ = now_ms;
  dirty_ = false;

  switch (screen_) {
    case Screen::UserId:
      ui_.renderUserId(user_id_, user_id_error_.c_str());
      break;
    case Screen::Pairing:
      ui_.renderPairing(pin_, pairing_error_.c_str());
      break;
    case Screen::AntennaCheck:
      ui_.renderAntennaCheck(radio_.moduleDetected());
      break;
    case Screen::Home:
      ui_.renderHome(buildHomeModel(now_ms));
      break;
    case Screen::MessageMenu:
      ui_.renderMessageMenu(storage_.cannedMessages(), menu_selected_);
      break;
    case Screen::TextInput:
      ui_.renderTextInput(text_input_);
      break;
    case Screen::History:
      ui_.renderHistory(buildHistoryModel(), history_selected_,
                        history_confirm_clear_);
      break;
    case Screen::Recording:
      ui_.renderRecording(audio_.recordingProgress(now_ms));
      break;
    case Screen::Inbox:
      ui_.renderInbox(buildInboxModel());
      break;
    case Screen::Notice:
      ui_.renderNotice(notice_title_, notice_message_, notice_color_);
      break;
  }
}

}  // namespace cmt

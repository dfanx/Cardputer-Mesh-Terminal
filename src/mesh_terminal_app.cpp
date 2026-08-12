#include "cmt/app/mesh_terminal_app.h"

#include <M5Cardputer.h>

#include <algorithm>
#include <cstdio>

namespace cmt {
namespace {

constexpr std::uint32_t kBeaconIntervalMs = 10UL * 60UL * 1000UL;
constexpr std::size_t kMaxPeers = 12;
constexpr std::size_t kMaxHistory = 32;
constexpr std::uint16_t kNoticeGreen = 0x07E0;
constexpr std::uint16_t kNoticeYellow = 0xFFE0;
constexpr std::uint16_t kNoticeRed = 0xF800;

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
  gnss_.begin();
  audio_.begin();
  // 音訊之後才要畫面緩衝：記憶體不夠時寧可畫面閃爍，也不要犧牲 Codec2 語音。
  ui_.enableFrameBuffer();

  delay(350);
  // 第一步先問「這台機器是誰」，代號會進 Beacon 的 callsign，隊友才分得出來源。
  // 已設定過的代號直接帶入，回訪時按 Enter 就能過。
  user_id_ = device_state_.userId();
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
    updateTrackAndBeacon(now_ms);
  }
  handleInput(now_ms);
  render(now_ms);
  delay(2);
}

void MeshTerminalApp::handleInput(const std::uint32_t now_ms) {
  const bool space_held = M5Cardputer.Keyboard.isKeyPressed(' ');
  if (screen_ == Screen::Recording) {
    if (audio_.updateRecording(space_held, now_ms)) {
      std::vector<std::uint8_t> encoded;
      if (audio_.takeEncoded(encoded) && sendVoice(encoded)) {
        showNotice("語音", "語音已排入傳送", kNoticeGreen);
      } else {
        // 一幀都沒錄到，實務上幾乎都是按一下就放開：startRecording() 的提示音
        // 會先擋掉約 200 ms，太短的按壓結束時連 40 ms 的第一幀都湊不滿。
        showNotice("語音", "錄音過短，請按住 Space 說完再放開", kNoticeYellow);
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
    // 配對前都還沒發射，退回去改代號是安全的。
    pin_.clear();
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
    addHistory("[系統] 已確認天線，發射啟用");
  } else {
    next_beacon_ms_ = 0U;
    addHistory("[系統] 未確認天線，僅接收不發射");
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
    dirty_ = true;
    return;
  }
  if (isVolumeDown(keys)) {
    audio_.adjustVolume(-10);
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
  const std::size_t total = messages.size() + 1U;
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
    } else {
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
  if (isEscape(keys)) {
    screen_ = Screen::Home;
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
  paired_ = true;
  pairing_error_.clear();
  const bool radio_ok = radio_.begin(group_);
  if (!radio_ok) {
    addHistory("[系統] LoRa 初始化失敗，僅可離線操作");
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

bool MeshTerminalApp::sendText(const std::string& text) {
  std::vector<std::uint8_t> payload;
  if (!encodeTextMessage(text, payload) ||
      !sendPayload(MessageType::Text, payload)) {
    return false;
  }
  addHistory("[我] " + text);
  return true;
}

bool MeshTerminalApp::sendBeacon() {
  const auto& fix = gnss_.snapshot();
  if (!fix.point.valid) {
    return false;
  }
  BeaconMessage beacon{};
  beacon.point = fix.point;
  const int battery = M5Cardputer.Power.getBatteryLevel();
  beacon.battery_percent = static_cast<std::uint8_t>(
      battery < 0 ? 0 : std::min(100, battery));
  beacon.callsign = device_state_.callsign();
  std::vector<std::uint8_t> payload;
  return encodeBeaconMessage(beacon, payload) &&
         sendPayload(MessageType::Beacon, payload);
}

bool MeshTerminalApp::sendVoice(
    const std::vector<std::uint8_t>& encoded) {
  VoiceMessage voice{};
  voice.frames = encoded;
  std::vector<std::uint8_t> payload;
  return encodeVoiceMessage(voice, payload) &&
         sendPayload(MessageType::Voice, payload);
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
      addHistory("[漏訊 " + sequenceLabel(missing_header) + "]");
    }
  }

  if (result.header.type == MessageType::Text) {
    std::string text;
    if (decodeTextMessage(result.message, text)) {
      char source[12]{};
      std::snprintf(source, sizeof(source), "N%04lX",
                    static_cast<unsigned long>(result.header.source_id &
                                               0xFFFFUL));
      addHistory(std::string("[") + source + " " +
                 sequenceLabel(result.header) + "] " + text);
      dirty_ = true;
    }
  } else if (result.header.type == MessageType::Beacon) {
    BeaconMessage beacon{};
    if (!decodeBeaconMessage(result.message, beacon)) {
      return;
    }
    auto peer = std::find_if(peers_.begin(), peers_.end(),
                             [&result](const PeerState& item) {
                               return item.node_id == result.header.source_id;
                             });
    if (peer == peers_.end()) {
      if (peers_.size() >= kMaxPeers) {
        peers_.erase(peers_.begin());
      }
      peers_.push_back(PeerState{});
      peer = peers_.end() - 1;
      peer->node_id = result.header.source_id;
    }
    peer->beacon = std::move(beacon);
    peer->last_seen_ms = now_ms;
    peer->rssi_dbm = rssi_dbm;
    dirty_ = true;
  } else if (result.header.type == MessageType::Voice) {
    VoiceMessage voice{};
    if (!decodeVoiceMessage(result.message, voice)) {
      addHistory("[語音] 格式錯誤，已捨棄");
    } else if (!audio_.playEncoded(voice.frames)) {
      addHistory("[語音] 音訊忙碌或解碼器不可用");
    } else {
      addHistory("[語音] " + sequenceLabel(result.header));
    }
    dirty_ = true;
  }
}

void MeshTerminalApp::updateTrackAndBeacon(const std::uint32_t now_ms) {
  const auto& fix = gnss_.snapshot();
  if (storage_.maybeAppendTrack(fix.point, fix.day_key, fix.unix_time, now_ms)) {
    dirty_ = true;
  }
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

void MeshTerminalApp::addHistory(const std::string& entry) {
  if (history_.size() >= kMaxHistory) {
    history_.erase(history_.begin());
  }
  history_.push_back(entry);
  history_selected_ = history_.size() - 1U;
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
  model.satellites = gnss_.snapshot().satellites;
  model.own_position = gnss_.snapshot().point;
  model.track = storage_.track();
  model.tx_queued = radio_.queuedCount();
  model.tx_inhibited = radio_.transmitInhibited();
  model.selected_peer = peer_selected_;
  model.peers.reserve(peers_.size());
  for (const auto& peer : peers_) {
    UiPeer view{};
    view.callsign = peer.beacon.callsign;
    view.relative =
        relativePosition(model.own_position, peer.beacon.point);
    view.battery_percent = peer.beacon.battery_percent;
    view.age_seconds = (now_ms - peer.last_seen_ms) / 1000UL;
    view.rssi_dbm = peer.rssi_dbm;
    model.peers.push_back(std::move(view));
  }
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
      ui_.renderHistory(history_, history_selected_);
      break;
    case Screen::Recording:
      ui_.renderRecording(audio_.recordingProgress(now_ms));
      break;
    case Screen::Notice:
      ui_.renderNotice(notice_title_, notice_message_, notice_color_);
      break;
  }
}

}  // namespace cmt

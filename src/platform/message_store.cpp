#include "cmt/platform/message_store.h"

#include <Arduino.h>
#include <LittleFS.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace cmt {
namespace {

constexpr const char* kPartitionLabel = "logfs";
constexpr const char* kBasePath = "/logfs";
constexpr const char* kLogPath = "/msg.log";
constexpr const char* kLogTempPath = "/msg.tmp";
constexpr const char* kClipDir = "/v";

// 文字紀錄的自有上限。語音淘汰只看 kMinFreeBytes，永遠不會碰到這個檔案；文字要到
// 這裡才輪替，以每筆約 80 bytes 估算相當於三千多筆，遠超過一趟行程的量。
constexpr std::size_t kMaxLogBytes = 384UL * 1024UL;
// 低於這個可用空間就開始淘汰最舊的語音。留白是給 LittleFS 的區塊配置與 log 成長用。
constexpr std::size_t kMinFreeBytes = 128UL * 1024UL;
// 讀取歷史時只回捲這麼多位元組，避免整個 log 進 RAM。
constexpr std::size_t kTailWindowBytes = 12UL * 1024UL;
constexpr std::size_t kMaxFieldBytes = 200;

// 欄位分隔符與換行不得出現在內容裡，否則一行會被拆成兩筆。
std::string sanitize(const std::string& value) {
  std::string out;
  out.reserve(std::min(value.size(), kMaxFieldBytes));
  for (const char item : value) {
    if (out.size() >= kMaxFieldBytes) {
      break;
    }
    out.push_back((item == '|' || item == '\n' || item == '\r') ? ' ' : item);
  }
  return out;
}

void clipPath(const std::uint32_t clip_id, char* buffer,
              const std::size_t size) {
  std::snprintf(buffer, size, "%s/%08lu.c2", kClipDir,
                static_cast<unsigned long>(clip_id));
}

std::uint32_t parseU32(const std::string& value) {
  return static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
}

// `File::name()` 在不同 Arduino core 版本間有時回傳完整路徑、有時只有檔名，
// 兩種都要能解出編號。
std::uint32_t clipIdFromName(const char* name) {
  if (name == nullptr) {
    return 0U;
  }
  const char* base = name;
  for (const char* cursor = name; *cursor != '\0'; ++cursor) {
    if (*cursor == '/') {
      base = cursor + 1;
    }
  }
  return static_cast<std::uint32_t>(std::strtoul(base, nullptr, 10));
}

// `id|unix|kind|source|dir|clip|sender|text`
bool parseRecord(const std::string& line, LogRecord& record) {
  std::string fields[8];
  std::size_t field = 0;
  for (const char item : line) {
    if (item == '|' && field + 1U < 8U) {
      ++field;
      continue;
    }
    fields[field].push_back(item);
  }
  if (field != 7U || fields[0].empty()) {
    return false;
  }
  const std::uint32_t kind = parseU32(fields[2]);
  if (kind > static_cast<std::uint32_t>(LogKind::Voice)) {
    return false;
  }
  record.record_id = parseU32(fields[0]);
  record.unix_time = parseU32(fields[1]);
  record.kind = static_cast<LogKind>(kind);
  record.source_id = static_cast<std::uint32_t>(
      std::strtoul(fields[3].c_str(), nullptr, 16));
  record.outgoing = fields[4] == "1";
  record.clip_id = parseU32(fields[5]);
  record.sender = fields[6];
  record.text = fields[7];
  return record.record_id != 0U;
}

}  // namespace

bool MessageStore::begin() {
  ready_ = LittleFS.begin(true, kBasePath, 10, kPartitionLabel);
  if (!ready_) {
    return false;
  }
  if (!LittleFS.exists(kClipDir)) {
    LittleFS.mkdir(kClipDir);
  }
  if (LittleFS.exists(kLogTempPath)) {
    LittleFS.remove(kLogTempPath);
  }
  scanExistingState();
  rotateLogIfOversized();
  return ready_;
}

bool MessageStore::ready() const { return ready_; }

void MessageStore::scanExistingState() {
  // record id 只看最後一筆就夠：log 是純附加的，id 單調遞增。
  const std::vector<LogRecord> tail = loadRecent(1U);
  if (!tail.empty()) {
    next_record_id_ = tail.back().record_id + 1U;
  }

  File dir = LittleFS.open(kClipDir);
  if (!dir || !dir.isDirectory()) {
    return;
  }
  clip_count_ = 0;
  std::uint32_t highest = 0;
  for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
    if (entry.isDirectory()) {
      continue;
    }
    ++clip_count_;
    highest = std::max(highest, clipIdFromName(entry.name()));
  }
  dir.close();
  next_clip_id_ = highest + 1U;
}

std::uint32_t MessageStore::totalBytes() const {
  return ready_ ? static_cast<std::uint32_t>(LittleFS.totalBytes()) : 0U;
}

std::uint32_t MessageStore::usedBytes() const {
  return ready_ ? static_cast<std::uint32_t>(LittleFS.usedBytes()) : 0U;
}

std::uint32_t MessageStore::freeBytes() const {
  if (!ready_) {
    return 0U;
  }
  const std::size_t total = LittleFS.totalBytes();
  const std::size_t used = LittleFS.usedBytes();
  return static_cast<std::uint32_t>(total > used ? total - used : 0U);
}

std::size_t MessageStore::clipCount() const { return clip_count_; }

std::uint32_t MessageStore::prunedClips() const { return pruned_clips_; }

bool MessageStore::deleteOldestClip() {
  File dir = LittleFS.open(kClipDir);
  if (!dir || !dir.isDirectory()) {
    return false;
  }
  std::uint32_t oldest = 0;
  bool found = false;
  for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
    if (entry.isDirectory()) {
      continue;
    }
    const std::uint32_t id = clipIdFromName(entry.name());
    if (id == 0U) {
      continue;
    }
    if (!found || id < oldest) {
      oldest = id;
      found = true;
    }
  }
  dir.close();
  if (!found) {
    return false;
  }
  char path[32]{};
  clipPath(oldest, path, sizeof(path));
  if (!LittleFS.remove(path)) {
    return false;
  }
  if (clip_count_ > 0U) {
    --clip_count_;
  }
  ++pruned_clips_;
  return true;
}

bool MessageStore::ensureFreeSpace(const std::size_t needed_bytes) {
  if (!ready_) {
    return false;
  }
  // 最多刪 64 次就放棄：真的清不出空間時寧可讓這一段語音存不下，也不要在主迴圈裡
  // 無上限地掃目錄。文字紀錄不在淘汰範圍內。
  for (int attempt = 0; attempt < 64; ++attempt) {
    if (freeBytes() >= kMinFreeBytes + needed_bytes) {
      return true;
    }
    if (!deleteOldestClip()) {
      return false;
    }
  }
  return freeBytes() >= needed_bytes;
}

void MessageStore::rotateLogIfOversized() {
  if (!ready_ || !LittleFS.exists(kLogPath)) {
    return;
  }
  File source = LittleFS.open(kLogPath, FILE_READ);
  if (!source) {
    return;
  }
  const std::size_t size = source.size();
  if (size <= kMaxLogBytes) {
    source.close();
    return;
  }
  // 只在文字自己的上限被打到時才輪替，保留較新的一半。
  source.seek(size / 2U);
  source.readStringUntil('\n');
  File target = LittleFS.open(kLogTempPath, FILE_WRITE);
  if (!target) {
    source.close();
    return;
  }
  std::uint8_t buffer[512];
  while (source.available() > 0) {
    const std::size_t read = source.read(buffer, sizeof(buffer));
    if (read == 0U) {
      break;
    }
    target.write(buffer, read);
  }
  target.close();
  source.close();
  LittleFS.remove(kLogPath);
  LittleFS.rename(kLogTempPath, kLogPath);
}

std::uint32_t MessageStore::appendText(const LogKind kind, const bool outgoing,
                                       const std::string& sender,
                                       const std::string& text,
                                       const std::uint32_t source_id,
                                       const std::uint32_t unix_time) {
  if (!ready_) {
    return 0U;
  }
  const std::uint32_t record_id = next_record_id_;
  File file = LittleFS.open(kLogPath, FILE_APPEND);
  if (!file) {
    return 0U;
  }
  const int written = file.printf(
      "%lu|%lu|%u|%08lX|%u|0|%s|%s\n", static_cast<unsigned long>(record_id),
      static_cast<unsigned long>(unix_time), static_cast<unsigned>(kind),
      static_cast<unsigned long>(source_id), outgoing ? 1U : 0U,
      sanitize(sender).c_str(), sanitize(text).c_str());
  file.close();
  if (written <= 0) {
    return 0U;
  }
  ++next_record_id_;
  rotateLogIfOversized();
  return record_id;
}

std::uint32_t MessageStore::appendVoice(
    const bool outgoing, const std::string& sender, const std::string& label,
    const std::vector<std::uint8_t>& frames, const std::uint32_t source_id,
    const std::uint32_t unix_time, std::uint32_t& clip_id) {
  clip_id = 0U;
  if (!ready_ || frames.empty()) {
    return 0U;
  }
  // 音檔先落地再寫紀錄：反過來的話中途失敗會留下指向不存在音檔的歷史。
  if (ensureFreeSpace(frames.size() + 512U)) {
    char path[32]{};
    clipPath(next_clip_id_, path, sizeof(path));
    File clip = LittleFS.open(path, FILE_WRITE);
    if (clip) {
      const std::size_t written = clip.write(frames.data(), frames.size());
      clip.close();
      if (written == frames.size()) {
        clip_id = next_clip_id_;
        ++next_clip_id_;
        ++clip_count_;
      } else {
        LittleFS.remove(path);
      }
    }
  }

  const std::uint32_t record_id = next_record_id_;
  File file = LittleFS.open(kLogPath, FILE_APPEND);
  if (!file) {
    return 0U;
  }
  const int written = file.printf(
      "%lu|%lu|%u|%08lX|%u|%lu|%s|%s\n", static_cast<unsigned long>(record_id),
      static_cast<unsigned long>(unix_time),
      static_cast<unsigned>(LogKind::Voice),
      static_cast<unsigned long>(source_id), outgoing ? 1U : 0U,
      static_cast<unsigned long>(clip_id), sanitize(sender).c_str(),
      sanitize(label).c_str());
  file.close();
  if (written <= 0) {
    return 0U;
  }
  ++next_record_id_;
  rotateLogIfOversized();
  return record_id;
}

bool MessageStore::clearAll() {
  if (!ready_) {
    return false;
  }
  bool all_removed = true;
  if (LittleFS.exists(kLogPath)) {
    all_removed = LittleFS.remove(kLogPath) && all_removed;
  }
  File dir = LittleFS.open(kClipDir);
  if (dir && dir.isDirectory()) {
    // 用編號重建路徑再刪，跟 deleteOldestClip() 同一招：不同 Arduino core 版本
    // 對 File::name() 回傳完整路徑還是只有檔名不一致，直接信任它組路徑會出錯。
    std::vector<std::uint32_t> ids;
    for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
      if (entry.isDirectory()) {
        continue;
      }
      const std::uint32_t id = clipIdFromName(entry.name());
      if (id != 0U) {
        ids.push_back(id);
      }
    }
    dir.close();
    for (const std::uint32_t id : ids) {
      char path[32]{};
      clipPath(id, path, sizeof(path));
      all_removed = LittleFS.remove(path) && all_removed;
    }
  }
  next_record_id_ = 1U;
  next_clip_id_ = 1U;
  clip_count_ = 0U;
  return all_removed;
}

bool MessageStore::hasClip(const std::uint32_t clip_id) const {
  if (!ready_ || clip_id == 0U) {
    return false;
  }
  char path[32]{};
  clipPath(clip_id, path, sizeof(path));
  return LittleFS.exists(path);
}

bool MessageStore::loadClip(const std::uint32_t clip_id,
                            std::vector<std::uint8_t>& frames) const {
  if (!ready_ || clip_id == 0U) {
    return false;
  }
  char path[32]{};
  clipPath(clip_id, path, sizeof(path));
  File file = LittleFS.open(path, FILE_READ);
  if (!file) {
    return false;
  }
  const std::size_t size = file.size();
  if (size == 0U || size > 4096U) {
    file.close();
    return false;
  }
  frames.assign(size, 0U);
  const std::size_t read = file.read(frames.data(), size);
  file.close();
  if (read != size) {
    frames.clear();
    return false;
  }
  return true;
}

std::vector<LogRecord> MessageStore::loadRecent(
    const std::size_t max_count) const {
  std::vector<LogRecord> records;
  if (!ready_ || max_count == 0U || !LittleFS.exists(kLogPath)) {
    return records;
  }
  File file = LittleFS.open(kLogPath, FILE_READ);
  if (!file) {
    return records;
  }
  const std::size_t size = file.size();
  const std::size_t offset = size > kTailWindowBytes ? size - kTailWindowBytes
                                                     : 0U;
  file.seek(offset);
  if (offset != 0U) {
    // 回捲的起點多半落在某一行中間，那半行丟掉。
    file.readStringUntil('\n');
  }

  std::vector<LogRecord> window;
  while (file.available() > 0) {
    const String line = file.readStringUntil('\n');
    if (line.length() == 0U) {
      continue;
    }
    LogRecord record{};
    if (parseRecord(std::string(line.c_str(), line.length()), record)) {
      window.push_back(std::move(record));
    }
  }
  file.close();

  const std::size_t keep = std::min(max_count, window.size());
  records.assign(window.end() - static_cast<std::ptrdiff_t>(keep),
                 window.end());
  return records;
}

}  // namespace cmt

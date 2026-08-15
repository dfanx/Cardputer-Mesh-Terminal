#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cmt {

enum class LogKind : std::uint8_t {
  System = 0,
  Text = 1,
  Voice = 2,
};

struct LogRecord {
  LogKind kind = LogKind::System;
  std::uint32_t record_id = 0;
  std::uint32_t unix_time = 0;
  std::uint32_t source_id = 0;
  // 語音片段編號，0 表示沒有可重播的音檔（文字、系統訊息，或音檔已被清掉）。
  std::uint32_t clip_id = 0;
  bool outgoing = false;
  std::string sender;
  std::string text;
};

// 訊息與語音歷史的持久層，放在獨立的 `logfs` flash 分割區，跟韌體與 NVS 完全分開，
// 也不動 SD 上的軌跡檔。空間吃緊時只淘汰最舊的語音音檔，文字紀錄永遠保留 —— 這是
// 登山聯繫的取捨：講過的話要查得到，聽過的錄音可以再問一次。
class MessageStore {
 public:
  bool begin();
  bool ready() const;

  // 回傳 record id，0 表示寫入失敗。
  std::uint32_t appendText(LogKind kind, bool outgoing,
                           const std::string& sender, const std::string& text,
                           std::uint32_t source_id, std::uint32_t unix_time);
  // 寫入語音音檔並記一筆歷史。`clip_id` 只有回傳非 0 時才有效。
  std::uint32_t appendVoice(bool outgoing, const std::string& sender,
                            const std::string& label,
                            const std::vector<std::uint8_t>& frames,
                            std::uint32_t source_id, std::uint32_t unix_time,
                            std::uint32_t& clip_id);

  bool loadClip(std::uint32_t clip_id, std::vector<std::uint8_t>& frames) const;
  bool hasClip(std::uint32_t clip_id) const;

  // 由新到舊回傳最多 max_count 筆，輸出仍以「舊在前」排列，方便直接餵給 UI。
  std::vector<LogRecord> loadRecent(std::size_t max_count) const;

  std::uint32_t totalBytes() const;
  std::uint32_t usedBytes() const;
  std::uint32_t freeBytes() const;
  std::size_t clipCount() const;
  // 開機以來為了騰出空間而刪掉的語音數量。
  std::uint32_t prunedClips() const;

 private:
  bool ensureFreeSpace(std::size_t needed_bytes);
  bool deleteOldestClip();
  void rotateLogIfOversized();
  void scanExistingState();

  bool ready_ = false;
  std::uint32_t next_record_id_ = 1;
  std::uint32_t next_clip_id_ = 1;
  std::size_t clip_count_ = 0;
  std::uint32_t pruned_clips_ = 0;
};

}  // namespace cmt

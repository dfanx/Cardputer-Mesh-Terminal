#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace cmt {

class AudioService {
 public:
  AudioService();
  ~AudioService();

  AudioService(const AudioService&) = delete;
  AudioService& operator=(const AudioService&) = delete;

  bool begin();
  bool available() const;
  bool startRecording(std::uint32_t now_ms);
  bool updateRecording(bool ptt_held, std::uint32_t now_ms);
  bool takeEncoded(std::vector<std::uint8_t>& encoded);
  bool playEncoded(const std::vector<std::uint8_t>& encoded);
  std::uint8_t volumePercent() const;
  std::uint8_t adjustVolume(int delta_percent);
  void update();
  bool recording() const;
  float recordingProgress(std::uint32_t now_ms) const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace cmt

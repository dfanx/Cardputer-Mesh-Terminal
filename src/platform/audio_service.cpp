#include "cmt/platform/audio_service.h"

#include "cmt/core/message_codec.h"

#include <M5Cardputer.h>

#include <algorithm>
#include <utility>

#if CMT_ENABLE_VOICE && __has_include(<codec2.h>)
#include <codec2.h>
#define CMT_CODEC2_AVAILABLE 1
#else
#define CMT_CODEC2_AVAILABLE 0
#endif

namespace cmt {
namespace {

constexpr std::uint32_t kSampleRate = kVoiceSampleRateHz;
constexpr std::uint32_t kMaxRecordingMs = kMaxVoiceDurationMs;
constexpr std::size_t kMaxSamples =
    static_cast<std::size_t>(kMaxVoiceFrames) * kVoiceSamplesPerFrame;
constexpr std::uint8_t kDefaultVolumePercent = 50;
// 輸出停止後多久關掉功放。留一小段是為了讓「提示音 → 語音」這種連續輸出不必反覆
// 開關 I2S；再長就等於一直供電，正是電流聲的來源。
constexpr std::uint32_t kSpeakerIdleOffMs = 300;
// I2S 剛啟動時 isPlaying() 可能還沒反映排入的資料，太早判定閒置會把聲音切掉。
constexpr std::uint32_t kSpeakerMinOnMs = 120;

}  // namespace

class AudioService::Impl {
 public:
  ~Impl() {
#if CMT_CODEC2_AVAILABLE
    if (codec_ != nullptr) {
      codec2_destroy(codec_);
    }
#endif
  }

  bool begin() {
    // M5.begin() 的 internal_spk/internal_mic 會讓功放與麥克風在開機後一直掛著，
    // 靜止時仍在耗電並送出可聽見的底噪。這裡先全部關掉，之後只在真的要發聲或錄音
    // 時才啟動，用完立刻關。
    M5Cardputer.Speaker.end();
    M5Cardputer.Mic.end();
    speaker_active_ = false;
    applyVolume();
#if CMT_CODEC2_AVAILABLE
    codec_ = codec2_create(CODEC2_MODE_700C);
    if (codec_ == nullptr) {
      return false;
    }
    samples_per_frame_ = codec2_samples_per_frame(codec_);
    bytes_per_frame_ = codec2_bytes_per_frame(codec_);
    if (samples_per_frame_ != kVoiceSamplesPerFrame ||
        codec2_bits_per_frame(codec_) != kVoiceBitsPerFrame ||
        bytes_per_frame_ != kVoiceCodedBytesPerFrame) {
      codec2_destroy(codec_);
      codec_ = nullptr;
      return false;
    }
    pcm_.resize(kMaxSamples);
    encoded_.reserve(static_cast<std::size_t>(kMaxVoiceFrames) *
                     kVoiceCodedBytesPerFrame);
    available_ = samples_per_frame_ > 0 && bytes_per_frame_ > 0;
#endif
    return available_;
  }

  bool startRecording(const std::uint32_t now_ms) {
    (void)now_ms;
    if (!available_ || recording_ || playing_) {
      return false;
    }
    M5Cardputer.Mic.end();
    if (speakerOn()) {
      M5Cardputer.Speaker.tone(1000, 200);
      // 麥克風與喇叭共用音訊資源，提示音必須先放完才能切到錄音。
      const std::uint32_t deadline = millis() + 500UL;
      while (M5Cardputer.Speaker.isPlaying() &&
             static_cast<std::int32_t>(millis() - deadline) < 0) {
        delay(1);
      }
    }
    speakerOff();
    if (!M5Cardputer.Mic.begin()) {
      return false;
    }
    pcm_.resize(kMaxSamples);
    completed_samples_ = 0;
    encoded_.clear();
    // 呼叫端的 now_ms 是提示音之前取的，拿它當起點等於把約 200 ms 的嗶聲算進
    // 錄音時間，錄音窗口會短少同樣的量。以實際開始收音的時刻為準。
    started_at_ms_ = millis();
    recording_ = true;
    return true;
  }

  bool updateRecording(const bool ptt_held, const std::uint32_t now_ms) {
    if (!recording_) {
      return false;
    }
#if CMT_CODEC2_AVAILABLE
    const bool time_expired = now_ms - started_at_ms_ >= kMaxRecordingMs;
    const bool buffer_full =
        completed_samples_ + samples_per_frame_ > pcm_.size();
    if (!ptt_held || time_expired || buffer_full) {
      finishRecording();
      return true;
    }
    if (M5Cardputer.Mic.record(pcm_.data() + completed_samples_,
                               samples_per_frame_, kSampleRate)) {
      completed_samples_ += samples_per_frame_;
    }
#else
    (void)ptt_held;
    (void)now_ms;
#endif
    return false;
  }

  void finishRecording() {
    while (M5Cardputer.Mic.isRecording()) {
      delay(1);
    }
    M5Cardputer.Mic.end();
#if CMT_CODEC2_AVAILABLE
    const std::size_t frames = completed_samples_ / samples_per_frame_;
    encoded_.resize(frames * bytes_per_frame_);
    for (std::size_t frame = 0; frame < frames; ++frame) {
      codec2_encode(codec_, encoded_.data() + frame * bytes_per_frame_,
                    pcm_.data() + frame * samples_per_frame_);
    }
#endif
    recording_ = false;
  }

  bool takeEncoded(std::vector<std::uint8_t>& encoded) {
    if (recording_ || encoded_.empty()) {
      return false;
    }
    encoded = std::move(encoded_);
    encoded_.clear();
    return true;
  }

  bool playEncoded(const std::vector<std::uint8_t>& encoded) {
#if CMT_CODEC2_AVAILABLE
    if (!available_ || encoded.empty() ||
        encoded.size() % bytes_per_frame_ != 0U) {
      return false;
    }
    if (recording_ || playing_) {
      if (!pending_encoded_.empty()) {
        return false;
      }
      pending_encoded_ = encoded;
      return true;
    }
    return startPlayback(encoded);
#else
    (void)encoded;
    return false;
#endif
  }

  std::uint8_t volumePercent() const { return volume_percent_; }

  std::uint8_t adjustVolume(const int delta_percent) {
    const int adjusted = std::clamp(static_cast<int>(volume_percent_) +
                                        delta_percent,
                                    0, 100);
    volume_percent_ = static_cast<std::uint8_t>(adjusted);
    applyVolume();
    return volume_percent_;
  }

  bool startPlayback(const std::vector<std::uint8_t>& encoded) {
#if CMT_CODEC2_AVAILABLE
    const std::size_t frames = encoded.size() / bytes_per_frame_;
    pcm_.resize(frames * samples_per_frame_);
    for (std::size_t frame = 0; frame < frames; ++frame) {
      codec2_decode(codec_, pcm_.data() + frame * samples_per_frame_,
                    encoded.data() + frame * bytes_per_frame_);
    }
    M5Cardputer.Mic.end();
    if (!speakerOn()) {
      return false;
    }
    if (!M5Cardputer.Speaker.playRaw(pcm_.data(), pcm_.size(), kSampleRate,
                                     false, 1, 0)) {
      speakerOff();
      return false;
    }
    playing_ = true;
    return true;
#else
    (void)encoded;
    return false;
#endif
  }

  bool playAlert(const AlertTone tone) {
    if (recording_ || playing_) {
      return false;
    }
    if (!speakerOn()) {
      return false;
    }
    switch (tone) {
      case AlertTone::Text:
        M5Cardputer.Speaker.tone(2200, 90);
        M5Cardputer.Speaker.tone(2900, 110);
        break;
      case AlertTone::Voice:
        M5Cardputer.Speaker.tone(1500, 90);
        M5Cardputer.Speaker.tone(1900, 90);
        M5Cardputer.Speaker.tone(2400, 140);
        break;
      case AlertTone::Confirm:
        M5Cardputer.Speaker.tone(2600, 60);
        break;
    }
    return true;
  }

  void update() {
    if (playing_ && !M5Cardputer.Speaker.isPlaying()) {
      playing_ = false;
    }
    if (!recording_ && !playing_ && !pending_encoded_.empty() &&
        !M5Cardputer.Speaker.isPlaying()) {
      std::vector<std::uint8_t> pending = std::move(pending_encoded_);
      pending_encoded_.clear();
      startPlayback(pending);
      return;
    }
    // 沒有東西在放就把功放關掉。這是「播過一次之後一直有電流聲」的修正點：
    // 先前只有語音播放結束會 end()，提示音與其他路徑會讓 I2S 一直開著。
    if (!speaker_active_) {
      return;
    }
    const std::uint32_t now = millis();
    if (M5Cardputer.Speaker.isPlaying()) {
      speaker_idle_since_ms_ = 0;
      return;
    }
    if (now - speaker_started_ms_ < kSpeakerMinOnMs) {
      return;
    }
    if (speaker_idle_since_ms_ == 0U) {
      speaker_idle_since_ms_ = now == 0U ? 1U : now;
      return;
    }
    if (now - speaker_idle_since_ms_ >= kSpeakerIdleOffMs) {
      speakerOff();
    }
  }

  bool available() const { return available_; }
  bool recording() const { return recording_; }
  bool playing() const { return playing_; }
  float recordingProgress(const std::uint32_t now_ms) const {
    if (!recording_) {
      return 0.0F;
    }
    return std::min(1.0F, static_cast<float>(now_ms - started_at_ms_) /
                              static_cast<float>(kMaxRecordingMs));
  }

 private:
  void applyVolume() {
    const auto hardware_volume = static_cast<std::uint8_t>(
        (static_cast<unsigned>(volume_percent_) * 255U + 50U) / 100U);
    M5Cardputer.Speaker.setVolume(hardware_volume);
  }

  // 唯一的功放開關入口，讓「開了就一定會關」這件事只有一處要維護。
  bool speakerOn() {
    if (speaker_active_) {
      speaker_idle_since_ms_ = 0;
      return true;
    }
    if (!M5Cardputer.Speaker.begin()) {
      return false;
    }
    applyVolume();
    speaker_active_ = true;
    speaker_started_ms_ = millis();
    speaker_idle_since_ms_ = 0;
    return true;
  }

  void speakerOff() {
    if (!speaker_active_) {
      return;
    }
    M5Cardputer.Speaker.stop();
    M5Cardputer.Speaker.end();
    speaker_active_ = false;
    speaker_idle_since_ms_ = 0;
  }

#if CMT_CODEC2_AVAILABLE
  CODEC2* codec_ = nullptr;
#endif
  bool available_ = false;
  bool recording_ = false;
  bool playing_ = false;
  bool speaker_active_ = false;
  std::uint32_t speaker_started_ms_ = 0;
  std::uint32_t speaker_idle_since_ms_ = 0;
  std::uint8_t volume_percent_ = kDefaultVolumePercent;
  std::uint32_t started_at_ms_ = 0;
  std::size_t samples_per_frame_ = 0;
  std::size_t bytes_per_frame_ = 0;
  std::size_t completed_samples_ = 0;
  std::vector<std::int16_t> pcm_;
  std::vector<std::uint8_t> encoded_;
  std::vector<std::uint8_t> pending_encoded_;
};

AudioService::AudioService() : impl_(new Impl()) {}
AudioService::~AudioService() = default;
bool AudioService::begin() { return impl_->begin(); }
bool AudioService::available() const { return impl_->available(); }
bool AudioService::startRecording(const std::uint32_t now_ms) {
  return impl_->startRecording(now_ms);
}
bool AudioService::updateRecording(const bool ptt_held,
                                   const std::uint32_t now_ms) {
  return impl_->updateRecording(ptt_held, now_ms);
}
bool AudioService::takeEncoded(std::vector<std::uint8_t>& encoded) {
  return impl_->takeEncoded(encoded);
}
bool AudioService::playEncoded(const std::vector<std::uint8_t>& encoded) {
  return impl_->playEncoded(encoded);
}
bool AudioService::playAlert(const AlertTone tone) {
  return impl_->playAlert(tone);
}
std::uint8_t AudioService::volumePercent() const {
  return impl_->volumePercent();
}
std::uint8_t AudioService::adjustVolume(const int delta_percent) {
  return impl_->adjustVolume(delta_percent);
}
void AudioService::update() { impl_->update(); }
bool AudioService::recording() const { return impl_->recording(); }
bool AudioService::playing() const { return impl_->playing(); }
float AudioService::recordingProgress(const std::uint32_t now_ms) const {
  return impl_->recordingProgress(now_ms);
}

}  // namespace cmt

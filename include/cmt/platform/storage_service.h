#pragma once

#include "cmt/core/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cmt {

class StorageService {
 public:
  bool begin();
  bool ready() const;
  const std::vector<std::string>& cannedMessages() const;
  const std::vector<GeoPoint>& track() const;
  bool maybeAppendTrack(const GeoPoint& point, std::uint32_t day_key,
                        std::uint32_t unix_time, std::uint32_t now_ms);

 private:
  void loadCannedMessages();
  void loadDefaultMessages();

  bool ready_ = false;
  std::vector<std::string> canned_messages_;
  std::vector<GeoPoint> track_;
  std::uint32_t last_track_ms_ = 0;
};

}  // namespace cmt

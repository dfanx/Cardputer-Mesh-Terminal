#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cmt {

enum class SequenceState {
  First,
  InOrder,
  Gap,
  OldOrDuplicate,
};

struct SequenceObservation {
  SequenceState state = SequenceState::First;
  std::uint32_t first_missing = 0;
  std::uint32_t last_missing = 0;
};

class SequenceTracker {
 public:
  explicit SequenceTracker(std::size_t source_capacity = 16);

  SequenceObservation observe(std::uint32_t source_id,
                              std::uint32_t day_key,
                              std::uint32_t sequence);

 private:
  struct SourceState {
    std::uint32_t source_id = 0;
    std::uint32_t day_key = 0;
    std::uint32_t last_sequence = 0;
  };

  std::size_t source_capacity_;
  std::vector<SourceState> states_;
};

}  // namespace cmt

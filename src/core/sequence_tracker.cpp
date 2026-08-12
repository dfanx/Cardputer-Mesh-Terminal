#include "cmt/core/sequence_tracker.h"

#include <algorithm>

namespace cmt {

SequenceTracker::SequenceTracker(const std::size_t source_capacity)
    : source_capacity_(std::max<std::size_t>(1U, source_capacity)) {
  states_.reserve(source_capacity_);
}

SequenceObservation SequenceTracker::observe(const std::uint32_t source_id,
                                             const std::uint32_t day_key,
                                             const std::uint32_t sequence) {
  auto state = std::find_if(states_.begin(), states_.end(),
                            [source_id](const SourceState& item) {
                              return item.source_id == source_id;
                            });
  if (state == states_.end()) {
    if (states_.size() >= source_capacity_) {
      states_.erase(states_.begin());
    }
    states_.push_back(SourceState{source_id, day_key, sequence});
    return SequenceObservation{SequenceState::First, 0, 0};
  }

  if (state->day_key != day_key || sequence == 0U) {
    state->day_key = day_key;
    state->last_sequence = sequence;
    return SequenceObservation{SequenceState::First, 0, 0};
  }
  if (sequence <= state->last_sequence) {
    return SequenceObservation{SequenceState::OldOrDuplicate, 0, 0};
  }
  if (sequence == state->last_sequence + 1U) {
    state->last_sequence = sequence;
    return SequenceObservation{SequenceState::InOrder, 0, 0};
  }

  const SequenceObservation observation{SequenceState::Gap,
                                        state->last_sequence + 1U,
                                        sequence - 1U};
  state->last_sequence = sequence;
  return observation;
}

}  // namespace cmt

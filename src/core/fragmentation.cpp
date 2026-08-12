#include "cmt/core/fragmentation.h"

#include <algorithm>

namespace cmt {

bool fragmentMessage(const PacketHeader& base_header,
                     const std::vector<std::uint8_t>& message,
                     std::vector<PlainFragment>& fragments) {
  if (message.size() > kMaxMessageBytes) {
    return false;
  }
  const std::size_t count =
      std::max<std::size_t>(1U, (message.size() + kMaxFragmentPayloadBytes - 1U) /
                                   kMaxFragmentPayloadBytes);
  if (count > kMaxFragmentCount) {
    return false;
  }

  fragments.clear();
  fragments.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    const std::size_t offset = index * kMaxFragmentPayloadBytes;
    const std::size_t remaining =
        offset < message.size() ? message.size() - offset : 0U;
    const std::size_t length =
        std::min<std::size_t>(remaining, kMaxFragmentPayloadBytes);

    PlainFragment fragment{};
    fragment.header = base_header;
    fragment.header.fragment_index = static_cast<std::uint8_t>(index);
    fragment.header.fragment_count = static_cast<std::uint8_t>(count);
    fragment.header.payload_length = static_cast<std::uint8_t>(length);
    if (length > 0U) {
      fragment.payload.insert(fragment.payload.end(), message.begin() + offset,
                              message.begin() + offset + length);
    }
    fragments.push_back(std::move(fragment));
  }
  return true;
}

Reassembler::Reassembler(const std::size_t capacity,
                         const std::uint32_t timeout_ms)
    : capacity_(std::max<std::size_t>(1U, capacity)),
      timeout_ms_(timeout_ms) {
  assemblies_.reserve(capacity_);
}

bool Reassembler::sameMessage(const Assembly& assembly,
                              const PacketHeader& header) const {
  return assembly.first_header.group_id == header.group_id &&
         assembly.first_header.source_id == header.source_id &&
         assembly.first_header.message_id == header.message_id &&
         assembly.first_header.type == header.type &&
         assembly.first_header.fragment_count == header.fragment_count;
}

bool Reassembler::isExpired(const std::uint32_t expires_at,
                            const std::uint32_t now_ms) const {
  return static_cast<std::int32_t>(now_ms - expires_at) >= 0;
}

void Reassembler::expire(const std::uint32_t now_ms) {
  assemblies_.erase(
      std::remove_if(assemblies_.begin(), assemblies_.end(),
                     [this, now_ms](const Assembly& assembly) {
                       return isExpired(assembly.expires_at, now_ms);
                     }),
      assemblies_.end());
}

ReassemblyResult Reassembler::accept(
    const PacketHeader& header, const std::vector<std::uint8_t>& payload,
    const std::uint32_t now_ms) {
  ReassemblyResult result{};
  if (validateHeader(header) != HeaderError::None ||
      payload.size() != header.payload_length) {
    return result;
  }

  expire(now_ms);
  auto found = std::find_if(
      assemblies_.begin(), assemblies_.end(),
      [this, &header](const Assembly& item) { return sameMessage(item, header); });

  if (found == assemblies_.end()) {
    if (assemblies_.size() >= capacity_) {
      assemblies_.erase(assemblies_.begin());
    }
    Assembly assembly{};
    assembly.first_header = header;
    assembly.expires_at = now_ms + timeout_ms_;
    assembly.parts.resize(header.fragment_count);
    assembly.received.assign(header.fragment_count, false);
    assemblies_.push_back(std::move(assembly));
    found = assemblies_.end() - 1;
  }

  const std::size_t index = header.fragment_index;
  if (found->received[index]) {
    result.state = ReassemblyState::Duplicate;
    return result;
  }
  if (found->total_bytes + payload.size() > kMaxMessageBytes) {
    assemblies_.erase(found);
    return result;
  }

  found->parts[index] = payload;
  found->received[index] = true;
  ++found->received_count;
  found->total_bytes += payload.size();
  found->expires_at = now_ms + timeout_ms_;

  if (found->received_count != found->parts.size()) {
    result.state = ReassemblyState::Accepted;
    return result;
  }

  result.state = ReassemblyState::Complete;
  result.header = found->first_header;
  result.message.reserve(found->total_bytes);
  for (const auto& part : found->parts) {
    result.message.insert(result.message.end(), part.begin(), part.end());
  }
  assemblies_.erase(found);
  return result;
}

std::size_t Reassembler::pendingCount() const { return assemblies_.size(); }

}  // namespace cmt

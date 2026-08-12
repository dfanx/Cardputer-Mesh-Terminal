#include "cmt/core/mesh_router.h"

#include <algorithm>

namespace cmt {

MeshRouter::MeshRouter(const std::size_t cache_capacity,
                       const std::uint32_t cache_lifetime_ms)
    : cache_capacity_(std::max<std::size_t>(1U, cache_capacity)),
      cache_lifetime_ms_(cache_lifetime_ms) {
  cache_.reserve(cache_capacity_);
}

bool MeshRouter::isSame(const PacketIdentity& left,
                        const PacketIdentity& right) const {
  return left.source_id == right.source_id &&
         left.message_id == right.message_id &&
         left.fragment_index == right.fragment_index;
}

bool MeshRouter::isExpired(const std::uint32_t expires_at,
                           const std::uint32_t now_ms) const {
  return static_cast<std::int32_t>(now_ms - expires_at) >= 0;
}

RouteDecision MeshRouter::onAuthenticatedPacket(
    const PacketHeader& header, const std::uint32_t local_node_id,
    const std::uint32_t now_ms) {
  RouteDecision decision{};
  if (header.source_id == local_node_id ||
      validateHeader(header) != HeaderError::None) {
    return decision;
  }

  cache_.erase(std::remove_if(cache_.begin(), cache_.end(),
                              [this, now_ms](const CacheEntry& entry) {
                                return isExpired(entry.expires_at, now_ms);
                              }),
               cache_.end());

  const PacketIdentity identity{header.source_id, header.message_id,
                                header.fragment_index};
  const auto duplicate = std::find_if(
      cache_.begin(), cache_.end(), [this, &identity](const CacheEntry& entry) {
        return isSame(entry.identity, identity);
      });
  if (duplicate != cache_.end()) {
    decision.duplicate = true;
    return decision;
  }

  if (cache_.size() >= cache_capacity_) {
    cache_.erase(cache_.begin());
  }
  cache_.push_back(CacheEntry{identity, now_ms + cache_lifetime_ms_});

  decision.deliver = true;
  if (header.ttl > 0U) {
    decision.relay = true;
    decision.relay_header = header;
    --decision.relay_header.ttl;
    ++decision.relay_header.hop_count;
  }
  return decision;
}

}  // namespace cmt

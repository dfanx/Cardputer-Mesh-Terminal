#pragma once

#include "cmt/core/wire_protocol.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cmt {

struct PacketIdentity {
  std::uint32_t source_id = 0;
  std::uint32_t message_id = 0;
  std::uint8_t fragment_index = 0;
};

struct RouteDecision {
  bool deliver = false;
  bool relay = false;
  bool duplicate = false;
  PacketHeader relay_header{};
};

class MeshRouter {
 public:
  explicit MeshRouter(std::size_t cache_capacity = 64,
                      std::uint32_t cache_lifetime_ms = 120000);

  RouteDecision onAuthenticatedPacket(const PacketHeader& header,
                                      std::uint32_t local_node_id,
                                      std::uint32_t now_ms);

 private:
  struct CacheEntry {
    PacketIdentity identity{};
    std::uint32_t expires_at = 0;
  };

  bool isSame(const PacketIdentity& left, const PacketIdentity& right) const;
  bool isExpired(std::uint32_t expires_at, std::uint32_t now_ms) const;

  std::size_t cache_capacity_;
  std::uint32_t cache_lifetime_ms_;
  std::vector<CacheEntry> cache_;
};

}  // namespace cmt

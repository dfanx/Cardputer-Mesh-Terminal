#pragma once

#include "cmt/core/wire_protocol.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cmt {

struct PlainFragment {
  PacketHeader header{};
  std::vector<std::uint8_t> payload;
};

bool fragmentMessage(const PacketHeader& base_header,
                     const std::vector<std::uint8_t>& message,
                     std::vector<PlainFragment>& fragments);

enum class ReassemblyState {
  Accepted,
  Duplicate,
  Complete,
  Rejected,
};

struct ReassemblyResult {
  ReassemblyState state = ReassemblyState::Rejected;
  PacketHeader header{};
  std::vector<std::uint8_t> message;
};

class Reassembler {
 public:
  explicit Reassembler(std::size_t capacity = 4,
                       std::uint32_t timeout_ms = 15000);

  ReassemblyResult accept(const PacketHeader& header,
                          const std::vector<std::uint8_t>& payload,
                          std::uint32_t now_ms);
  void expire(std::uint32_t now_ms);
  std::size_t pendingCount() const;

 private:
  struct Assembly {
    PacketHeader first_header{};
    std::uint32_t expires_at = 0;
    std::vector<std::vector<std::uint8_t>> parts;
    std::vector<bool> received;
    std::size_t received_count = 0;
    std::size_t total_bytes = 0;
  };

  bool sameMessage(const Assembly& assembly,
                   const PacketHeader& header) const;
  bool isExpired(std::uint32_t expires_at, std::uint32_t now_ms) const;

  std::size_t capacity_;
  std::uint32_t timeout_ms_;
  std::vector<Assembly> assemblies_;
};

}  // namespace cmt

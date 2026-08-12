#pragma once

#include "cmt/core/types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace cmt {

struct PacketHeader {
  std::uint8_t version = 1;
  MessageType type = MessageType::Text;
  std::uint8_t flags = 0;
  std::uint8_t ttl = 0;
  std::uint8_t hop_count = 0;
  std::uint8_t fragment_index = 0;
  std::uint8_t fragment_count = 1;
  std::uint8_t payload_length = 0;
  std::uint32_t group_id = 0;
  std::uint32_t source_id = 0;
  std::uint32_t message_id = 0;
  std::uint32_t sequence = 0;
  std::uint32_t timestamp = 0;
  std::array<std::uint8_t, 12> nonce{};
};

enum class HeaderError {
  None,
  BufferTooSmall,
  BadMagic,
  UnsupportedVersion,
  UnknownType,
  InvalidTtl,
  InvalidHopCount,
  InvalidFragments,
  PayloadTooLarge,
};

HeaderError validateHeader(const PacketHeader& header);
HeaderError encodeHeader(const PacketHeader& header, std::uint8_t* output,
                         std::size_t output_size);
HeaderError decodeHeader(const std::uint8_t* input, std::size_t input_size,
                         PacketHeader& header);

}  // namespace cmt

#include "cmt/core/wire_protocol.h"

namespace cmt {
namespace {

constexpr std::uint8_t kMagic0 = 'C';
constexpr std::uint8_t kMagic1 = 'M';
constexpr std::uint8_t kProtocolVersion = 1;

void writeU32(std::uint8_t* output, const std::uint32_t value) {
  output[0] = static_cast<std::uint8_t>(value >> 24U);
  output[1] = static_cast<std::uint8_t>(value >> 16U);
  output[2] = static_cast<std::uint8_t>(value >> 8U);
  output[3] = static_cast<std::uint8_t>(value);
}

std::uint32_t readU32(const std::uint8_t* input) {
  return (static_cast<std::uint32_t>(input[0]) << 24U) |
         (static_cast<std::uint32_t>(input[1]) << 16U) |
         (static_cast<std::uint32_t>(input[2]) << 8U) |
         static_cast<std::uint32_t>(input[3]);
}

}  // namespace

HeaderError validateHeader(const PacketHeader& header) {
  if (header.version != kProtocolVersion) {
    return HeaderError::UnsupportedVersion;
  }
  if (!isKnownMessageType(header.type)) {
    return HeaderError::UnknownType;
  }
  if (header.ttl > 2U) {
    return HeaderError::InvalidTtl;
  }
  if (header.hop_count > 2U) {
    return HeaderError::InvalidHopCount;
  }
  if (header.fragment_count == 0U ||
      header.fragment_count > kMaxFragmentCount ||
      header.fragment_index >= header.fragment_count) {
    return HeaderError::InvalidFragments;
  }
  if (header.payload_length > kMaxFragmentPayloadBytes) {
    return HeaderError::PayloadTooLarge;
  }
  return HeaderError::None;
}

HeaderError encodeHeader(const PacketHeader& header, std::uint8_t* output,
                         const std::size_t output_size) {
  if (output == nullptr || output_size < kWireHeaderBytes) {
    return HeaderError::BufferTooSmall;
  }
  const HeaderError validity = validateHeader(header);
  if (validity != HeaderError::None) {
    return validity;
  }

  output[0] = kMagic0;
  output[1] = kMagic1;
  output[2] = header.version;
  output[3] = static_cast<std::uint8_t>(header.type);
  output[4] = header.flags;
  output[5] = header.ttl;
  output[6] = header.hop_count;
  output[7] = header.fragment_index;
  output[8] = header.fragment_count;
  output[9] = header.payload_length;
  writeU32(output + 10, header.group_id);
  writeU32(output + 14, header.source_id);
  writeU32(output + 18, header.message_id);
  writeU32(output + 22, header.sequence);
  writeU32(output + 26, header.timestamp);
  for (std::size_t index = 0; index < header.nonce.size(); ++index) {
    output[30 + index] = header.nonce[index];
  }
  return HeaderError::None;
}

HeaderError decodeHeader(const std::uint8_t* input,
                         const std::size_t input_size, PacketHeader& header) {
  if (input == nullptr || input_size < kWireHeaderBytes) {
    return HeaderError::BufferTooSmall;
  }
  if (input[0] != kMagic0 || input[1] != kMagic1) {
    return HeaderError::BadMagic;
  }

  header.version = input[2];
  header.type = static_cast<MessageType>(input[3]);
  header.flags = input[4];
  header.ttl = input[5];
  header.hop_count = input[6];
  header.fragment_index = input[7];
  header.fragment_count = input[8];
  header.payload_length = input[9];
  header.group_id = readU32(input + 10);
  header.source_id = readU32(input + 14);
  header.message_id = readU32(input + 18);
  header.sequence = readU32(input + 22);
  header.timestamp = readU32(input + 26);
  for (std::size_t index = 0; index < header.nonce.size(); ++index) {
    header.nonce[index] = input[30 + index];
  }
  return validateHeader(header);
}

}  // namespace cmt

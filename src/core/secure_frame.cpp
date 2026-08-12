#include "cmt/core/secure_frame.h"

#include <algorithm>

namespace cmt {

FrameError sealFrame(PacketHeader header,
                     const std::vector<std::uint8_t>& plaintext,
                     CryptoProvider& crypto, std::vector<std::uint8_t>& wire) {
  if (plaintext.size() > kMaxFragmentPayloadBytes) {
    return FrameError::InvalidHeader;
  }
  header.payload_length = static_cast<std::uint8_t>(plaintext.size());

  std::array<std::uint8_t, kWireHeaderBytes> encoded_header{};
  if (encodeHeader(header, encoded_header.data(), encoded_header.size()) !=
      HeaderError::None) {
    return FrameError::InvalidHeader;
  }

  std::vector<std::uint8_t> ciphertext;
  std::array<std::uint8_t, kAuthTagBytes> tag{};
  if (!crypto.encrypt(header.nonce, encoded_header.data(),
                      encoded_header.size(), plaintext.data(),
                      plaintext.size(), ciphertext, tag) ||
      ciphertext.size() != plaintext.size()) {
    return FrameError::CryptoFailure;
  }

  wire.clear();
  wire.reserve(encoded_header.size() + ciphertext.size() + tag.size());
  wire.insert(wire.end(), encoded_header.begin(), encoded_header.end());
  wire.insert(wire.end(), ciphertext.begin(), ciphertext.end());
  wire.insert(wire.end(), tag.begin(), tag.end());
  return FrameError::None;
}

FrameError openFrame(const std::uint8_t* wire, const std::size_t wire_size,
                     const std::uint32_t expected_group,
                     CryptoProvider& crypto, DecodedFrame& decoded) {
  if (wire == nullptr ||
      wire_size < kWireHeaderBytes + kAuthTagBytes ||
      wire_size > kMaxRadioPacketBytes) {
    return FrameError::SizeMismatch;
  }

  PacketHeader header{};
  if (decodeHeader(wire, wire_size, header) != HeaderError::None) {
    return FrameError::InvalidHeader;
  }
  if (header.group_id != expected_group) {
    return FrameError::WrongGroup;
  }
  const std::size_t expected_size =
      kWireHeaderBytes + header.payload_length + kAuthTagBytes;
  if (wire_size != expected_size) {
    return FrameError::SizeMismatch;
  }

  std::array<std::uint8_t, kAuthTagBytes> tag{};
  std::copy(wire + kWireHeaderBytes + header.payload_length,
            wire + expected_size, tag.begin());

  std::vector<std::uint8_t> plaintext;
  if (!crypto.decrypt(header.nonce, wire, kWireHeaderBytes,
                      wire + kWireHeaderBytes, header.payload_length, tag,
                      plaintext) ||
      plaintext.size() != header.payload_length) {
    return FrameError::CryptoFailure;
  }

  decoded.header = header;
  decoded.payload = std::move(plaintext);
  return FrameError::None;
}

}  // namespace cmt

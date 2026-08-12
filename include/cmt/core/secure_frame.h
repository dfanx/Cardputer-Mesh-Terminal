#pragma once

#include "cmt/core/wire_protocol.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace cmt {

class CryptoProvider {
 public:
  virtual ~CryptoProvider() = default;

  virtual bool encrypt(const std::array<std::uint8_t, 12>& nonce,
                       const std::uint8_t* aad, std::size_t aad_size,
                       const std::uint8_t* plaintext,
                       std::size_t plaintext_size,
                       std::vector<std::uint8_t>& ciphertext,
                       std::array<std::uint8_t, kAuthTagBytes>& tag) = 0;

  virtual bool decrypt(const std::array<std::uint8_t, 12>& nonce,
                       const std::uint8_t* aad, std::size_t aad_size,
                       const std::uint8_t* ciphertext,
                       std::size_t ciphertext_size,
                       const std::array<std::uint8_t, kAuthTagBytes>& tag,
                       std::vector<std::uint8_t>& plaintext) = 0;
};

struct DecodedFrame {
  PacketHeader header{};
  std::vector<std::uint8_t> payload;
};

enum class FrameError {
  None,
  InvalidHeader,
  WrongGroup,
  SizeMismatch,
  CryptoFailure,
};

FrameError sealFrame(PacketHeader header,
                     const std::vector<std::uint8_t>& plaintext,
                     CryptoProvider& crypto, std::vector<std::uint8_t>& wire);
FrameError openFrame(const std::uint8_t* wire, std::size_t wire_size,
                     std::uint32_t expected_group, CryptoProvider& crypto,
                     DecodedFrame& decoded);

}  // namespace cmt

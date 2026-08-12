#pragma once

#include "cmt/core/secure_frame.h"

#include <mbedtls/gcm.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace cmt {

class MbedTlsCrypto final : public CryptoProvider {
 public:
  MbedTlsCrypto();
  ~MbedTlsCrypto() override;

  MbedTlsCrypto(const MbedTlsCrypto&) = delete;
  MbedTlsCrypto& operator=(const MbedTlsCrypto&) = delete;

  bool begin(const std::string& pin, std::uint32_t group_id);
  bool ready() const;

  bool encrypt(const std::array<std::uint8_t, 12>& nonce,
               const std::uint8_t* aad, std::size_t aad_size,
               const std::uint8_t* plaintext, std::size_t plaintext_size,
               std::vector<std::uint8_t>& ciphertext,
               std::array<std::uint8_t, kAuthTagBytes>& tag) override;
  bool decrypt(const std::array<std::uint8_t, 12>& nonce,
               const std::uint8_t* aad, std::size_t aad_size,
               const std::uint8_t* ciphertext, std::size_t ciphertext_size,
               const std::array<std::uint8_t, kAuthTagBytes>& tag,
               std::vector<std::uint8_t>& plaintext) override;

 private:
  void clear();

  mbedtls_gcm_context context_{};
  std::array<std::uint8_t, 16> key_{};
  bool ready_ = false;
};

}  // namespace cmt

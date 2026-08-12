#include "cmt/platform/mbedtls_crypto.h"

#include "cmt/core/channel_plan.h"

#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>

#include <cstdio>
#include <cstring>

namespace cmt {

MbedTlsCrypto::MbedTlsCrypto() { mbedtls_gcm_init(&context_); }

MbedTlsCrypto::~MbedTlsCrypto() {
  clear();
  mbedtls_gcm_free(&context_);
}

void MbedTlsCrypto::clear() {
  volatile std::uint8_t* bytes = key_.data();
  for (std::size_t index = 0; index < key_.size(); ++index) {
    bytes[index] = 0;
  }
  ready_ = false;
}

bool MbedTlsCrypto::begin(const std::string& pin,
                         const std::uint32_t group_id) {
  clear();
  if (!isValidPin(pin)) {
    return false;
  }

  char salt[24]{};
  const int salt_length =
      std::snprintf(salt, sizeof(salt), "CMT-v1:%08lX",
                    static_cast<unsigned long>(group_id));
  if (salt_length <= 0 || static_cast<std::size_t>(salt_length) >= sizeof(salt)) {
    return false;
  }

  mbedtls_md_context_t md{};
  mbedtls_md_init(&md);
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  bool success = info != nullptr && mbedtls_md_setup(&md, info, 1) == 0;
  if (success) {
    success = mbedtls_pkcs5_pbkdf2_hmac(
                  &md, reinterpret_cast<const unsigned char*>(pin.data()),
                  pin.size(), reinterpret_cast<const unsigned char*>(salt),
                  static_cast<std::size_t>(salt_length), 10000, key_.size(),
                  key_.data()) == 0;
  }
  mbedtls_md_free(&md);
  if (!success ||
      mbedtls_gcm_setkey(&context_, MBEDTLS_CIPHER_ID_AES, key_.data(), 128) !=
          0) {
    clear();
    return false;
  }
  ready_ = true;
  return true;
}

bool MbedTlsCrypto::ready() const { return ready_; }

bool MbedTlsCrypto::encrypt(
    const std::array<std::uint8_t, 12>& nonce, const std::uint8_t* aad,
    const std::size_t aad_size, const std::uint8_t* plaintext,
    const std::size_t plaintext_size, std::vector<std::uint8_t>& ciphertext,
    std::array<std::uint8_t, kAuthTagBytes>& tag) {
  if (!ready_ || aad == nullptr ||
      (plaintext_size > 0U && plaintext == nullptr)) {
    return false;
  }
  ciphertext.resize(plaintext_size);
  std::uint8_t dummy = 0;
  const std::uint8_t* input = plaintext_size > 0U ? plaintext : &dummy;
  std::uint8_t* output = plaintext_size > 0U ? ciphertext.data() : &dummy;
  return mbedtls_gcm_crypt_and_tag(
             &context_, MBEDTLS_GCM_ENCRYPT, plaintext_size, nonce.data(),
             nonce.size(), aad, aad_size, input, output, tag.size(),
             tag.data()) == 0;
}

bool MbedTlsCrypto::decrypt(
    const std::array<std::uint8_t, 12>& nonce, const std::uint8_t* aad,
    const std::size_t aad_size, const std::uint8_t* ciphertext,
    const std::size_t ciphertext_size,
    const std::array<std::uint8_t, kAuthTagBytes>& tag,
    std::vector<std::uint8_t>& plaintext) {
  if (!ready_ || aad == nullptr ||
      (ciphertext_size > 0U && ciphertext == nullptr)) {
    return false;
  }
  plaintext.resize(ciphertext_size);
  std::uint8_t dummy = 0;
  const std::uint8_t* input = ciphertext_size > 0U ? ciphertext : &dummy;
  std::uint8_t* output = ciphertext_size > 0U ? plaintext.data() : &dummy;
  if (mbedtls_gcm_auth_decrypt(
          &context_, ciphertext_size, nonce.data(), nonce.size(), aad,
          aad_size, tag.data(), tag.size(), input, output) != 0) {
    plaintext.clear();
    return false;
  }
  return true;
}

}  // namespace cmt

#include <unity.h>

#include "cmt/core/channel_plan.h"
#include "cmt/core/fragmentation.h"
#include "cmt/core/geo.h"
#include "cmt/core/mesh_router.h"
#include "cmt/core/message_codec.h"
#include "cmt/core/secure_frame.h"
#include "cmt/core/sequence_tracker.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

class TestCrypto final : public cmt::CryptoProvider {
 public:
  bool encrypt(const std::array<std::uint8_t, 12>& nonce,
               const std::uint8_t* aad, const std::size_t aad_size,
               const std::uint8_t* plaintext,
               const std::size_t plaintext_size,
               std::vector<std::uint8_t>& ciphertext,
               std::array<std::uint8_t, cmt::kAuthTagBytes>& tag) override {
    ciphertext.resize(plaintext_size);
    for (std::size_t index = 0; index < plaintext_size; ++index) {
      ciphertext[index] = static_cast<std::uint8_t>(plaintext[index] ^ 0xA5U);
    }
    makeTag(nonce, aad, aad_size, ciphertext.data(), ciphertext.size(), tag);
    return true;
  }

  bool decrypt(
      const std::array<std::uint8_t, 12>& nonce, const std::uint8_t* aad,
      const std::size_t aad_size, const std::uint8_t* ciphertext,
      const std::size_t ciphertext_size,
      const std::array<std::uint8_t, cmt::kAuthTagBytes>& tag,
      std::vector<std::uint8_t>& plaintext) override {
    std::array<std::uint8_t, cmt::kAuthTagBytes> expected{};
    makeTag(nonce, aad, aad_size, ciphertext, ciphertext_size, expected);
    if (!std::equal(expected.begin(), expected.end(), tag.begin())) {
      return false;
    }
    plaintext.resize(ciphertext_size);
    for (std::size_t index = 0; index < ciphertext_size; ++index) {
      plaintext[index] = static_cast<std::uint8_t>(ciphertext[index] ^ 0xA5U);
    }
    return true;
  }

 private:
  static void mix(std::uint32_t& hash, const std::uint8_t* data,
                  const std::size_t size) {
    for (std::size_t index = 0; index < size; ++index) {
      hash ^= data[index];
      hash *= 16777619UL;
    }
  }

  static void makeTag(
      const std::array<std::uint8_t, 12>& nonce, const std::uint8_t* aad,
      const std::size_t aad_size, const std::uint8_t* ciphertext,
      const std::size_t ciphertext_size,
      std::array<std::uint8_t, cmt::kAuthTagBytes>& tag) {
    std::uint32_t hash = 2166136261UL;
    mix(hash, nonce.data(), nonce.size());
    mix(hash, aad, aad_size);
    mix(hash, ciphertext, ciphertext_size);
    for (std::size_t index = 0; index < tag.size(); ++index) {
      hash ^= static_cast<std::uint32_t>(index * 41U);
      hash *= 16777619UL;
      tag[index] = static_cast<std::uint8_t>(hash >> ((index % 4U) * 8U));
    }
  }
};

cmt::PacketHeader makeHeader() {
  cmt::PacketHeader header{};
  header.type = cmt::MessageType::Text;
  header.ttl = cmt::initialTtl(header.type);
  header.group_id = 0x11223344UL;
  header.source_id = 0x01020304UL;
  header.message_id = 99;
  header.sequence = 7;
  header.timestamp = 1786464000UL;
  for (std::size_t index = 0; index < header.nonce.size(); ++index) {
    header.nonce[index] = static_cast<std::uint8_t>(index + 1U);
  }
  return header;
}

void test_channel_plan_is_deterministic_and_bounded() {
  cmt::GroupProfile first{};
  cmt::GroupProfile second{};
  TEST_ASSERT_TRUE(cmt::deriveGroupProfile("8888", first));
  TEST_ASSERT_TRUE(cmt::deriveGroupProfile("8888", second));
  TEST_ASSERT_EQUAL_UINT32(first.group_id, second.group_id);
  TEST_ASSERT_EQUAL_UINT8(first.channel_index, second.channel_index);
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, first.frequency_mhz,
                           second.frequency_mhz);
  TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(920.125F, first.frequency_mhz);
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(922.725F, first.frequency_mhz);
  TEST_ASSERT_FALSE(cmt::deriveGroupProfile("123", second));
  TEST_ASSERT_FALSE(cmt::deriveGroupProfile("12A4", second));
}

void test_secure_frame_round_trip_and_tamper_rejection() {
  TestCrypto crypto;
  const auto header = makeHeader();
  const std::vector<std::uint8_t> message{'h', 'e', 'l', 'l', 'o'};
  std::vector<std::uint8_t> wire;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(cmt::FrameError::None),
                        static_cast<int>(
                            cmt::sealFrame(header, message, crypto, wire)));
  TEST_ASSERT_EQUAL_UINT(cmt::kWireHeaderBytes + message.size() +
                             cmt::kAuthTagBytes,
                         wire.size());

  cmt::DecodedFrame decoded{};
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(cmt::FrameError::None),
      static_cast<int>(cmt::openFrame(wire.data(), wire.size(), header.group_id,
                                      crypto, decoded)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(message.data(), decoded.payload.data(),
                                message.size());
  TEST_ASSERT_EQUAL_UINT32(header.message_id, decoded.header.message_id);

  wire[cmt::kWireHeaderBytes] ^= 0x01U;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(cmt::FrameError::CryptoFailure),
      static_cast<int>(cmt::openFrame(wire.data(), wire.size(), header.group_id,
                                      crypto, decoded)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(cmt::FrameError::WrongGroup),
      static_cast<int>(cmt::openFrame(wire.data(), wire.size(), 0x99UL, crypto,
                                      decoded)));
}

void test_wire_bounds_and_bad_headers_are_rejected() {
  auto header = makeHeader();
  std::array<std::uint8_t, cmt::kWireHeaderBytes> bytes{};
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(cmt::HeaderError::None),
      static_cast<int>(cmt::encodeHeader(header, bytes.data(), bytes.size())));

  cmt::PacketHeader decoded{};
  bytes[2] = 9;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(cmt::HeaderError::UnsupportedVersion),
      static_cast<int>(cmt::decodeHeader(bytes.data(), bytes.size(), decoded)));
  bytes[2] = 1;
  bytes[8] = 0;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(cmt::HeaderError::InvalidFragments),
      static_cast<int>(cmt::decodeHeader(bytes.data(), bytes.size(), decoded)));

  std::vector<std::uint8_t> oversized(cmt::kMaxMessageBytes + 1U, 0x55U);
  std::vector<cmt::PlainFragment> fragments;
  TEST_ASSERT_FALSE(cmt::fragmentMessage(header, oversized, fragments));
  TEST_ASSERT_EQUAL_UINT(255, cmt::kMaxRadioPacketBytes);
  TEST_ASSERT_EQUAL_UINT(197, cmt::kMaxFragmentPayloadBytes);
}

void test_fragmentation_and_out_of_order_reassembly() {
  auto header = makeHeader();
  std::vector<std::uint8_t> message(cmt::kMaxFragmentPayloadBytes + 1U);
  for (std::size_t index = 0; index < message.size(); ++index) {
    message[index] = static_cast<std::uint8_t>(index);
  }
  std::vector<cmt::PlainFragment> fragments;
  TEST_ASSERT_TRUE(cmt::fragmentMessage(header, message, fragments));
  TEST_ASSERT_EQUAL_UINT(2, fragments.size());
  TEST_ASSERT_EQUAL_UINT(cmt::kMaxFragmentPayloadBytes,
                         fragments[0].payload.size());
  TEST_ASSERT_EQUAL_UINT(1, fragments[1].payload.size());

  cmt::Reassembler reassembler(2, 1000);
  auto second = reassembler.accept(fragments[1].header, fragments[1].payload, 0);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(cmt::ReassemblyState::Accepted),
                        static_cast<int>(second.state));
  auto duplicate =
      reassembler.accept(fragments[1].header, fragments[1].payload, 1);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(cmt::ReassemblyState::Duplicate),
                        static_cast<int>(duplicate.state));
  auto complete =
      reassembler.accept(fragments[0].header, fragments[0].payload, 2);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(cmt::ReassemblyState::Complete),
                        static_cast<int>(complete.state));
  TEST_ASSERT_EQUAL_UINT(message.size(), complete.message.size());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(message.data(), complete.message.data(),
                                message.size());
  TEST_ASSERT_EQUAL_UINT(0, reassembler.pendingCount());
}

void test_reassembly_expires_incomplete_message() {
  auto header = makeHeader();
  header.fragment_count = 2;
  header.fragment_index = 0;
  header.payload_length = 1;
  cmt::Reassembler reassembler(1, 100);
  const std::vector<std::uint8_t> payload{0x42U};
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(cmt::ReassemblyState::Accepted),
      static_cast<int>(reassembler.accept(header, payload, 10).state));
  TEST_ASSERT_EQUAL_UINT(1, reassembler.pendingCount());
  reassembler.expire(110);
  TEST_ASSERT_EQUAL_UINT(0, reassembler.pendingCount());
}

void test_mesh_router_ttl_and_duplicate_cache() {
  cmt::MeshRouter router(4, 1000);
  const auto header = makeHeader();
  const auto first = router.onAuthenticatedPacket(header, 0xDEADBEEFUL, 10);
  TEST_ASSERT_TRUE(first.deliver);
  TEST_ASSERT_TRUE(first.relay);
  TEST_ASSERT_EQUAL_UINT8(1, first.relay_header.ttl);
  TEST_ASSERT_EQUAL_UINT8(1, first.relay_header.hop_count);

  const auto duplicate =
      router.onAuthenticatedPacket(header, 0xDEADBEEFUL, 11);
  TEST_ASSERT_TRUE(duplicate.duplicate);
  TEST_ASSERT_FALSE(duplicate.deliver);

  auto final_hop = header;
  final_hop.message_id = 100;
  final_hop.ttl = 0;
  final_hop.hop_count = 2;
  const auto final =
      router.onAuthenticatedPacket(final_hop, 0xDEADBEEFUL, 12);
  TEST_ASSERT_TRUE(final.deliver);
  TEST_ASSERT_FALSE(final.relay);

  auto voice = header;
  voice.type = cmt::MessageType::Voice;
  voice.message_id = 101;
  voice.ttl = cmt::initialTtl(voice.type);
  const auto voice_route =
      router.onAuthenticatedPacket(voice, 0xDEADBEEFUL, 13);
  TEST_ASSERT_TRUE(voice_route.relay);
  TEST_ASSERT_EQUAL_UINT8(0, voice_route.relay_header.ttl);
  TEST_ASSERT_FALSE(
      router.onAuthenticatedPacket(header, header.source_id, 14).deliver);
}

void test_sequence_tracker_reports_only_forward_gaps() {
  cmt::SequenceTracker tracker;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(cmt::SequenceState::First),
      static_cast<int>(tracker.observe(7, 20260812, 1).state));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(cmt::SequenceState::InOrder),
      static_cast<int>(tracker.observe(7, 20260812, 2).state));
  const auto gap = tracker.observe(7, 20260812, 5);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(cmt::SequenceState::Gap),
                        static_cast<int>(gap.state));
  TEST_ASSERT_EQUAL_UINT32(3, gap.first_missing);
  TEST_ASSERT_EQUAL_UINT32(4, gap.last_missing);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(cmt::SequenceState::OldOrDuplicate),
      static_cast<int>(tracker.observe(7, 20260812, 4).state));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(cmt::SequenceState::First),
      static_cast<int>(tracker.observe(7, 20260813, 1).state));
}

void test_beacon_text_and_geo_round_trip() {
  const cmt::BeaconMessage original{{25.0330, 121.5654, 120, true}, 87,
                                    "A-01"};
  std::vector<std::uint8_t> encoded;
  TEST_ASSERT_TRUE(cmt::encodeBeaconMessage(original, encoded));
  cmt::BeaconMessage decoded{};
  TEST_ASSERT_TRUE(cmt::decodeBeaconMessage(encoded, decoded));
  TEST_ASSERT_FLOAT_WITHIN(0.000001F,
                           static_cast<float>(original.point.latitude),
                           static_cast<float>(decoded.point.latitude));
  TEST_ASSERT_FLOAT_WITHIN(0.000001F,
                           static_cast<float>(original.point.longitude),
                           static_cast<float>(decoded.point.longitude));
  TEST_ASSERT_EQUAL_INT16(120, decoded.point.altitude_m);
  TEST_ASSERT_EQUAL_UINT8(87, decoded.battery_percent);
  TEST_ASSERT_EQUAL_STRING("A-01", decoded.callsign.c_str());

  std::vector<std::uint8_t> text_bytes;
  TEST_ASSERT_TRUE(cmt::encodeTextMessage("Need water", text_bytes));
  std::string text;
  TEST_ASSERT_TRUE(cmt::decodeTextMessage(text_bytes, text));
  TEST_ASSERT_EQUAL_STRING("Need water", text.c_str());

  const cmt::GeoPoint east{25.0330, 121.5754, 150, true};
  const auto relative = cmt::relativePosition(original.point, east);
  TEST_ASSERT_TRUE(relative.valid);
  TEST_ASSERT_FLOAT_WITHIN(2.0F, 90.0F,
                           static_cast<float>(relative.bearing_deg));
  TEST_ASSERT_GREATER_THAN_FLOAT(900.0F,
                                 static_cast<float>(relative.distance_m));
  TEST_ASSERT_LESS_THAN_FLOAT(1100.0F,
                              static_cast<float>(relative.distance_m));
  TEST_ASSERT_EQUAL_INT16(30, relative.altitude_delta_m);
  TEST_ASSERT_EQUAL_STRING("E", relative.direction);
}

void test_voice_message_round_trip_and_limits() {
  cmt::VoiceMessage voice{};
  voice.frames.resize(cmt::kMaxVoiceFrames * cmt::kVoiceBytesPerFrame);
  for (std::size_t index = 0; index < voice.frames.size(); ++index) {
    voice.frames[index] = static_cast<std::uint8_t>(index);
  }

  std::vector<std::uint8_t> encoded;
  TEST_ASSERT_TRUE(cmt::encodeVoiceMessage(voice, encoded));
  TEST_ASSERT_EQUAL_UINT(8U + voice.frames.size(), encoded.size());

  cmt::VoiceMessage decoded{};
  TEST_ASSERT_TRUE(cmt::decodeVoiceMessage(encoded, decoded));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(cmt::VoiceCodec::Codec2_1300),
                        static_cast<int>(decoded.codec));
  TEST_ASSERT_EQUAL_UINT16(cmt::kVoiceSampleRateHz,
                           decoded.sample_rate_hz);
  TEST_ASSERT_EQUAL_UINT16(cmt::kVoiceSamplesPerFrame,
                           decoded.samples_per_frame);
  TEST_ASSERT_EQUAL_UINT8(cmt::kVoiceBytesPerFrame,
                          decoded.bytes_per_frame);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(voice.frames.data(), decoded.frames.data(),
                                voice.frames.size());

  auto malformed = encoded;
  malformed[7] = static_cast<std::uint8_t>(cmt::kMaxVoiceFrames - 1U);
  TEST_ASSERT_FALSE(cmt::decodeVoiceMessage(malformed, decoded));
  malformed = encoded;
  malformed[0] = 2U;
  TEST_ASSERT_FALSE(cmt::decodeVoiceMessage(malformed, decoded));

  voice.frames.push_back(0U);
  TEST_ASSERT_FALSE(cmt::encodeVoiceMessage(voice, encoded));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_channel_plan_is_deterministic_and_bounded);
  RUN_TEST(test_secure_frame_round_trip_and_tamper_rejection);
  RUN_TEST(test_wire_bounds_and_bad_headers_are_rejected);
  RUN_TEST(test_fragmentation_and_out_of_order_reassembly);
  RUN_TEST(test_reassembly_expires_incomplete_message);
  RUN_TEST(test_mesh_router_ttl_and_duplicate_cache);
  RUN_TEST(test_sequence_tracker_reports_only_forward_gaps);
  RUN_TEST(test_beacon_text_and_geo_round_trip);
  RUN_TEST(test_voice_message_round_trip_and_limits);
  return UNITY_END();
}

#include "cmt/core/types.h"

namespace cmt {

bool isKnownMessageType(const MessageType type) {
  return type == MessageType::Voice || type == MessageType::Text ||
         type == MessageType::Beacon;
}

std::uint8_t initialTtl(const MessageType type) {
  return type == MessageType::Voice ? 1U : 2U;
}

}  // namespace cmt

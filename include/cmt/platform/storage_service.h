#pragma once

#include <string>
#include <vector>

namespace cmt {

class StorageService {
 public:
  bool begin();
  bool ready() const;
  const std::vector<std::string>& cannedMessages() const;

 private:
  void loadCannedMessages();
  void loadDefaultMessages();

  bool ready_ = false;
  std::vector<std::string> canned_messages_;
};

}  // namespace cmt

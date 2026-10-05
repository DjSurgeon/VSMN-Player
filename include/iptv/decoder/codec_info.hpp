#pragma once

#include <string>

namespace iptv::decoder {

/**
 * @brief Contains information about the active codec.
 */
struct CodecInfo {
  std::string name;
  std::string profile;
  bool hardware_accelerated{false};
};

}  // namespace iptv::decoder

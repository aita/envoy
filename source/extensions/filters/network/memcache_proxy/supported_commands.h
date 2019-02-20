#pragma once

#include <string>
#include <vector>

#include "common/common/macros.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MemcacheProxy {

struct SupportedCommands {
  /**
   * @return commands which hash to a single server
   */
  static const std::vector<std::string>& simpleCommands() {
    CONSTRUCT_ON_FIRST_USE(std::vector<std::string>, "add", "append", "cas", "decr", "incr",
                           "prepend", "set", "replace");
  }

  /**
   * @return commands which are sent to multiple servers and coalesced by summing the responses
   */
  static const std::vector<std::string>& hashMultipleSumResultCommands() {
    CONSTRUCT_ON_FIRST_USE(std::vector<std::string>, "delete", "touch");
  }

  /**
   * @return get command
   */
  static const std::string& get() { CONSTRUCT_ON_FIRST_USE(std::string, "get", "gets"); }
};

} // namespace MemcacheProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy

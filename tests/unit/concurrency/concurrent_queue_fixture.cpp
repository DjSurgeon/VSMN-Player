#include "concurrency/concurrent_queue_fixture.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace iptv::test::concurrency {

std::uint8_t payloadByte(std::size_t index) { return static_cast<std::uint8_t>(index * 31u + 7u); }

}  // namespace iptv::test::concurrency

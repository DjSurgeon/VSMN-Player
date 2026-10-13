#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include "concurrency/concurrent_queue_fixture.hpp"
#include "iptv/concurrency/concurrent_queue.hpp"

using iptv::ConcurrentQueue;
using iptv::test::concurrency::MoveOnlyBuffer;
using iptv::test::concurrency::payloadByte;

class ConcurrentQueueTest : public ::testing::Test {};

// Test 1: Basic Push and Pop
TEST_F(ConcurrentQueueTest, BasicPushPop) {
  ConcurrentQueue<int> q;
  EXPECT_TRUE(q.empty());

  q.push(42);
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(q.size(), 1);

  std::jthread worker([&](std::stop_token st) {
    auto val = q.pop(st);
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 42);
  });
  worker.join();
}

// Test 2: Move Semantics and "Corrupt Data"
TEST_F(ConcurrentQueueTest, MoveSemanticsAndCorruptData) {
  ConcurrentQueue<MoveOnlyBuffer> q;

  // We push a perfectly valid buffer
  q.push(MoveOnlyBuffer{"ValidSegment", false});

  // We push a wildly corrupt buffer. The queue MUST transport it as-is.
  // It is the Decoder's job to validate it later. The Queue is just a dumb, fast pipe.
  q.push(MoveOnlyBuffer{"#$^#%@!@", true});

  std::jthread worker([&](std::stop_token st) {
    auto val1 = q.pop(st);
    ASSERT_TRUE(val1.has_value());
    EXPECT_EQ(val1->data, "ValidSegment");
    EXPECT_FALSE(val1->corrupt);

    auto val2 = q.pop(st);
    ASSERT_TRUE(val2.has_value());
    EXPECT_EQ(val2->data, "#$^#%@!@");
    EXPECT_TRUE(val2->corrupt);  // The corruption transited perfectly
  });
  worker.join();
}

// Test 3: try_pop() Non-blocking
TEST_F(ConcurrentQueueTest, TryPop) {
  ConcurrentQueue<int> q;

  // Empty queue
  auto val = q.try_pop();
  EXPECT_FALSE(val.has_value());

  q.push(99);

  // Now it has something
  val = q.try_pop();
  EXPECT_TRUE(val.has_value());
  EXPECT_EQ(*val, 99);

  // Empty again
  val = q.try_pop();
  EXPECT_FALSE(val.has_value());
}

// Test 4: Zero-copy proof for large media buffers. A std::vector<std::uint8_t> payload keeps
// its heap address across push -> pop and across a second hop, which is only possible if the
// queue moves the buffer instead of copying its bytes. Any accidental copy (const& overload,
// by-value push without std::move, extra local round trip) changes the address and fails here.
TEST_F(ConcurrentQueueTest, ZeroCopyMoveSemantics) {
  ConcurrentQueue<std::vector<std::uint8_t>> q;

  std::vector<std::uint8_t> segment(iptv::test::concurrency::kSegmentBytes);
  for (std::size_t i = 0; i < segment.size(); ++i) {
    segment[i] = payloadByte(i);
  }

  const std::uint8_t* const payload_address = segment.data();
  ASSERT_NE(payload_address, nullptr);

  q.push(std::move(segment));
  EXPECT_EQ(q.size(), 1u);
  // Ownership left the caller entirely: moved, not duplicated.
  EXPECT_TRUE(segment.empty());

  std::optional<std::vector<std::uint8_t>> popped;
  std::jthread consumer([&](std::stop_token st) { popped = q.pop(st); });
  consumer.join();

  ASSERT_TRUE(popped.has_value());
  EXPECT_EQ(popped->size(), iptv::test::concurrency::kSegmentBytes);
  EXPECT_EQ(popped->data(), payload_address) << "blocking pop copied the payload";
  EXPECT_EQ(static_cast<int>((*popped)[0]), static_cast<int>(payloadByte(0)));
  EXPECT_EQ(static_cast<int>((*popped)[iptv::test::concurrency::kSegmentBytes - 1]),
            static_cast<int>(payloadByte(iptv::test::concurrency::kSegmentBytes - 1)));

  // Taking ownership from the optional must not reallocate either.
  std::vector<std::uint8_t> owned = std::move(*popped);
  EXPECT_EQ(owned.data(), payload_address);
  EXPECT_EQ(owned.size(), iptv::test::concurrency::kSegmentBytes);

  // Second hop through the queue, this time via try_pop(): same address, same bytes.
  q.push(std::move(owned));
  auto polled = q.try_pop();
  ASSERT_TRUE(polled.has_value());
  EXPECT_EQ(polled->data(), payload_address) << "try_pop() copied the payload";
  EXPECT_EQ(polled->size(), iptv::test::concurrency::kSegmentBytes);
  EXPECT_EQ(static_cast<int>((*polled)[iptv::test::concurrency::kSegmentBytes / 2]),
            static_cast<int>(payloadByte(iptv::test::concurrency::kSegmentBytes / 2)));
  EXPECT_TRUE(q.empty());

  // Small payloads keep the same guarantee, in both directions of the API.
  std::vector<std::uint8_t> atom{0xDE, 0xAD, 0xBE, 0xEF};
  const std::uint8_t* const atom_address = atom.data();
  q.push(std::move(atom));
  EXPECT_TRUE(atom.empty());

  auto atom_out = q.try_pop();
  ASSERT_TRUE(atom_out.has_value());
  EXPECT_EQ(atom_out->data(), atom_address);
  EXPECT_EQ(atom_out->size(), 4u);
  EXPECT_EQ(atom_out->at(0), 0xDE);
  EXPECT_EQ(atom_out->at(3), 0xEF);
  EXPECT_TRUE(q.empty());
}

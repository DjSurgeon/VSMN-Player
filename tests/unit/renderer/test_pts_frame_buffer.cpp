#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "iptv/decoder/decoded_frame.hpp"
#include "iptv/renderer/pts_frame_buffer.hpp"

using namespace iptv::decoder;
using namespace iptv::renderer;

DecodedFrame MakeVideoFrame(int64_t pts) {
  return DecodedFrame(
      FrameTiming{pts, 1000},
      VideoFrameGeometry{1920, 1080, PixelFormat::YUV420P, {1920, 960, 960, 0}},
      std::vector<uint8_t>{});
}

TEST(PtsFrameBufferTest, InOrderPop) {
  PtsFrameBuffer buffer(10);
  
  buffer.push(MakeVideoFrame(100));
  buffer.push(MakeVideoFrame(50));
  buffer.push(MakeVideoFrame(200));

  auto f1 = buffer.pop();
  EXPECT_EQ(f1->pts, 50);

  auto f2 = buffer.pop();
  EXPECT_EQ(f2->pts, 100);

  auto f3 = buffer.pop();
  EXPECT_EQ(f3->pts, 200);
}

TEST(PtsFrameBufferTest, StabilityIdenticalPts) {
  PtsFrameBuffer buffer(10);
  
  // Create frames with identical PTS
  auto frame1 = MakeVideoFrame(100);
  frame1.duration = 1; // Mark to identify
  
  auto frame2 = MakeVideoFrame(100);
  frame2.duration = 2;

  buffer.push(std::move(frame1));
  buffer.push(std::move(frame2));

  auto f1 = buffer.pop();
  EXPECT_EQ(f1->duration, 1);

  auto f2 = buffer.pop();
  EXPECT_EQ(f2->duration, 2);
}

TEST(PtsFrameBufferTest, WrapAround) {
  PtsFrameBuffer buffer(10);
  
  int64_t max_pts = (1LL << 33) * 100 / 9; // ~95443717688
  int64_t before_wrap = max_pts - 100;
  int64_t after_wrap = 50;
  
  buffer.push(MakeVideoFrame(after_wrap));
  buffer.push(MakeVideoFrame(before_wrap));

  // The before_wrap should pop first because after_wrap is actually LATER chronologically
  auto f1 = buffer.pop();
  EXPECT_EQ(f1->pts, before_wrap);

  auto f2 = buffer.pop();
  EXPECT_EQ(f2->pts, after_wrap);
}

TEST(PtsFrameBufferTest, PushBlocksWhenFull) {
  PtsFrameBuffer buffer(2);
  
  buffer.push(MakeVideoFrame(1));
  buffer.push(MakeVideoFrame(2));

  bool pushed = false;
  std::thread t([&]() {
    pushed = buffer.push(MakeVideoFrame(3));
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto popped = buffer.pop();
  EXPECT_EQ(popped->pts, 1);

  t.join();
  EXPECT_TRUE(pushed);
  
  popped = buffer.pop();
  EXPECT_EQ(popped->pts, 2);
  
  popped = buffer.pop();
  EXPECT_EQ(popped->pts, 3);
}

TEST(PtsFrameBufferTest, AbortWakesPushAndPop) {
  PtsFrameBuffer buffer_push(1);
  PtsFrameBuffer buffer_pop(1);
  
  // Fill buffer_push so push blocks
  buffer_push.push(MakeVideoFrame(1));

  bool push_result = true;
  std::thread t_push([&]() {
    push_result = buffer_push.push(MakeVideoFrame(2));
  });

  bool pop_result = true;
  std::thread t_pop([&]() {
    auto res = buffer_pop.pop(); // blocks because empty
    if (!res) {
      pop_result = false;
    }
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  buffer_push.abort();
  buffer_pop.abort();

  t_push.join();
  t_pop.join();

  EXPECT_FALSE(push_result);
  EXPECT_FALSE(pop_result);
}

TEST(PtsFrameBufferTest, ConcurrencyStress) {
  PtsFrameBuffer buffer(50);
  std::atomic<int> pushed_count{0};
  std::atomic<int> popped_count{0};

  std::vector<std::thread> producers;
  for (int i = 0; i < 4; ++i) {
    producers.emplace_back([&, i]() {
      for (int j = 0; j < 100; ++j) {
        if (buffer.push(MakeVideoFrame(i * 1000 + j))) {
          pushed_count++;
        }
      }
    });
  }

  std::vector<std::thread> consumers;
  for (int i = 0; i < 4; ++i) {
    consumers.emplace_back([&]() {
      for (int j = 0; j < 100; ++j) {
        auto f = buffer.pop();
        if (f) {
          popped_count++;
        }
      }
    });
  }

  for (auto& t : producers) t.join();
  for (auto& t : consumers) t.join();

  EXPECT_EQ(pushed_count, 400);
  EXPECT_EQ(popped_count, 400);
}

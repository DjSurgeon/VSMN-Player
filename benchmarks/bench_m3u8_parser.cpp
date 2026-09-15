#include <benchmark/benchmark.h>

#include <atomic>
#include <cstdlib>
#include <string>

#include "iptv/manifest/playlist.hpp"
#include "iptv/manifest/playlist_m3u8_parser.hpp"

// Cache-line efficiency test (Data-Oriented Design)
// 64 bytes is the typical size of an L1 cache line on x86_64/ARM64.
// If this assertion fails, the struct is too bloated or poorly packed.
static_assert(sizeof(iptv::manifest::MediaSegmentRef) <= 64,
              "MediaSegmentRef must fit within a single 64-byte cache line");

// Global allocation tracker to prove Zero-Heap Copies
std::atomic<size_t> g_allocations{0};
std::atomic<bool> g_track_allocations{false};

void* operator new(std::size_t size) {
  if (g_track_allocations.load(std::memory_order_relaxed)) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
  }
  void* ptr = std::malloc(size);
  if (!ptr)
    throw std::bad_alloc{};
  return ptr;
}

void operator delete(void* ptr) noexcept { std::free(ptr); }

void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }

static std::string generate_mock_manifest(size_t lines) {
  std::string manifest;
  // Each segment takes 2 lines: #EXTINF + URI
  size_t segments = lines / 2;
  manifest.reserve(segments * 30 + 100);

  manifest += "#EXTM3U\n#EXT-X-TARGETDURATION:10\n";
  for (size_t i = 0; i < segments; ++i) {
    manifest += "#EXTINF:10.0,\n";
    manifest += "segment_" + std::to_string(i) + ".ts\n";
  }
  manifest += "#EXT-X-ENDLIST\n";
  return manifest;
}

static void BM_ParseLargePlaylist(benchmark::State& state) {
  // Generate outside the timing loop
  std::string large_manifest = generate_mock_manifest(state.range(0));

  for (auto _ : state) {
    // Start tracking allocations specifically during the parse phase
    g_allocations.store(0, std::memory_order_relaxed);
    g_track_allocations.store(true, std::memory_order_relaxed);

    auto result = iptv::manifest::M3u8Parser::parse({large_manifest, "http://cdn.example.com/"});

    g_track_allocations.store(false, std::memory_order_relaxed);

    benchmark::DoNotOptimize(result);

    // Report allocations as a custom counter
    state.counters["Allocations"] =
        benchmark::Counter(g_allocations.load(std::memory_order_relaxed),
                           benchmark::Counter::kDefaults, benchmark::Counter::OneK::kIs1000);
  }
  // Measure Throughput in MB/s
  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(large_manifest.size()));
}

// Test with 10k, 50k, and 100k lines
BENCHMARK(BM_ParseLargePlaylist)->RangeMultiplier(5)->Range(10000, 250000);

BENCHMARK_MAIN();

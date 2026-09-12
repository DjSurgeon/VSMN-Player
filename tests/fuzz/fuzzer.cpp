#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "iptv/manifest/playlist_m3u8_parser.hpp"

using namespace iptv::manifest;

const std::string base_manifest =
    "#EXTM3U\n"
    "#EXT-X-TARGETDURATION:10\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=3000000,RESOLUTION=1280x720,FRAME-RATE=60.0,CODECS=\"avc1.4d401f,"
    "mp4a.40.2\"\n"
    "720p.m3u8\n"
    "#EXTINF:10.0,\n"
    "seg1.ts\n"
    "#EXTINF:10.0,\n"
    "seg2.ts\n"
    "#EXT-X-ENDLIST\n";

int main(int argc, char** argv) {
  size_t iterations = 50000;  // Default 50k mutations
  if (argc > 1) {
    iterations = std::stoull(argv[1]);
  }

  std::mt19937 rng(42);  // Fixed seed
  std::uniform_int_distribution<size_t> pos_dist(0, base_manifest.size() - 1);
  std::uniform_int_distribution<int> char_dist(0, 255);
  std::uniform_int_distribution<int> mut_dist(1, 10);

  M3u8Parser parser;

  for (size_t i = 0; i < iterations; ++i) {
    std::string mutated = base_manifest;

    // Mutate up to 10 bytes randomly
    int mutations = mut_dist(rng);
    for (int m = 0; m < mutations; ++m) {
      mutated[pos_dist(rng)] = static_cast<char>(char_dist(rng));
    }

    // Random truncations 10% of the time
    if (rng() % 10 == 0) {
      mutated.resize(pos_dist(rng));
    }

    // Random null bytes injection 10% of the time
    if (rng() % 10 == 0) {
      mutated[pos_dist(rng)] = '\0';
    }

    auto result = parser.parse(mutated, "http://fuzz.example.com/");
    (void)result;
  }

  std::cout << "SUCCESS: Parser survived " << iterations
            << " mutated manifests without crashing.\n";
  return 0;
}

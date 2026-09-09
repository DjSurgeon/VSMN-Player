#include <gtest/gtest.h>
#include "iptv/network/http_response.hpp"

using namespace iptv::network;

TEST(HttpResponseTest, ConstructorPreallocatesMemory) {
    std::size_t expected_size = 1024 * 1024; // 1MB
    HttpResponse response(HttpStatusCode::Ok, expected_size);
    
    EXPECT_EQ(response.getStatusCode(), HttpStatusCode::Ok);
    EXPECT_GE(response.getBody().capacity(), expected_size);
    EXPECT_EQ(response.getBytesDownloaded(), 0);
}

TEST(HttpResponseTest, AppendToBodyWorksCorrectly) {
    HttpResponse response(HttpStatusCode::Ok, 100);
    
    std::vector<uint8_t> chunk1 = {0x01, 0x02, 0x03};
    std::vector<uint8_t> chunk2 = {0x04, 0x05};
    
    response.appendToBody(chunk1.data(), chunk1.size());
    EXPECT_EQ(response.getBytesDownloaded(), 3);
    
    response.appendToBody(chunk2.data(), chunk2.size());
    EXPECT_EQ(response.getBytesDownloaded(), 5);
    
    const auto& body = response.getBody();
    EXPECT_EQ(body.size(), 5);
    EXPECT_EQ(body[0], 0x01);
    EXPECT_EQ(body[4], 0x05);
}

TEST(HttpResponseTest, MoveSemanticsAreEnforced) {
    HttpResponse response1(HttpStatusCode::NotFound);
    std::vector<uint8_t> chunk = {0xDE, 0xAD, 0xBE, 0xEF};
    response1.appendToBody(chunk.data(), chunk.size());
    
    // std::move transfers ownership without copying
    HttpResponse response2 = std::move(response1);
    
    EXPECT_EQ(response2.getStatusCode(), HttpStatusCode::NotFound);
    EXPECT_EQ(response2.getBytesDownloaded(), 4);
    EXPECT_EQ(response2.getBody().size(), 4);
    EXPECT_EQ(response2.getBody().size(), 4);
}

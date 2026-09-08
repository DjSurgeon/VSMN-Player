#include <gtest/gtest.h>
#include "iptv/network/http_response.hpp"

using namespace iptv::network;

class HttpResponseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Shared setup logic if needed
    }
};

TEST_F(HttpResponseTest, ConstructorPreallocatesMemory) {
    std::size_t expected_size = 1024 * 1024; // 1MB
    HttpResponse response(HttpStatusCode::Ok, expected_size);
    
    EXPECT_EQ(response.getStatusCode(), HttpStatusCode::Ok);
    EXPECT_GE(response.getBody().capacity(), expected_size);
    EXPECT_EQ(response.getBytesDownloaded(), 0);
}

TEST_F(HttpResponseTest, AppendToBodyWorksCorrectly) {
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

TEST_F(HttpResponseTest, MoveSemanticsAreEnforced) {
    HttpResponse response1(HttpStatusCode::NotFound);
    std::vector<uint8_t> chunk = {0xDE, 0xAD, 0xBE, 0xEF};
    response1.appendToBody(chunk.data(), chunk.size());
    
    // std::move transfers ownership without copying
    HttpResponse response2 = std::move(response1);
    
    EXPECT_EQ(response2.getStatusCode(), HttpStatusCode::NotFound);
    EXPECT_EQ(response2.getBytesDownloaded(), 4);
    EXPECT_EQ(response2.getBody().size(), 4);
    
    // Note: accessing response1 after move is technically valid in standard C++ 
    // for vectors (leaves them in a valid but unspecified state), but the capacity/size is typically 0.
    EXPECT_EQ(response1.getBody().size(), 0);
}

TEST_F(HttpResponseTest, BuilderCreatesValidResponse) {
    auto response = HttpResponseBuilder()
        .withStatusCode(HttpStatusCode::MovedPermanently)
        .withLatency(std::chrono::milliseconds(42))
        .build();
        
    EXPECT_EQ(response.getStatusCode(), HttpStatusCode::MovedPermanently);
    EXPECT_EQ(response.getLatency().count(), 42);
}

TEST_F(HttpResponseTest, BuilderThrowsOnMissingStatusCode) {
    HttpResponseBuilder builder;
    // Missing status code
    EXPECT_THROW(builder.build(), std::invalid_argument);
}

TEST_F(HttpResponseTest, MetadataIsExtensible) {
    HttpResponse::Metadata meta;
    meta.retry_count = 3;
    meta.headers["Content-Type"] = "video/mp2t";
    meta.was_redirected = true;
    
    auto response = HttpResponseBuilder()
        .withStatusCode(HttpStatusCode::PartialContent)
        .withMetadata(std::move(meta))
        .build();
        
    EXPECT_EQ(response.getMetadata().retry_count, 3);
    EXPECT_EQ(response.getMetadata().headers.at("Content-Type"), "video/mp2t");
    EXPECT_TRUE(response.getMetadata().was_redirected);
}

TEST_F(HttpResponseTest, PlaceholderHashesWork) {
    HttpResponse response(HttpStatusCode::Ok);
    EXPECT_EQ(response.computeBodyHash(), "placeholder_hash");
    EXPECT_TRUE(response.verifyBodyHash("placeholder_hash"));
    EXPECT_FALSE(response.verifyBodyHash("invalid_hash"));
}

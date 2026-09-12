#include <gtest/gtest.h>

#include "iptv/manifest/playlist_attribute_scanner.hpp"

namespace iptv::manifest {
namespace {

// In order to test private methods using FRIEND_TEST, we need a test fixture
// matching the first argument of the FRIEND_TEST macro.
class AttributeScannerTest : public ::testing::Test {};

class TestableAttributeScanner : public AttributeScanner {
 public:
  using AttributeScanner::AttributeScanner;
  using AttributeScanner::cursor_;
  using AttributeScanner::readKey;
  using AttributeScanner::readValue;
  using AttributeScanner::skipComma;
  using AttributeScanner::stripQuotes;
};

TEST_F(AttributeScannerTest, ReadKey) {
  // Test extracting a key normally
  TestableAttributeScanner scanner("BANDWIDTH=8000");
  EXPECT_EQ(scanner.readKey(), "BANDWIDTH");
  EXPECT_EQ(scanner.cursor_, 10);  // After '='

  // Test extracting a key when there's no '='
  TestableAttributeScanner scanner_malformed("BANDWIDTH");
  EXPECT_EQ(scanner_malformed.readKey(), "");
  EXPECT_EQ(scanner_malformed.cursor_, 9);  // Jumped to end
}

TEST_F(AttributeScannerTest, ReadValue) {
  // Simple unquoted value
  TestableAttributeScanner scanner1("BANDWIDTH=8000,NEXT=...");
  scanner1.readKey();  // Advance past key
  EXPECT_EQ(scanner1.readValue(), "8000");

  // Quoted value without commas
  TestableAttributeScanner scanner2("CODECS=\"avc1.4d401e\"");
  scanner2.readKey();
  EXPECT_EQ(scanner2.readValue(), "\"avc1.4d401e\"");

  // Quoted value containing commas
  TestableAttributeScanner scanner3("CODECS=\"avc1.4d401e,mp4a.40.2\",RESOLUTION=1920x1080");
  scanner3.readKey();
  EXPECT_EQ(scanner3.readValue(), "\"avc1.4d401e,mp4a.40.2\"");
}

TEST_F(AttributeScannerTest, StripQuotes) {
  TestableAttributeScanner scanner("");
  EXPECT_EQ(scanner.stripQuotes("\"Hello\""), "Hello");
  EXPECT_EQ(scanner.stripQuotes("\"\""), "");  // Empty quotes
  EXPECT_EQ(scanner.stripQuotes("Unquoted"), "Unquoted");
  EXPECT_EQ(scanner.stripQuotes("\"Unclosed"), "\"Unclosed");
}

TEST_F(AttributeScannerTest, SkipComma) {
  TestableAttributeScanner scanner(",NEXT=1");
  scanner.skipComma();
  EXPECT_EQ(scanner.cursor_, 1);

  TestableAttributeScanner scanner_no_comma("NEXT=1");
  scanner_no_comma.skipComma();
  EXPECT_EQ(scanner_no_comma.cursor_, 0);
}

TEST_F(AttributeScannerTest, NextPipelineFull) {
  // Integration test of the full `next()` method
  AttributeScanner scanner(
      "CODECS=\"avc1.4d401e,mp4a.40.2\",BANDWIDTH=5000000,RESOLUTION=1920x1080");

  auto first = scanner.next();
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->key, "CODECS");
  EXPECT_EQ(first->value, "avc1.4d401e,mp4a.40.2");  // Notice quotes are stripped!

  auto second = scanner.next();
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(second->key, "BANDWIDTH");
  EXPECT_EQ(second->value, "5000000");

  auto third = scanner.next();
  ASSERT_TRUE(third.has_value());
  EXPECT_EQ(third->key, "RESOLUTION");
  EXPECT_EQ(third->value, "1920x1080");

  auto fourth = scanner.next();
  EXPECT_FALSE(fourth.has_value());
}

}  // namespace
}  // namespace iptv::manifest

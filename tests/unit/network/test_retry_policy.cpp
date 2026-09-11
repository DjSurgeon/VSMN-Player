#include <gtest/gtest.h>
#include "iptv/network/http_retry_policy.hpp"

using namespace iptv::network;

TEST(RetryPolicyTest, DefaultValuesAreIndustryStandard) {
    RetryPolicy policy;
    
    EXPECT_EQ(policy.max_retries, 3);
    EXPECT_EQ(policy.initial_delay.count(), 1000);
    EXPECT_EQ(policy.max_delay.count(), 15000);
    EXPECT_EQ(policy.strategy, BackoffStrategy::Exponential);
}

TEST(RetryPolicyTest, CustomValuesAreRespected) {
    RetryPolicy policy{
        5, 
        std::chrono::milliseconds(500), 
        std::chrono::milliseconds(5000), 
        BackoffStrategy::Fixed
    };
    
    EXPECT_EQ(policy.max_retries, 5);
    EXPECT_EQ(policy.initial_delay.count(), 500);
    EXPECT_EQ(policy.max_delay.count(), 5000);
    EXPECT_EQ(policy.strategy, BackoffStrategy::Fixed);
}

#include <gtest/gtest.h>

#include <string>
using std::string;

const char *actualValTrue  = "hello gtest";
const char *actualValFalse = "hello world";
const char *expectVal      = "hello gtest";

TEST(StrCompare, CStrEqual) {
    EXPECT_STREQ(expectVal, actualValTrue);
}

TEST(StrCompare, CStrNotEqual) {
    EXPECT_STREQ(expectVal, actualValFalse);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv); 
    return RUN_ALL_TESTS();
}

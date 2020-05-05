#include "gtest/gtest.h"
#include "sum_of_two.h"

namespace leetcode::tests
{

TEST(test_of_two, invalid_imput)
{
    std::vector<int> test_data = {};
    std::vector<int> expected = {};
    EXPECT_EQ(expected, sum_of_two(test_data, 0));

    test_data = {1, 2, 3};
    expected = {};
    EXPECT_EQ(expected, sum_of_two(test_data, 0));
}

TEST(test_of_two, test_valid_input)
{
    std::vector<int> test_data = { 1, 2, 4 };
    std::vector<int> expected = { 0, 1 };
    EXPECT_EQ(expected, sum_of_two(test_data, 3));

    test_data = { 1, 2, 4, 6, 9 };
    expected = { 1, 4 };
    EXPECT_EQ(expected, sum_of_two(test_data, 11));
}

} // namespace leetcode::tests


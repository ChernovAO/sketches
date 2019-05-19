#include "stdafx.h"
#include "gtest/gtest.h"

#include "fibonacci.h"

struct FibonacciTests : public testing::Test
{

};

TEST_F(FibonacciTests, sequence)
{
	ASSERT_EQ(0, fibonacci(0));
	ASSERT_EQ(1, fibonacci(1));
	ASSERT_EQ(1, fibonacci(2));
	ASSERT_EQ(2, fibonacci(3));
	ASSERT_EQ(3, fibonacci(4));
	ASSERT_EQ(5, fibonacci(5));
	ASSERT_EQ(8, fibonacci(6));
}

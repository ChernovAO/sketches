#include "gtest/gtest.h"

#include "fibonacci.h"

TEST(fibonacci)
{
	ASSERT_EQ(0, fibonacci(0));
	ASSERT_EQ(1, fibonacci(1));
	ASSERT_EQ(1, fibonacci(2));
	ASSERT_EQ(2, fibonacci(3));
	ASSERT_EQ(3, fibonacci(4));
	ASSERT_EQ(5, fibonacci(5));
	ASSERT_EQ(8, fibonacci(6));
}

TEST(fibonacci_last_number)
{
	ASSERT_EQ(0, fibonacci_last_number(0));
	ASSERT_EQ(1, fibonacci_last_number(1));
	ASSERT_EQ(1, fibonacci_last_number(2));
	ASSERT_EQ(2, fibonacci_last_number(3));
	ASSERT_EQ(3, fibonacci_last_number(4));
	ASSERT_EQ(5, fibonacci_last_number(5));
	ASSERT_EQ(8, fibonacci_last_number(6));
	ASSERT_EQ(3, fibonacci_last_number(7));
	ASSERT_EQ(2, fibonacci_last_number(317457));
}

TEST(fibonacci_rem_10)
{
	ASSERT_EQ(0, fibonacci_rem(0, 10));
	ASSERT_EQ(1, fibonacci_rem(1, 10));
	ASSERT_EQ(1, fibonacci_rem(2, 10));
	ASSERT_EQ(2, fibonacci_rem(3, 10));
	ASSERT_EQ(3, fibonacci_rem(4, 10));
	ASSERT_EQ(5, fibonacci_rem(5, 10));
	ASSERT_EQ(8, fibonacci_rem(6, 10));
	ASSERT_EQ(3, fibonacci_rem(7, 10));
	ASSERT_EQ(2, fibonacci_rem(317457, 10));
}

TEST(fibonacci_rem_2)
{
	ASSERT_EQ(0, fibonacci_rem(0, 2));
	ASSERT_EQ(1, fibonacci_rem(1, 2));
	ASSERT_EQ(1, fibonacci_rem(2, 2));
	ASSERT_EQ(0, fibonacci_rem(3, 2));
	ASSERT_EQ(1, fibonacci_rem(4, 2));
	ASSERT_EQ(1, fibonacci_rem(5, 2));
	ASSERT_EQ(0, fibonacci_rem(6, 2));
	ASSERT_EQ(1, fibonacci_rem(7, 2));
	ASSERT_EQ(1, fibonacci_rem(10, 2));
	ASSERT_EQ(0, fibonacci_rem(317457, 2));
}
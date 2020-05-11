#include "gtest/gtest.h"
#include "zig_zag_conv.h"

namespace leetcode::tests
{
	TEST(zig_zag_conv, valid_input)
	{
		std::string input = "PAHNAPLSIIGYIR";
		std::string expected_output = "PAHNAPLSIIGYIR";
		ASSERT_EQ(expected_output, convert(input, 1));
	}
} // namespace leetcode::tests

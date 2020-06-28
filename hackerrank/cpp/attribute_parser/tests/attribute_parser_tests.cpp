#include "attribute_parser.h"

#include "gtest/gtest.h"

#include <sstream>

namespace hackerrank::attribute_parser::tests
{
TEST(node_parser, simple_parser)
{
    std::stringstream source = {};
    size_t linesCount = 0;
    Node expected = {};
    auto result = parseInput(linesCount, source);
    ASSERT_EQ(expected, result);

    source.str("<tag1>\r\n</tag1>");
    linesCount = 2;
    expected.children_.insert({"tag1", {"tag1", {}, {}}});
    result = parseInput(linesCount, source);
    ASSERT_EQ(expected, result);
}
} // namespace hackerrank::attribute_parser::tests

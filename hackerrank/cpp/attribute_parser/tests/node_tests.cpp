#include "attribute_parser.h"

#include "gtest/gtest.h"

#include <sstream>

namespace hackerrank::attribute_parser::tests
{
TEST(node_parser, node_ostream_operator_test)
{
    std::stringstream actual;
    Node node = {};
    actual << node;
    std::string expected = "{}";
    ASSERT_EQ(expected, actual.str());

    node.name_ = "test_name";
    actual.str(std::string());
    actual << node;
    expected = "{test_name}";
    ASSERT_EQ(expected, actual.str());

    actual.str(std::string());
    node.attributes_.insert({ "attribute", "value" });
    actual << node;
    expected = "{test_name : [(attribute:value)]}";
    ASSERT_EQ(expected, actual.str());

    actual.str(std::string());
    node.attributes_.insert({ "one_more_attribute", "another_value" });
    actual << node;
    expected = "{test_name : [(attribute:value)(one_more_attribute:another_value)]}";
    ASSERT_EQ(expected, actual.str());

    actual.str(std::string());
    Node child;
    child.name_ = "child_node";
    node.children_.insert({ child.name_, child });
    actual << node;
    expected = "{test_name : [(attribute:value)(one_more_attribute:another_value)] : {{child_node}}}";
    ASSERT_EQ(expected, actual.str());
}
} // namespace hackerrank::attribute_parser::tests

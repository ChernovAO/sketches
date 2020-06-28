#pragma once

#include <iostream>
#include <unordered_map>
#include <string>

namespace hackerrank::attribute_parser
{
    struct Node
    {
        using Attributes = std::unordered_map<std::string, std::string>;
        std::string name_;
        std::unordered_map<std::string, Node> children_;
        Attributes attributes_;
    };

    bool operator == (const Node& lhs, const Node& rhs);
    std::ostream& operator << (std::ostream& ost, const Node& node);

} // namespace hackerrank::attribute_parser

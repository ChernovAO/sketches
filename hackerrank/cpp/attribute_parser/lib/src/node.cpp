#include "node.h"

namespace hackerrank::attribute_parser
{
    bool operator == (const Node& lhs, const Node& rhs)
    {
        return lhs.name_ == rhs.name_
            && lhs.attributes_ == rhs.attributes_
            && lhs.children_ == rhs.children_;
    }

    std::ostream& operator << (std::ostream& ost, const Node& node)
    {
        ost << "{" << node.name_;

        if (!node.attributes_.empty())
        {
            ost << " : [";
            for (const auto& n : node.attributes_)
            {
                ost << "(" << n.first << ":" << n.second << ")";
            }
            ost << "]";
        }

        if (!node.children_.empty())
        {
            ost << " : {";
            for (const auto& n : node.children_)
            {
                ost << n.second;
            }
            ost << "}";
        }

        ost << "}";

        return ost;
    }
} // namespace hackerrank::attribute_parser

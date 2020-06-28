#include "attribute_parser.h"

#include <string>
#include <string_view>
#include <stdexcept>

namespace hackerrank::attribute_parser
{
    [[nodiscard]] std::string_view getName(std::string_view source);

    Node parseInput(size_t linesCount, std::istream& stream)
    {
        Node root = {};

        Node active;
        Node& parent = root;

        std::string source;
        for (size_t i = 0; i < linesCount; ++i)
        {
            std::getline(stream, source);
            auto it = source.begin();
            auto itEnd = source.rbegin();
            if ('\r' == *itEnd)
            {
                ++itEnd;
            }

            if (*it != '<' || *itEnd != '>')
            {
                return {};
            }

            if (active.name_.empty())
            {
                active.name_ = getName(source);
            }

            if (*(++it) == '/')
            {
                const auto name = getName(source);
                if (name == active.name_)
                {
                    parent.children_.insert({ active.name_, active });
                }
                active = Node{};
            }
        }

        return root;
    }

    std::string_view getName(std::string_view source)
    {
        const auto start = source.find_last_of("</") + 1;
        const auto end = source.find_first_of(">");
        return source.substr(start, end - start);
    }

} // namespace hackerrank::attribute_parser

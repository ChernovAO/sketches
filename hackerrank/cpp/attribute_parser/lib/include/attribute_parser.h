#pragma once

#include "node.h"

#include <iostream>

namespace hackerrank::attribute_parser
{
    Node parseInput(size_t linesCount, std::istream& stream);
} // namespace hackerrank::attribute_parser

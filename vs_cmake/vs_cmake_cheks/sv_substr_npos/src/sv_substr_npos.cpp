#include <iomanip>
#include <iostream>
#include <string_view>

int main(int argc, char** argv)
{
    std::string_view example = "example";
    const auto nposStr = example.substr(std::string_view::npos, 1);

    std::string_view result = nposStr.empty() ? "nposStr is empty"
                                              : "nposStr is not empty";
    std::cout << result << std::endl;

    return 0;
}
#include <iostream>

int main(int argc, char** argv)
{
    auto i = 1 | ~10 << 1 & 010;
    std::cout << i << std::endl;
    return 0;
}

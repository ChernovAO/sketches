#include <iostream>

int main(int argc, char** argv)
{
    int cycleNumber = 3;
    int result = 0;
    for (int i = 0; i < cycleNumber; ++i)
    {
        int tmp = 0;
        std::cin >> tmp;
        result += tmp;
    }

    std::cout << result;
    return 0;
}


#include <iostream>
#include <iomanip>
#include <string_view>
#include <string>

int main(int argc, char** argv)
{
    std::string_view m_ticker("M");
    std::string ticker;
    ticker += m_ticker.front();
    std::cout << "2: " << ticker << std::endl;

    return 0;
}

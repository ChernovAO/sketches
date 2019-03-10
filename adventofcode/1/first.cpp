// --- Day 1: Chronal Calibration ---
#include <filesystem>
#include <iostream>
#include <iomanip>

int main(int argc, char** argv)
{
    if(argc != 2)
    {
        std::cout << "Mandatory parameter missed. \r\n"
                  << "Usage: " << argv[0] << " <data_source>"
                  << std::endl;
        return 1;
    }

    return 0;
}

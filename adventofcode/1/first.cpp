// --- Day 1: Chronal Calibration ---
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <vector>
#include <numeric>
#include <fstream>

std::vector<int> get_data(const std::filesystem::path& data_source)
{
    std::vector<int> result;
    std::ifstream source(data_source, std::ios::in);
    while(source.good())
    {
        int tmp = 0;
        source >> tmp;
        result.push_back(tmp);
    }

    return result;
}

int main(int argc, char** argv)
{
    if(argc != 2)
    {
        std::cout << "Mandatory parameter missed. \r\n"
                  << "Usage: " << argv[0] << " <data_source>"
                  << std::endl;
        return 1;
    }

    if(not std::filesystem::exists(argv[1]))
    {
        std::cout << "File '" << argv[1] << "' isn't exists" << std::endl;
        return 2;
    }
    
    const auto data = get_data(argv[1]);
    std::cout << "Result: " << std::accumulate(data.begin(), data.end(), 0)
              << std::endl;

    return 0;
}

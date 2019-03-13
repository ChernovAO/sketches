// --- Day 1: Chronal Calibration ---
// --- Part Two ---

#include <filesystem>
#include <iostream>
#include <iomanip>
#include <vector>
#include <numeric>
#include <fstream>
#include <set>

std::vector<int> get_data(const std::filesystem::path& data_source)
{
    std::vector<int> result;
    std::ifstream source(data_source, std::ios::in);
    for(;;)
    {
        int tmp = 0;
        source >> tmp;
        if (source.good())
        {
            result.push_back(tmp);
            std::cout << "read: " << tmp << std::endl;
        }
        else
        {
            break;
        }
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
    std::set<int> frequencies;
    frequencies.insert(0);
    int frequency = 0;
    for (const auto item : data)
    {
        std::cout << "f: " << frequency << " i: " << item << std::endl;
        frequency += item;
        const auto it = frequencies.insert(frequency);
        if (not it.second)
        {
            std::cout << "Result: " << frequency << std::endl;
            return 0;
        }
    }

    std::cout << "Resulti wasn't finded. Last frequency is: " << frequency << std::endl;
    return 0;
}

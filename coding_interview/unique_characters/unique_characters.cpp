/// @brief Implement an algorithm to determine if a string has all
///        unique characters.
///        What if you can not use additional data structures?

#include <string>
#include <iostream>
#include <iomanip>
#include <unordered_set>
#include <algorithm>

bool is_unique_chars( const std::string& tested )
{
    std::unordered_set< char > set;
    for( auto c : tested )
    {
        if ( set.find( c ) != set.end() )
        {
            return false;
        }
        set.insert( c );
    }

    return true;
}

bool is_unique_chars_no_memmory( std::string& tested )
{
    std::sort( tested.begin(), tested.end() );
    char accum = *( tested.begin() );
    for ( std::string::iterator it = tested.begin() + 1;
            it != tested.end(); ++it )
    {
        if ( accum == *it )
        {
            return false;
        }
    }

    return true;
}

namespace
{
    const char* unique = "abcdef";
    const char* not_unique = "abcdefa";
}

int main()
{
    if ( !is_unique_chars( unique ) )
    {
        std::cout << "Error" << std::endl;
        return 1;
    }

    std::string tmp( unique );
    if ( !is_unique_chars_no_memmory( tmp ) )
    {
        std::cout << "Error" << std::endl;
        return 1;
    }

    return 0;
}

/// @brief Write code to reverse a C-Style String.
///        (C-String means that “abcd” is represented as
///        five characters, including the null character.)

#include <iostream>
#include <iomanip>
#include <string.h>

const char* test_str = "abcdef";

void print_reversed( const char* str )
{
    std::cout << str << std::endl; 
    size_t str_size = strlen( str ); // O(n)
    size_t full_size = str_size + 1;
    char* new_str = new char[ full_size ];
    for ( size_t i = 0; i <= str_size; --i )
    {
        *( new_str + i ) = *( str + str_size - i );
    }
    *( new_str + full_size ) = 0;

    std::cout << new_str << std::endl; 
}


int main( int argc, char** argv )
{
    print_reversed( test_str );
    return 0;
}

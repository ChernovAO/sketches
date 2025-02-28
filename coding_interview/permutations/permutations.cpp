// @brief Write a method in C++ that will find and print out all
//        the possible combinations (or “permutations”) of the characters
//        in a string. 
//        So, if the method is given the string “dog” as input,
//        then it will print out the strings “god”, “gdo”, “odg”,
//        “ogd”, “dgo”, and “dog” – since these are all of the possible
//        permutations of the string “dog”. 
//        Even if a string has characters repeated, each character should
//        be treated as a distinct character – so if the string “xxx” is
//        input then your method will just print out “xxx” 6 times.
//
//        Это такая бенчмарковая задача
//        Плюс иногда говорят, что строки не только такой длины, а всех.
//        Т.е. и “d” и “o”  и “od” тоже валидные

#include <vector>
#include <string>
#include <iterator>
#include <algorithm>
#include <iostream>
#include <iomanip>

using std::vector;
using std::string;
using std::back_inserter;
using std::cout;
using std::endl;

typedef vector< string > permutations;

/// @brief получение перестновок
permutations get_permutations( const string& str )
{
    permutations ret_val;
    if ( str.size() == 1 )
    {
        ret_val.push_back( str );
        return ret_val;
    }

    for( string::const_iterator it = str.begin(); it != str.end(); ++it )
    {
        string tmp;
        std::copy( str.begin(), it, back_inserter( tmp ) );
        std::copy( it + 1, str.end(), back_inserter( tmp ) );

        auto perms = get_permutations( tmp );
        for ( const auto& perm : perms )
        {
            string ret_perm = *it + perm;
            ret_val.push_back( ret_perm );
        }
    }

    return ret_val;
}

void print_permutations( const string& str )
{
    auto perms = get_permutations( str );

    for ( const auto& perm : perms )
    {
        cout << perm << endl;
    }
}

int main( int argc, char** argv )
{
    print_permutations( "abcd" );
    return 0;
}

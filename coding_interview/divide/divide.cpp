// @brief You are given two integer numbers.
//        Divide one by another without using division operator.
//        If you have a period in the fraction part, output it inside {}.
//        Example:
//        Input: 23526, 675
//        Output: 34,85{3}
//
//        Input: 444, 13
//        Output: 34,{153846}

#include <iostream>
#include <iomanip>
#include <stdlib.h>

using std::cout;
using std::endl;
using std::strtol;

const char* ussage = "Ussage: divide a b";

void divide( long int a,
             long int b,
             long int& integer,
             long int& fractional )
{
    if ( a == b )
    {
        integer = a;
        fractional = 0;
        return;
    }
}

int main( int argc, char** argv )
{
    long int a = 0;
    long int b = 0;
    if ( argc != 3 )
    {
        cout << ussage << endl;;
    }
    else
    {
        a = strtol( *(argv + 1), nullptr, 0 );
        b = strtol( *(argv + 2), nullptr, 0 );
    }

    cout << "a = " << a << " b = " << b << endl; 


    return 0;
}

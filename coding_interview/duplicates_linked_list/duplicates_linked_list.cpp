// @brief Write code to remove duplicates from an unsorted linked list.
//        FOLLOW UP
//        How would you solve this problem if a temporary buffer
//        is not allowed?

#include <unordered_set>
#include <iostream>
#include <iomanip>
#include <list>
#include <vector>

using std::unordered_set;
using std::cout;
using std::endl;
using std::list;
using std::vector;

template < typename T >
void remove_duplicates( T& t )
{
    unordered_set< typename T::value_type > buffer;
    vector< typename T::iterator > to_erase;

    for ( typename T::iterator it = t.begin(); it != t.end(); ++it )
    {
        if ( buffer.end() == buffer.find( *it ) )
        {
            buffer.insert( *it );
        }
        else
        {
            to_erase.push_back( it );
        }
    }
    
    for ( auto to_erase_it : to_erase )
    {
        t.erase( to_erase_it );
    }
}

template< typename T >
void print_stl_container( const T& t )
{
    for ( auto t_value : t )
    {
        cout << t_value << ' ';
    }

    cout << endl;
}


int main( int argc, char** argv )
{
    list< char > l = { 'a', 'b', 'c', 'c', 'b', 'd', 'a', 'a' };
    cout << "before: ";
    print_stl_container( l );

    remove_duplicates( l );
    cout << "after: ";
    print_stl_container( l );

    return 0;
}


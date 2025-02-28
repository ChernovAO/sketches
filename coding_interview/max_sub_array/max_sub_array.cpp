// @brief giving an array, find the sub-array with max sum
//        Example: [4, -1, 5, 3, -2, 1],
//        answer: 11, [4, -1, 5, 3] not 8 [5, 3]

#include <gtest/gtest.h>

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <vector>
#include <numeric>

using namespace std;

vector< int > get_max_sub_vector( const vector< int >& v )
{
    auto sub_v = get_sub_vector( v );
    int max_val = accumulate( sub_v.begin() );
    vector< vector< int > >::iterator max_v;

    for( auto& vector : sub_v )
    {
        auto sum = accumulate( vector ); 
        if ( sum > max_val )
        {
            max_val = sum;
        }
    }

    return v;
}

vector< vector< int > > get_sub_vector( const vector< int >& v )
{
    vector< vector< int > > ret_val;
    if( v.empty() )
    {
        return ret_val;
    }

    if( 1 == v.size() )
    {
        ret_val.push_back( v );
        return ret_val;
    }

    vector< int > beg_v( ( v.begin() + 1 ), v.end() );
    vector< int > end_v( v.begin(), ( v.end() - 1 ) );

    ret_val.push_back( beg_v );
    ret_val.push_back( end_v );

    auto beg_sub_v = get_sub_vector( beg_v );
    auto end_sub_v = get_sub_vector( end_v );

    ret_val.reserve( beg_sub_v.size() + end_sub_v.size() );
    ret_val.insert( ret_val.end(), beg_sub_v.begin(), beg_sub_v.end() );
    ret_val.insert( ret_val.end(), end_sub_v.begin(), end_sub_v.edn() );

    return ret_val;
}


TEST( max_sub_vector, smoke_test )
{
    vector< int > expected = { 2, 3 };
    ASSERT_EQ( expected, get_max_sub_vector( { 2, 2, 3 } ) );
}


int main( int argc, char** argv )
{
    testing::InitGoogleTest( &argc, argv );
    return RUN_ALL_TESTS();
}


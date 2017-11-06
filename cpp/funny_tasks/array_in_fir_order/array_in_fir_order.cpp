// we need to reorder array in order: a1 > a2 < a3 > a4 < a5 > a6 ....


#include <gtest/gtest.h>

#include <vector>

void reorder_in_place( std::vector< int >& to_reorder )
{
    for ( size_t i = 1; i < to_reorder.size(); ++i )
    {
        if (i % 2 == 0)
        {
            if ( to_reorder[i - 1] > to_reorder[i] )
            {
                int tmp = to_reorder[i-1];
                to_reorder[i-1] = to_reorder[i];
                to_reorder[i] = tmp;
            }
        }
        else
        {
            if ( to_reorder[i-1] < to_reorder[i] )
            {
                int tmp = to_reorder[i-1];
                to_reorder[i-1] = to_reorder[i];
                to_reorder[i] = tmp;
            }
        }
    }
}

TEST( ArrayInFirOrder, replace_in_place_array_in_ascending_order )
{
    std::vector<int> expected = { 2, 1, 4, 3, 6, 5 };
    std::vector<int> to_reorder = {1, 2, 3, 4, 5, 6};
    reorder_in_place(to_reorder);
    ASSERT_EQ(to_reorder, expected);
}

TEST( ArrayInFirOrder, replace_in_place_array_in_decreasing_order )
{
    std::vector<int> expected = { 6, 4, 5, 2, 3, 1 };
    std::vector<int> to_reorder = {6, 5, 4, 3, 2, 1};
    reorder_in_place(to_reorder);
    ASSERT_EQ(to_reorder, expected);
}

TEST( ArrayInFirOrder, replace_in_place_array_in_random_order_do_nothing )
{
    std::vector<int> expected = { 6, 1, 4, 2, 5, 3 };
    std::vector<int> to_reorder = {6, 1, 4, 2, 5, 3};
    reorder_in_place(to_reorder);
    ASSERT_EQ(to_reorder, expected);
}

TEST( ArrayInFirOrder, replace_in_place_array_in_random_order )
{
    std::vector<int> expected = { 3, 1, 6, 2, 5, 4 };
    std::vector<int> to_reorder = {3, 1, 6, 5, 2, 4};
    reorder_in_place(to_reorder);
    ASSERT_EQ(to_reorder, expected);
}

int main( int argc, char** argv )
{
    testing::InitGoogleTest( &argc, argv );
    return RUN_ALL_TESTS();
}

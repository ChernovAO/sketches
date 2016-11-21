/// @brief A small test library possibilities

#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

TEST( boost_filesystem_tests, smoke_test )
{
    boost::filesystem::path expected_path("some_folder/some_path.some_another_extension");

    boost::filesystem::path some_path("some_folder/some_path.some_extension");
    some_path.replace_extension( "some_another_extension" );

    ASSERT_EQ( expected_path, some_path );
}


int main( int argc, char** argv )
{
    testing::InitGoogleTest( &argc, argv );
    return RUN_ALL_TESTS();
}

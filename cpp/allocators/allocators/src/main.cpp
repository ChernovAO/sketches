/// @file
/// @brief Точка входа

#include <allocators/src/precompiled.h>
#include <allocators/src/my_allocator.h>

#include <vector>

int main(int argc, char* argv[])
{
     std::vector< unsigned char, allocators::MyAllocator< unsigned char > > tmp;
     tmp.push_back( 0x01 );
     tmp.push_back( 0x02 );
     tmp.erase( tmp.begin() );
     return 0;
}

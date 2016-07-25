/// @file
/// @brief Точка входа

#include <allocators/src/precompiled.h>
#include <allocators/src/my_allocator.h>
#include <allocators/src/my_vector.h>

#include <vector>

class TempClass
{
public:
     TempClass( const details::MyVector& vector )
          : vector_( vector.begin(), vector.end() )
     {
     }

private:
     details::MyVector vector_;
};

struct MyStruct
{

};

int main(int argc, char* argv[])
{
     std::vector< unsigned char, details::MyAllocator< unsigned char > > tmp;
     tmp.push_back( 0x01 );
     tmp.push_back( 0x02 );

     std::vector< unsigned char, details::MyAllocator< unsigned char > > otherVector( tmp.begin(), tmp.end() );
     std::vector< unsigned char, details::MyAllocator< unsigned char > > someOtherVector = tmp;

     TempClass classObj( tmp );

     return 0;
}

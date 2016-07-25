/// @file
/// @brief My vector specification for tests allocator redefinition.

#ifndef __MY_VECTOR_H_15_02_2016_1655__
#define __MY_VECTOR_H_15_02_2016_1655__

#include <vector>

#include <allocators/src/my_allocator.h>

namespace details
{
     typedef std::vector< unsigned char, MyAllocator< unsigned char > > MyVector;
}

#endif // !__MY_VECTOR_H_15_02_2016_1655__

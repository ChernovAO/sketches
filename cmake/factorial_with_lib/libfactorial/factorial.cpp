#include "libfactorial/factorial.h"

uint64_t get_factoial(uint64_t base)
{
    if ( base == 0 )
    {
        return 1;
    }

    return base*(get_factoial(base-1));
}

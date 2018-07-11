#include "oryx.h"

#ifdef HAVE_APR
oryx_status_t oryx_atomic_initialize(oryx_pool_t __oryx_unused_param__ *p)
{
    return ORYX_SUCCESS;
}
#else
oryx_status_t oryx_atomic_initialize(void __oryx_unused_param__ *p)
{
    return ORYX_SUCCESS;
}
#endif




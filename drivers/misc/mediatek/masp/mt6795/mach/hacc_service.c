#include <mach/mt_typedefs.h>

typedef unsigned int u32;
extern u32 get_devinfo_with_index(u32 index);

int masp_hal_get_uuid(unsigned int *uuid)
{
    uuid[0] = get_devinfo_with_index(12);
    uuid[1] = get_devinfo_with_index(13);
    uuid[2] = get_devinfo_with_index(12);
    uuid[3] = get_devinfo_with_index(13);

    return 0;
}
int masp_hal_sbc_enabled(void)
{
    return (get_devinfo_with_index(6)&0x00000002)?1:0;
}



#include "ion_profile.h"

MMP_Event ION_MMP_Events[PROFILE_MAX];

void ion_profile_init(void)
{
    MMP_Event ION_Event;
    ION_Event = MMProfileRegisterEvent(MMP_RootEvent, "ION");
    
    ION_MMP_Events[PROFILE_ALLOC] = MMProfileRegisterEvent(ION_Event, "alloc");
    ION_MMP_Events[PROFILE_FREE] = MMProfileRegisterEvent(ION_Event, "free");
    ION_MMP_Events[PROFILE_SHARE] = MMProfileRegisterEvent(ION_Event, "share");
    ION_MMP_Events[PROFILE_IMPORT] = MMProfileRegisterEvent(ION_Event, "import");
    ION_MMP_Events[PROFILE_MAP_KERNEL] = MMProfileRegisterEvent(ION_Event, "map_kern");
    ION_MMP_Events[PROFILE_UNMAP_KERNEL] = MMProfileRegisterEvent(ION_Event, "unmap_kern");
    ION_MMP_Events[PROFILE_MAP_USER] = MMProfileRegisterEvent(ION_Event, "map_user");
    ION_MMP_Events[PROFILE_UNMAP_USER] = MMProfileRegisterEvent(ION_Event, "unmap_user");
    ION_MMP_Events[PROFILE_CUSTOM_IOC] = MMProfileRegisterEvent(ION_Event, "custom_ioc");
    ION_MMP_Events[PROFILE_GET_PHYS] = MMProfileRegisterEvent(ION_Event, "phys");
    ION_MMP_Events[PROFILE_DMA_CLEAN_RANGE] = MMProfileRegisterEvent(ION_Event, "clean_range");
    ION_MMP_Events[PROFILE_DMA_FLUSH_RANGE] = MMProfileRegisterEvent(ION_Event, "flush_range");
    ION_MMP_Events[PROFILE_DMA_INVALID_RANGE] = MMProfileRegisterEvent(ION_Event, "inv_range");
    ION_MMP_Events[PROFILE_DMA_CLEAN_ALL] = MMProfileRegisterEvent(ION_Event, "clean_all");
    ION_MMP_Events[PROFILE_DMA_FLUSH_ALL] = MMProfileRegisterEvent(ION_Event, "flush_all");
    ION_MMP_Events[PROFILE_DMA_INVALID_ALL] = MMProfileRegisterEvent(ION_Event, "inv_all");
    
}


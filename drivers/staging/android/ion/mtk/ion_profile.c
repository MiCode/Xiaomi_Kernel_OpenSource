/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include "ion_profile.h"

mmp_event ion_mmp_events[PROFILE_MAX];
/*because avoid CamelCase, will modify after*/
void ion_profile_init(void)
{
	mmp_event ion_event;

	mmprofile_enable(1);
	ion_event = mmprofile_register_event(mmp_root_event, "ION");
	ion_mmp_events[PROFILE_ALLOC] =
	    mmprofile_register_event(ion_event, "alloc");
	ion_mmp_events[PROFILE_FREE] =
	    mmprofile_register_event(ion_event, "free");
	ion_mmp_events[PROFILE_SHARE] =
	    mmprofile_register_event(ion_event, "share");
	ion_mmp_events[PROFILE_IMPORT] =
	    mmprofile_register_event(ion_event, "import");
	ion_mmp_events[PROFILE_MAP_KERNEL] =
	    mmprofile_register_event(ion_event, "map_kern");
	ion_mmp_events[PROFILE_UNMAP_KERNEL] =
	    mmprofile_register_event(ion_event, "unmap_kern");
	ion_mmp_events[PROFILE_MAP_USER] =
	    mmprofile_register_event(ion_event, "map_user");
	ion_mmp_events[PROFILE_UNMAP_USER] =
	    mmprofile_register_event(ion_event, "unmap_user");
	ion_mmp_events[PROFILE_CUSTOM_IOC] =
	    mmprofile_register_event(ion_event, "custom_ioc");
	ion_mmp_events[PROFILE_GET_PHYS] =
	    mmprofile_register_event(ion_event, "phys");
	ion_mmp_events[PROFILE_DMA_CLEAN_RANGE] =
	    mmprofile_register_event(ion_event, "clean_range");
	ion_mmp_events[PROFILE_DMA_FLUSH_RANGE] =
	    mmprofile_register_event(ion_event, "flush_range");
	ion_mmp_events[PROFILE_DMA_INVALID_RANGE] =
	    mmprofile_register_event(ion_event, "inv_range");
	ion_mmp_events[PROFILE_DMA_CLEAN_ALL] =
	    mmprofile_register_event(ion_event, "clean_all");
	ion_mmp_events[PROFILE_DMA_FLUSH_ALL] =
	    mmprofile_register_event(ion_event, "flush_all");
	ion_mmp_events[PROFILE_DMA_INVALID_ALL] =
	    mmprofile_register_event(ion_event, "inv_all");
	ion_mmp_events[PROFILE_MVA_ALLOC] =
	    mmprofile_register_event(ion_event, "alloc_mva");
	ion_mmp_events[PROFILE_MVA_DEALLOC] =
	    mmprofile_register_event(ion_event, "dealloc_mva");

	/* enable events by default */
	mmprofile_enable_event(ion_mmp_events[PROFILE_ALLOC], 1);
	mmprofile_enable_event(ion_mmp_events[PROFILE_MAP_KERNEL], 0);
	mmprofile_enable_event(ion_mmp_events[PROFILE_MAP_USER], 0);
	mmprofile_enable_event(ion_mmp_events[PROFILE_DMA_CLEAN_ALL], 1);
	mmprofile_enable_event(ion_mmp_events[PROFILE_DMA_FLUSH_ALL], 1);
	mmprofile_enable_event(ion_mmp_events[PROFILE_DMA_INVALID_ALL], 1);
	mmprofile_enable_event(ion_mmp_events[PROFILE_MVA_ALLOC], 1);
	mmprofile_enable_event(ion_mmp_events[PROFILE_MVA_DEALLOC], 1);

	mmprofile_start(1);
}

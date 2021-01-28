/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#if !defined(__ION_PROFILE_H__)
#define __ION_PROFILE_H__

enum ION_PROFILE_TYPE {
	PROFILE_ALLOC = 0,
	PROFILE_FREE,
	PROFILE_SHARE,
	PROFILE_IMPORT,
	PROFILE_MAP_KERNEL,
	PROFILE_UNMAP_KERNEL,
	PROFILE_MAP_USER,
	PROFILE_UNMAP_USER,
	PROFILE_CUSTOM_IOC,
	PROFILE_GET_PHYS,
	PROFILE_MM_HEAP_DEBUG,
	PROFILE_DMA_CLEAN_RANGE,
	PROFILE_DMA_FLUSH_RANGE,
	PROFILE_DMA_INVALID_RANGE,
	PROFILE_DMA_CLEAN_ALL,
	PROFILE_DMA_FLUSH_ALL,
	PROFILE_DMA_INVALID_ALL,
	PROFILE_MVA_ALLOC,
	PROFILE_MVA_DEALLOC,
	PROFILE_MAX,
};

#define ION_PROFILE

#define mmp_root_event 1

void ion_profile_init(void);

#ifndef ION_PROFILE
#define mmprofile_enable_event(...) 0
#define mmprofile_log_ex(...)
#define mmprofile_enable(...)
#define mmprofile_start(...)
#define mmprofile_register_event(...) 0
#define mmp_event unsigned int
#else
#include <mmprofile.h>
#include <mmprofile_function.h>
void mmprofile_enable(int enable);
void mmprofile_start(int start);
#endif

extern mmp_event ion_mmp_events[PROFILE_MAX];

#endif

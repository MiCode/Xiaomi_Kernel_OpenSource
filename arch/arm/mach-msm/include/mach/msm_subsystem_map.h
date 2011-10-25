/*
 * Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __ARCH_MACH_MSM_SUBSYSTEM_MAP_H
#define __ARCH_MACH_MSM_SUBSYSTEM_MAP_H

#include <linux/iommu.h>
#include <mach/iommu_domains.h>

/* map the physical address in the kernel vaddr space */
#define MSM_SUBSYSTEM_MAP_KADDR		0x1
/* map the physical address in the iova address space */
#define MSM_SUBSYSTEM_MAP_IOVA		0x2
/* ioremaps in the kernel address space are cached */
#define	MSM_SUBSYSTEM_MAP_CACHED	0x4
/* ioremaps in the kernel address space are uncached */
#define MSM_SUBSYSTEM_MAP_UNCACHED	0x8
/*
 * Will map 2x the length requested.
 */
#define MSM_SUBSYSTEM_MAP_IOMMU_2X 0x10

/*
 * Shortcut flags for alignment.
 * The flag must be equal to the alignment requested.
 * e.g. for 8k alignment the flags must be (0x2000 | other flags)
 */
#define	MSM_SUBSYSTEM_ALIGN_IOVA_8K	SZ_8K
#define MSM_SUBSYSTEM_ALIGN_IOVA_1M	SZ_1M


enum msm_subsystem_id {
	INVALID_SUBSYS_ID = -1,
	MSM_SUBSYSTEM_VIDEO,
	MSM_SUBSYSTEM_VIDEO_FWARE,
	MSM_SUBSYSTEM_CAMERA,
	MSM_SUBSYSTEM_DISPLAY,
	MSM_SUBSYSTEM_ROTATOR,
	MAX_SUBSYSTEM_ID
};

static inline int msm_subsystem_check_id(int subsys_id)
{
	return subsys_id > INVALID_SUBSYS_ID && subsys_id < MAX_SUBSYSTEM_ID;
}

struct msm_mapped_buffer {
	/*
	 * VA mapped in the kernel address space. This field shall be NULL if
	 * MSM_SUBSYSTEM_MAP_KADDR was not passed to the map buffer function.
	 */
	void *vaddr;
	/*
	 * iovas mapped in the iommu address space. The ith entry of this array
	 * corresponds to the iova mapped in the ith subsystem in the array
	 * pased in to msm_subsystem_map_buffer. This field shall be NULL if
	 * MSM_SUBSYSTEM_MAP_IOVA was not passed to the map buffer function,
	 */
	unsigned long *iova;
};

extern struct msm_mapped_buffer *msm_subsystem_map_buffer(
				unsigned long phys,
				unsigned int length,
				unsigned int flags,
				int *subsys_ids,
				unsigned int nsubsys);

extern int msm_subsystem_unmap_buffer(struct msm_mapped_buffer *buf);

extern phys_addr_t msm_subsystem_check_iova_mapping(int subsys_id,
						unsigned long iova);

#endif /* __ARCH_MACH_MSM_SUBSYSTEM_MAP_H */

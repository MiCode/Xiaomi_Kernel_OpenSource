/**
 * Copyright (c) 2017 The Linux Foundation. All rights reserved.
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
#ifndef SDE_SPLASH_H_
#define SDE_SPLASH_H_

#include "msm_kms.h"
#include "msm_mmu.h"

struct sde_splash_info {
	bool handoff;
	bool program_scratch_regs;
	uint32_t splash_mem_num;
	phys_addr_t *splash_mem_paddr;
	size_t *splash_mem_size;
	struct drm_gem_object **obj;
};

/* APIs for early splash handoff functions */

/**
 * sde_splash_get_handoff_status.
 *
 * This function will read DISP_INTF_SEL regsiter to get
 * the status of early splash.
 */
int sde_splash_get_handoff_status(struct msm_kms *kms);

/**
 * sde_splash_clean_up.
 *
 * Tell LK to exit, and clean up the resource.
 */
int sde_splash_clean_up(struct msm_kms *kms);

/**
 * sde_splash_reserve_memory.
 *
 * To reserve the memory block LK accesses.
 */
int sde_splash_reserve_memory(phys_addr_t phys, size_t size);

/**
 * sde_splash_notify_lk_to_exit.
 *
 * Function to monitor LK's status and tell it to exit.
 */
int sde_splash_notify_lk_to_exit(struct msm_kms *kms);

/**
 * sde_splash_parse_dt.
 *
 * Parse reserved memory block from DT for early splash.
 */
int sde_splash_parse_dt(struct drm_device *dev);

/**
 * sde_splash_smmu_map.
 *
 * Map the physical memory LK visited into iommu driver.
 */
int sde_splash_smmu_map(struct drm_device *dev, struct msm_mmu *mmu,
			struct sde_splash_info *sinfo);

/**
 * sde_splash_smmu_unmap.
 *
 * Unmap the memory.
 */
int sde_splash_smmu_unmap(struct msm_mmu *mmu,
			struct sde_splash_info *sinfo);

/**
 * sde_splash_destroy.
 *
 * Destroy the splash objects.
 */
int sde_splash_destroy(struct sde_splash_info *sinfo);

#endif

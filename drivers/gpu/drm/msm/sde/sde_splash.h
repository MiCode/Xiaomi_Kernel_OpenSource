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

enum splash_connector_type {
	SPLASH_DSI = 0,
	SPLASH_HDMI,
};

struct sde_splash_info {
	/* handoff flag */
	bool handoff;

	/* flag of display scratch registers */
	bool program_scratch_regs;

	/* to indicate LK is totally exited */
	bool lk_is_exited;

	/* memory node used for display buffer */
	uint32_t splash_mem_num;

	/* physical address of memory node for display buffer */
	phys_addr_t *splash_mem_paddr;

	/* size of memory node */
	size_t *splash_mem_size;

	/* constructed gem objects for smmu mapping */
	struct drm_gem_object **obj;

	/* physical address of lk pool */
	phys_addr_t lk_pool_paddr;

	/* memory size of lk pool */
	size_t lk_pool_size;

	/* registered hdmi connector count */
	uint32_t hdmi_connector_cnt;

	/* registered dst connector count */
	uint32_t dsi_connector_cnt;
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
 * sde_splash_init
 *
 * This function will do bandwidth vote and reserved memory
 */
int sde_splash_init(struct sde_power_handle *phandle, struct msm_kms *kms);

/**
 *sde_splash_setup_connector_count
 *
 * To count connector numbers for DSI and HDMI respectively.
 */
void sde_splash_setup_connector_count(struct sde_splash_info *sinfo,
				int connector_type);

/**
 * sde_splash_clean_up_exit_lk.
 *
 * Tell LK to exit, and clean up the resource.
 */
int sde_splash_clean_up_exit_lk(struct msm_kms *kms);

/**
 * sde_splash_clean_up_free_resource.
 *
 * According to input connector_type, free
 * HDMI's and DSI's resource respectively.
 */
int sde_splash_clean_up_free_resource(struct msm_kms *kms,
				struct sde_power_handle *phandle,
				int connector_type, void *display);

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
 * sde_splash_destroy
 *
 * Destroy the resource in failed case.
 */
void sde_splash_destroy(struct sde_splash_info *sinfo,
			struct sde_power_handle *phandle,
			struct sde_power_client *pclient);

/**
 * sde_splash_get_lk_complete_status
 *
 * Get LK's status to check if it has been stopped.
 */
bool sde_splash_get_lk_complete_status(struct sde_splash_info *sinfo);

#endif

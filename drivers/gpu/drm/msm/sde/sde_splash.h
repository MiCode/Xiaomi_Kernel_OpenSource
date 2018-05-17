/**
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#include "sde_hw_mdss.h"

enum splash_connector_type {
	SPLASH_DSI = 0,
	SPLASH_HDMI,
};

struct splash_lm_hw {
	u8 lm_id;
	u8 ctl_id;
	u32 lm_reg_value;
};

struct splash_ctl_top {
	u32 value;
	u8 intf_sel;
	u8 ctl_lm_cnt;
	struct splash_lm_hw lm[LM_MAX - LM_0];
};

struct sde_res_data {
	struct splash_ctl_top top[CTL_MAX - CTL_0];
	u8 ctl_ids[CTL_MAX - CTL_0];
	u8 lm_ids[LM_MAX - LM_0];
	u8 ctl_top_cnt;
	u8 lm_cnt;
};

struct sde_splash_info {
	/* handoff flag */
	bool handoff;

	/* current hw configuration */
	struct sde_res_data res;

	/* flag of display splash status */
	bool display_splash_enabled;

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

	/* enabled statue of displays*/
	uint32_t intf_sel_status;

	/* DSI split enabled flag */
	bool split_is_enabled;

	/* registered hdmi connector count */
	uint32_t hdmi_connector_cnt;

	/* registered dst connector count */
	uint32_t dsi_connector_cnt;

	/* reserved pipe info for early RVC */
	uint32_t reserved_pipe_info[MAX_BLOCKS];
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
 * sde_splash_lk_stop_splash.
 *
 * Tell LK to stop display splash once one valid user commit arrives.
 */
int sde_splash_lk_stop_splash(struct msm_kms *kms,
				struct drm_atomic_state *state);

/**
 * sde_splash_free_resource.
 *
 * To free all LK's resource, including free reserved memory to system,
 * withdraw data bus vote, disable MDP core power, send uevent to user
 * to recycle pipe etc.
 */
int sde_splash_free_resource(struct msm_kms *kms,
			struct sde_power_handle *phandle,
			int connector_type, void *display);

/**
 * sde_splash_parse_memory_dt.
 *
 * Parse reserved memory block from DT for early splash.
 */
int sde_splash_parse_memory_dt(struct drm_device *dev);

/**
 * sde_splash_parse_reserved_plane_dt
 *
 * Parse reserved plane information from DT for early RVC case.
 */
int sde_splash_parse_reserved_plane_dt(struct sde_splash_info *splash_info,
					struct sde_mdss_cfg *cfg);

/*
 * sde_splash_query_plane_is_reserved
 *
 * Query plane is reserved in dt.
 */
bool sde_splash_query_plane_is_reserved(struct sde_splash_info *sinfo,
					uint32_t pipe);

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
bool sde_splash_get_lk_complete_status(struct msm_kms *kms);

/**
 * sde_splash_setup_display_resource
 *
 * Setup display resource based on connector type.
 */
int sde_splash_setup_display_resource(struct sde_splash_info *sinfo,
				void *disp, int connector_type);
#endif

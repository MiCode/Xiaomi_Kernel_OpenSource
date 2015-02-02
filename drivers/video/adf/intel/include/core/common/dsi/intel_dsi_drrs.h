/*
 * Copyright (C) 2015, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author:
 * Ramalingam C <ramalingam.c@intel.com>
 */

#ifndef INTEL_DSI_DRRS_H
#define INTEL_DSI_DRRS_H

#include <core/vlv/vlv_pll.h>

struct drrs_dsi_platform_ops {
	int (*configure_dsi_pll)(struct intel_pipeline *pipeline,
						struct dsi_mnp *dsi_mnp);
	int (*mnp_calculate_for_pclk)(struct intel_pipeline *pipeline,
				struct dsi_mnp *dsi_mnp, unsigned int pclk);
};

/**
 * MIPI PLL register dont have a option to perform a seamless
 * PLL divider change. To simulate that operation in SW we are using
 * this deferred work
 */
struct intel_mipi_drrs_work {
	struct delayed_work work;
	struct intel_pipeline *pipeline;

	/* Target Refresh rate type and the target mode */
	enum drrs_refresh_rate_type target_rr_type;
	struct drm_mode_modeinfo *target_mode;

	/* Atomic variable to terminate the any executing deferred work */
	atomic_t abort_wait_loop;
};

struct dsi_mnp_list {
	struct dsi_mnp mnp1;	/* Fixed mode */
	struct dsi_mnp mnp2;	/* Downclock mode */
	struct dsi_mnp mnp3;	/* Media playback mode */
};

struct dsi_drrs {
	struct intel_mipi_drrs_work *mipi_drrs_work;
	struct dsi_mnp_list mnp_list;
	struct drrs_dsi_platform_ops *platform_ops;
};


extern inline struct drrs_encoder_ops *intel_drrs_dsi_encoder_ops_init(void);
extern inline struct drrs_dsi_platform_ops *vlv_dsi_drrs_ops_init(void);

#endif /* INTEL_DSI_DRRS_H */

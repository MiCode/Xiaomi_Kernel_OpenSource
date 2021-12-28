/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_HW_VDC_H
#define _SDE_HW_VDC_H

#include "sde_hw_catalog.h"
#include "sde_hw_mdss.h"
#include "sde_hw_util.h"
#include "sde_hw_blk.h"

struct sde_hw_vdc;
struct msm_display_vdc_info;

/**
 * struct sde_hw_vdc_ops - interface to the vdc hardware driver functions
 * Assumption is these functions will be called after clocks are enabled
 */
struct sde_hw_vdc_ops {
	/**
	 * vdc_disable - disable vdc
	 * @hw_vdc: Pointer to vdc context
	 */
	void (*vdc_disable)(struct sde_hw_vdc *hw_vdc);

	/**
	 * vdc_config - configures vdc encoder
	 * @hw_vdc: Pointer to vdc context
	 * @vdc: panel vdc parameters
	 * @is_video_mode: current panel mode is video
	 */
	void (*vdc_config)(struct sde_hw_vdc *hw_vdc,
			struct msm_display_vdc_info *vdc, bool is_video_mode);

	/**
	 * bind_pingpong_blk - enable/disable the connection with pp
	 * @hw_vdc: Pointer to vdc context
	 * @enable: enable/disable connection
	 * @pp: pingpong blk id
	 */
	void (*bind_pingpong_blk)(struct sde_hw_vdc *hw_vdc,
			bool enable,
			const enum sde_pingpong pp);
};

struct sde_hw_vdc {
	struct sde_hw_blk base;
	struct sde_hw_blk_reg_map hw;

	/* vdc */
	enum sde_vdc idx;
	const struct sde_vdc_cfg *caps;

	/* ops */
	struct sde_hw_vdc_ops ops;
};

/**
 * sde_hw_vdc - convert base object sde_hw_base to container
 * @hw: Pointer to base hardware block
 * return: Pointer to hardware block container
 */
static inline struct sde_hw_vdc *to_sde_hw_vdc(struct sde_hw_blk *hw)
{
	return container_of(hw, struct sde_hw_vdc, base);
}

/**
 * sde_hw_vdc_init - initializes the vdc block for the passed
 *                   vdc idx.
 * @idx:  VDC index for which driver object is required
 * @addr: Mapped register io address of MDP
 * @m:    Pointer to mdss catalog data
 * Returns: Error code or allocated sde_hw_vdc context
 */
struct sde_hw_vdc *sde_hw_vdc_init(enum sde_vdc idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m);

/**
 * sde_hw_vdc_destroy - destroys vdc driver context
 *                      should be called to free the context
 * @vdc:   Pointer to vdc driver context returned by sde_hw_vdc_init
 */
void sde_hw_vdc_destroy(struct sde_hw_vdc *vdc);

#endif /*_SDE_HW_VDC_H */

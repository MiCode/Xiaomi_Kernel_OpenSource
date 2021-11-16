/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_HW_DSC_H
#define _SDE_HW_DSC_H

#include "sde_hw_catalog.h"
#include "sde_hw_mdss.h"
#include "sde_hw_util.h"
#include "sde_hw_blk.h"

struct sde_hw_dsc;
struct msm_display_dsc_info;

#define DSC_MODE_SPLIT_PANEL            BIT(0)
#define DSC_MODE_MULTIPLEX              BIT(1)
#define DSC_MODE_VIDEO                  BIT(2)

/**
 * struct sde_hw_dsc_ops - interface to the dsc hardware driver functions
 * Assumption is these functions will be called after clocks are enabled
 */
struct sde_hw_dsc_ops {
	/**
	 * dsc_disable - disable dsc
	 * @hw_dsc: Pointer to dsc context
	 */
	void (*dsc_disable)(struct sde_hw_dsc *hw_dsc);

	/**
	 * dsc_config - configures dsc encoder
	 * @hw_dsc: Pointer to dsc context
	 * @dsc: panel dsc parameters
	 * @mode: dsc topology mode to be set
	 * @ich_reset_override: option to reset ich
	 */
	void (*dsc_config)(struct sde_hw_dsc *hw_dsc,
			struct msm_display_dsc_info *dsc,
			u32 mode, bool ich_reset_override);

	/**
	 * dsc_config_thresh - programs panel thresholds
	 * @hw_dsc: Pointer to dsc context
	 * @dsc: panel dsc parameters
	 */
	void (*dsc_config_thresh)(struct sde_hw_dsc *hw_dsc,
			struct msm_display_dsc_info *dsc);

	/**
	 * bind_pingpong_blk - enable/disable the connection with pp
	 * @hw_dsc: Pointer to dsc context
	 * @enable: enable/disable connection
	 * @pp: pingpong blk id
	 */
	void (*bind_pingpong_blk)(struct sde_hw_dsc *hw_dsc,
			bool enable,
			const enum sde_pingpong pp);
};

struct sde_hw_dsc {
	struct sde_hw_blk base;
	struct sde_hw_blk_reg_map hw;

	/* dsc */
	enum sde_dsc idx;
	const struct sde_dsc_cfg *caps;

	/* ops */
	struct sde_hw_dsc_ops ops;
};

/**
 * sde_hw_dsc - convert base object sde_hw_base to container
 * @hw: Pointer to base hardware block
 * return: Pointer to hardware block container
 */
static inline struct sde_hw_dsc *to_sde_hw_dsc(struct sde_hw_blk *hw)
{
	return container_of(hw, struct sde_hw_dsc, base);
}

/**
 * sde_hw_dsc_init - initializes the dsc block for the passed
 *                   dsc idx.
 * @idx:  DSC index for which driver object is required
 * @addr: Mapped register io address of MDP
 * @m:    Pointer to mdss catalog data
 * Returns: Error code or allocated sde_hw_dsc context
 */
struct sde_hw_dsc *sde_hw_dsc_init(enum sde_dsc idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m);

/**
 * sde_hw_dsc_destroy - destroys dsc driver context
 *                      should be called to free the context
 * @dsc:   Pointer to dsc driver context returned by sde_hw_dsc_init
 */
void sde_hw_dsc_destroy(struct sde_hw_dsc *dsc);

#endif /*_SDE_HW_DSC_H */

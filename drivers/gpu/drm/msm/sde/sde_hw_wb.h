/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_HW_WB_H
#define _SDE_HW_WB_H

#include "sde_hw_catalog.h"
#include "sde_hw_mdss.h"
#include "sde_hw_top.h"
#include "sde_hw_util.h"

struct sde_hw_wb;

struct sde_hw_wb_cfg {
	struct sde_hw_fmt_layout dest;
	enum sde_intf_mode intf_mode;
	struct traffic_shaper_cfg ts_cfg;
	struct sde_rect roi;
	bool is_secure;
};

/**
 *
 * struct sde_hw_wb_ops : Interface to the wb Hw driver functions
 *  Assumption is these functions will be called after clocks are enabled
 */
struct sde_hw_wb_ops {
	void (*setup_csc_data)(struct sde_hw_wb *ctx,
			struct sde_csc_cfg *data);

	void (*setup_outaddress)(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *wb);

	void (*setup_outformat)(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *wb);

	void (*setup_rotator)(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *wb);

	void (*setup_dither)(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *wb);

	void (*setup_cdwn)(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *wb);

	void (*setup_trafficshaper)(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *wb);

	void (*setup_roi)(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *wb);
};

/**
 * struct sde_hw_wb : WB driver object
 * @struct sde_hw_blk_reg_map *hw;
 * @idx
 * @wb_hw_caps
 * @ops
 * @highest_bank_bit: GPU highest memory bank bit used
 * @hw_mdp: MDP top level hardware block
 */
struct sde_hw_wb {
	/* base */
	struct sde_hw_blk_reg_map hw;

	/* wb path */
	int idx;
	const struct sde_wb_cfg *caps;

	/* ops */
	struct sde_hw_wb_ops ops;

	u32 highest_bank_bit;

	struct sde_hw_mdp *hw_mdp;
};

/**
 * sde_hw_wb_init(): Initializes and return writeback hw driver object.
 * @idx:  wb_path index for which driver object is required
 * @addr: mapped register io address of MDP
 * @m :   pointer to mdss catalog data
 * @hw_mdp: pointer to mdp top hw driver object
 */
struct sde_hw_wb *sde_hw_wb_init(enum sde_wb idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m,
		struct sde_hw_mdp *hw_mdp);

/**
 * sde_hw_wb_destroy(): Destroy writeback hw driver object.
 * @hw_wb:  Pointer to writeback hw driver object
 */
void sde_hw_wb_destroy(struct sde_hw_wb *hw_wb);

#endif /*_SDE_HW_WB_H */

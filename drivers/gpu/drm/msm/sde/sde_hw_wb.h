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
#include "sde_hw_mdp_util.h"

struct sde_hw_wb;

struct sde_hw_wb_cfg {
	struct sde_hw_source_info dest;
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
};

/**
 * struct sde_hw_wb : WB driver object
 * @struct sde_hw_blk_reg_map *hw;
 * @idx
 * @wb_hw_caps
 * @mixer_hw_caps
 * @ops
 */
struct sde_hw_wb {
	/* base */
	struct sde_hw_blk_reg_map hw;

	/* wb path */
	int idx;
	const struct sde_wb_cfg *caps;

	/* ops */
	struct sde_hw_wb_ops ops;
};

/**
 * sde_hw_wb_init(): Initializes the wb_path hw driver object.
 * should be called before accessing every mixer.
 * @idx:  wb_path index for which driver object is required
 * @addr: mapped register io address of MDP
 * @m :   pointer to mdss catalog data
 */
struct sde_hw_wb *sde_hw_wb_init(enum sde_wb idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m);

#endif /*_SDE_HW_WB_H */

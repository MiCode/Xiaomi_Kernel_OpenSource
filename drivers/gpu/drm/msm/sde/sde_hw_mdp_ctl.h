/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_HW_MDP_CTL_H
#define _SDE_HW_MDP_CTL_H

#include "sde_hw_mdss.h"
#include "sde_hw_mdp_util.h"
#include "sde_hw_catalog.h"

struct sde_hw_ctl;
/**
 * struct sde_hw_stage_cfg - blending stage cfg
 * @stage
 * @border_enable
 */
struct sde_hw_stage_cfg {
	enum sde_sspp stage[SDE_STAGE_MAX][PIPES_PER_STAGE];
	u8 border_enable;
};

/**
 * struct sde_hw_intf_cfg :Desbribes how the mdp writes data to
 *                         output interface
 * @intf :                 Interface id
 * @wb:                    writeback id
 * @mode_3d:               3d mux configuration
 */
struct sde_hw_intf_cfg {
	enum sde_intf intf;
	enum sde_wb wb;
	enum sde_3d_blend_mode mode_3d;
};

/**
 * struct sde_hw_ctl_ops - Interface to the wb Hw driver functions
 * Assumption is these functions will be called after clocks are enabled
 */
struct sde_hw_ctl_ops {
	/**
	 * kickoff hw operation for Sw controlled interfaces
	 * DSI cmd mode and WB interface are SW controlled
	 * @ctx       : ctl path ctx pointer
	 */
	void (*setup_start)(struct sde_hw_ctl *ctx);

	/**
	 * FLUSH the modules for this control path
	 * @ctx       : ctl path ctx pointer
	 * @flushbits : module flushmask
	 */
	void (*setup_flush)(struct sde_hw_ctl *ctx,
		u32  flushbits);

	/**
	 * Setup ctl_path interface config
	 * @ctx
	 * @cfg    : interface config structure pointer
	 */
	void (*setup_intf_cfg)(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg *cfg);

	int (*reset)(struct sde_hw_ctl *c);

	int (*get_bitmask_sspp)(struct sde_hw_ctl *ctx,
		u32 *flushbits,
		enum sde_sspp blk);

	int (*get_bitmask_mixer)(struct sde_hw_ctl *ctx,
		u32 *flushbits,
		enum sde_lm blk);

	int (*get_bitmask_dspp)(struct sde_hw_ctl *ctx,
		u32 *flushbits,
		enum sde_dspp blk);

	int (*get_bitmask_intf)(struct sde_hw_ctl *ctx,
		u32 *flushbits,
		enum sde_intf blk);

	int (*get_bitmask_cdm)(struct sde_hw_ctl *ctx,
		u32 *flushbits,
		enum sde_cdm blk);

	void (*setup_blendstage)(struct sde_hw_ctl *ctx,
		enum sde_lm lm,
		struct sde_hw_stage_cfg *cfg);
};

/**
 * struct sde_hw_ctl : CTL PATH driver object
 * @struct sde_hw_blk_reg_map *hw;
 * @idx
 * @ctl_hw_caps
 * @mixer_hw_caps
 * @ops
 */
struct sde_hw_ctl {
	/* base */
	struct sde_hw_blk_reg_map hw;

	/* ctl path */
	int idx;
	const struct sde_ctl_cfg *caps;
	int mixer_count;
	const struct sde_lm_cfg *mixer_hw_caps;

	/* ops */
	struct sde_hw_ctl_ops ops;
};

/**
 * sde_hw_ctl_init(): Initializes the ctl_path hw driver object.
 * should be called before accessing every ctl path registers.
 * @idx:  ctl_path index for which driver object is required
 * @addr: mapped register io address of MDP
 * @m :   pointer to mdss catalog data
 */
struct sde_hw_ctl *sde_hw_ctl_init(enum sde_ctl idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m);

/**
 * sde_hw_ctl_destroy(): Destroys ctl driver context
 * should be called to free the context
 */
void sde_hw_ctl_destroy(struct sde_hw_ctl *ctx);

#endif /*_SDE_HW_MDP_CTL_H */

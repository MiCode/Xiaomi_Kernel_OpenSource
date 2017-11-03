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

#ifndef _SDE_HW_LM_H
#define _SDE_HW_LM_H

#include "sde_hw_mdss.h"
#include "sde_hw_util.h"
#include "sde_hw_blk.h"

struct sde_hw_mixer;

struct sde_hw_mixer_cfg {
	u32 out_width;
	u32 out_height;
	bool right_mixer;
	int flags;
};

struct sde_hw_color3_cfg {
	u8 keep_fg[SDE_STAGE_MAX];
};

/**
 *
 * struct sde_hw_lm_ops : Interface to the mixer Hw driver functions
 *  Assumption is these functions will be called after clocks are enabled
 */
struct sde_hw_lm_ops {
	/*
	 * Sets up mixer output width and height
	 * and border color if enabled
	 */
	void (*setup_mixer_out)(struct sde_hw_mixer *ctx,
		struct sde_hw_mixer_cfg *cfg);

	/*
	 * Alpha blending configuration
	 * for the specified stage
	 */
	void (*setup_blend_config)(struct sde_hw_mixer *ctx, uint32_t stage,
		uint32_t fg_alpha, uint32_t bg_alpha, uint32_t blend_op);

	/*
	 * Alpha color component selection from either fg or bg
	 */
	void (*setup_alpha_out)(struct sde_hw_mixer *ctx, uint32_t mixer_op);

	/**
	 * setup_border_color : enable/disable border color
	 */
	void (*setup_border_color)(struct sde_hw_mixer *ctx,
		struct sde_mdss_color *color,
		u8 border_en);
	/**
	 * setup_gc : enable/disable gamma correction feature
	 */
	void (*setup_gc)(struct sde_hw_mixer *mixer,
			void *cfg);

	/**
	 * setup_dim_layer: configure dim layer settings
	 * @ctx: Pointer to layer mixer context
	 * @dim_layer: dim layer configs
	 */
	void (*setup_dim_layer)(struct sde_hw_mixer *ctx,
			struct sde_hw_dim_layer *dim_layer);

	/**
	 * clear_dim_layer: clear dim layer settings
	 * @ctx: Pointer to layer mixer context
	 */
	void (*clear_dim_layer)(struct sde_hw_mixer *ctx);

	/* setup_misr: enables/disables MISR in HW register */
	void (*setup_misr)(struct sde_hw_mixer *ctx,
			bool enable, u32 frame_count);

	/* collect_misr: reads and stores MISR data from HW register */
	u32 (*collect_misr)(struct sde_hw_mixer *ctx);
};

struct sde_hw_mixer {
	struct sde_hw_blk base;
	struct sde_hw_blk_reg_map hw;

	/* lm */
	enum sde_lm  idx;
	const struct sde_lm_cfg   *cap;
	const struct sde_mdp_cfg  *mdp;
	const struct sde_ctl_cfg  *ctl;

	/* ops */
	struct sde_hw_lm_ops ops;

	/* store mixer info specific to display */
	struct sde_hw_mixer_cfg cfg;
};

/**
 * to_sde_hw_mixer - convert base object sde_hw_base to container
 * @hw: Pointer to base hardware block
 * return: Pointer to hardware block container
 */
static inline struct sde_hw_mixer *to_sde_hw_mixer(struct sde_hw_blk *hw)
{
	return container_of(hw, struct sde_hw_mixer, base);
}

/**
 * sde_hw_lm_init(): Initializes the mixer hw driver object.
 * should be called once before accessing every mixer.
 * @idx:  mixer index for which driver object is required
 * @addr: mapped register io address of MDP
 * @m :   pointer to mdss catalog data
 */
struct sde_hw_mixer *sde_hw_lm_init(enum sde_lm idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m);

/**
 * sde_hw_lm_destroy(): Destroys layer mixer driver context
 * @lm:   Pointer to LM driver context
 */
void sde_hw_lm_destroy(struct sde_hw_mixer *lm);

#endif /*_SDE_HW_LM_H */

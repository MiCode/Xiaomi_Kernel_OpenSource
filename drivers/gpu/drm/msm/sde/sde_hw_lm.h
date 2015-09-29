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

#ifndef _SDE_HW_LM_H
#define _SDE_HW_LM_H

#include "sde_hw_mdss.h"
#include "sde_hw_mdp_util.h"

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
	void (*setup_blend_config)(struct sde_hw_mixer *ctx,
			int stage,
			struct sde_hw_blend_cfg *blend);

	/*
	 * Alpha color component selection from either fg or bg
	 */
	void (*setup_alpha_out)(struct sde_hw_mixer *ctx,
			struct sde_hw_color3_cfg *cfg);

	/**
	 * setup_border_color : enable/disable border color
	 */
	void (*setup_border_color)(struct sde_hw_mixer *ctx,
		struct sde_mdss_color *color,
		u8 border_en);

	void (*setup_gammcorrection)(struct sde_hw_mixer *mixer,
			void *cfg);

};

struct sde_hw_mixer {
	/* base */
	struct sde_hw_blk_reg_map hw;

	/* lm */
	enum sde_lm  idx;
	const struct sde_lm_cfg   *cap;
	const struct sde_mdp_cfg  *mdp;
	const struct sde_ctl_cfg  *ctl;

	/* ops */
	struct sde_hw_lm_ops ops;
};

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

#endif /*_SDE_HW_LM_H */

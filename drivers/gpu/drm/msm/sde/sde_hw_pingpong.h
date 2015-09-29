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

#ifndef _SDE_HW_PINGPONG_H
#define _SDE_HW_PINGPONG_H

struct sde_hw_pingpong;

struct sde_hw_tear_check {
	/*
	 * This is ratio of MDP VSYNC clk freq(Hz) to
	 * refresh rate divided by no of lines
	 */
	u32 vsync_count;
	u32 sync_cfg_height;
	u32 vsync_init_val;
	u32 sync_threshold_start;
	u32 sync_threshold_continue;
	u32 start_pos;
	u32 rd_ptr_irq;
	u8 hw_vsync_mode;
};

struct sde_hw_autorefresh {
	bool  enable;
	u32 frame_count;
};

struct sde_hw_pp_vsync_info {
	u32 init_val; /* value of rd pointer at vsync edge */
	u32 vsync_count;    /* mdp clocks to complete one line */
	u32 line_count;   /* current line count */
};

struct sde_hw_dsc_cfg {
	u8 enable;
};

/**
 *
 * struct sde_hw_pingpong_ops : Interface to the pingpong Hw driver functions
 *  Assumption is these functions will be called after clocks are enabled
 *  @setup_tearcheck :
 *  @enable_tearcheck :
 *  @get_vsync_info :
 *  @setup_autorefresh :
 *  #setup_dsc :
 */
struct sde_hw_pingpong_ops {
	/**
	 * enables vysnc generation and sets up init value of
	 * read pointer and programs the tear check cofiguration
	 */
	int (*setup_tearcheck)(struct sde_hw_pingpong *pp,
			struct sde_hw_tear_check *cfg);

	/**
	 * enables tear check block
	 */
	int (*enable_tearcheck)(struct sde_hw_pingpong *pp,
			bool enable);

	/**
	 * provides the programmed and current
	 * line_count
	 */
	int (*get_vsync_info)(struct sde_hw_pingpong *pp,
			struct sde_hw_pp_vsync_info  *info);

	/**
	 * configure and enable the autorefresh config
	 */
	int (*setup_autorefresh)(struct sde_hw_pingpong *pp,
			struct sde_hw_autorefresh *cfg);

	/**
	 * Program the dsc compression block
	 */
	int (*setup_dsc)(struct sde_hw_pingpong *pp,
			struct sde_hw_dsc_cfg *cfg);
};

struct sde_hw_pingpong {
	/* base */
	struct sde_hw_blk_reg_map hw;

	/* pingpong */
	enum sde_pingpong idx;
	const struct sde_pingpong_cfg *pingpong_hw_cap;

	/* ops */
	struct sde_hw_pingpong_ops ops;
};

/**
 * sde_hw_pingpong_init(): Initializes the pingpong driver for the passed
 *        pingpong idx.
 * @idx:  pingpong index for which driver object is required
 * @addr: mapped register io address of MDP
 * @m :   pointer to mdss catalog data
 */
struct sde_hw_pingpong *sde_hw_pingpong_init(enum sde_pingpong idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m);

#endif /*_SDE_HW_PINGPONG_H */

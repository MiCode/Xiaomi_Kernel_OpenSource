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

#ifndef _SDE_HW_PINGPONG_H
#define _SDE_HW_PINGPONG_H

#include "sde_hw_catalog.h"
#include "sde_hw_mdss.h"
#include "sde_hw_util.h"
#include "sde_hw_blk.h"
#include <uapi/drm/msm_drm_pp.h>

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
	u32 rd_ptr_init_val;	/* value of rd pointer at vsync edge */
	u32 rd_ptr_frame_count;	/* num frames sent since enabling interface */
	u32 rd_ptr_line_count;	/* current line on panel (rd ptr) */
	u32 wr_ptr_line_count;	/* current line within pp fifo (wr ptr) */
};

struct sde_hw_dsc_cfg {
	u8 enable;
};

/**
 *
 * struct sde_hw_pingpong_ops : Interface to the pingpong Hw driver functions
 *  Assumption is these functions will be called after clocks are enabled
 *  @setup_tearcheck : program tear check values
 *  @enable_tearcheck : enables tear check
 *  @get_vsync_info : retries timing info of the panel
 *  @setup_autorefresh : program auto refresh
 *  @setup_dsc : program DSC block with encoding details
 *  @enable_dsc : enables DSC encoder
 *  @disable_dsc : disables DSC encoder
 *  @setup_dither : function to program the dither hw block
 *  @get_line_count: obtain current vertical line counter
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
	 * read, modify, write to either set or clear listening to external TE
	 * @Return: 1 if TE was originally connected, 0 if not, or -ERROR
	 */
	int (*connect_external_te)(struct sde_hw_pingpong *pp,
			bool enable_external_te);

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
	 * retrieve autorefresh config from hardware
	 */
	int (*get_autorefresh)(struct sde_hw_pingpong *pp,
			struct sde_hw_autorefresh *cfg);

	/**
	 * poll until write pointer transmission starts
	 * @Return: 0 on success, -ETIMEDOUT on timeout
	 */
	int (*poll_timeout_wr_ptr)(struct sde_hw_pingpong *pp, u32 timeout_us);

	/**
	 * Program the dsc compression block
	 */
	int (*setup_dsc)(struct sde_hw_pingpong *pp);

	/**
	 * Enables DSC encoder
	 */
	void (*enable_dsc)(struct sde_hw_pingpong *pp);

	/**
	 * Disables DSC encoder
	 */
	void (*disable_dsc)(struct sde_hw_pingpong *pp);
       /**
        * Get DSC status
        * @Return: register value of DSC config
        */
       u32 (*get_dsc_status)(struct sde_hw_pingpong *pp);
	/**
	 * Program the dither hw block
	 */
	int (*setup_dither)(struct sde_hw_pingpong *pp, void *cfg, size_t len);

	/**
	 * Obtain current vertical line counter
	 */
	u32 (*get_line_count)(struct sde_hw_pingpong *pp);
};

struct sde_hw_pingpong {
	struct sde_hw_blk base;
	struct sde_hw_blk_reg_map hw;

	/* pingpong */
	enum sde_pingpong idx;
	const struct sde_pingpong_cfg *caps;

	/* ops */
	struct sde_hw_pingpong_ops ops;
};

/**
 * sde_hw_pingpong - convert base object sde_hw_base to container
 * @hw: Pointer to base hardware block
 * return: Pointer to hardware block container
 */
static inline struct sde_hw_pingpong *to_sde_hw_pingpong(struct sde_hw_blk *hw)
{
	return container_of(hw, struct sde_hw_pingpong, base);
}

/**
 * sde_hw_pingpong_init - initializes the pingpong driver for the passed
 *	pingpong idx.
 * @idx:  Pingpong index for which driver object is required
 * @addr: Mapped register io address of MDP
 * @m:    Pointer to mdss catalog data
 * Returns: Error code or allocated sde_hw_pingpong context
 */
struct sde_hw_pingpong *sde_hw_pingpong_init(enum sde_pingpong idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m);

/**
 * sde_hw_pingpong_destroy - destroys pingpong driver context
 *	should be called to free the context
 * @pp:   Pointer to PP driver context returned by sde_hw_pingpong_init
 */
void sde_hw_pingpong_destroy(struct sde_hw_pingpong *pp);

#endif /*_SDE_HW_PINGPONG_H */

/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_HW_INTF_H
#define _SDE_HW_INTF_H

#include "sde_hw_catalog.h"
#include "sde_hw_mdss.h"
#include "sde_hw_util.h"
#include "sde_hw_blk.h"

struct sde_hw_intf;

/* intf timing settings */
struct intf_timing_params {
	u32 width;		/* active width */
	u32 height;		/* active height */
	u32 xres;		/* Display panel width */
	u32 yres;		/* Display panel height */

	u32 h_back_porch;
	u32 h_front_porch;
	u32 v_back_porch;
	u32 v_front_porch;
	u32 hsync_pulse_width;
	u32 vsync_pulse_width;
	u32 hsync_polarity;
	u32 vsync_polarity;
	u32 border_clr;
	u32 underflow_clr;
	u32 hsync_skew;
	u32 v_front_porch_fixed;
};

struct intf_prog_fetch {
	u8 enable;
	/* vsync counter for the front porch pixel line */
	u32 fetch_start;
};

struct intf_status {
	u8 is_en;		/* interface timing engine is enabled or not */
	u32 frame_count;	/* frame count since timing engine enabled */
	u32 line_count;		/* current line count including blanking */
};

/**
 * struct sde_hw_intf_ops : Interface to the interface Hw driver functions
 *  Assumption is these functions will be called after clocks are enabled
 * @ setup_timing_gen : programs the timing engine
 * @ setup_prog_fetch : enables/disables the programmable fetch logic
 * @ setup_rot_start  : enables/disables the rotator start trigger
 * @ enable_timing: enable/disable timing engine
 * @ get_status: returns if timing engine is enabled or not
 * @ setup_misr: enables/disables MISR in HW register
 * @ collect_misr: reads and stores MISR data from HW register
 * @ get_line_count: reads current vertical line counter
 */
struct sde_hw_intf_ops {
	void (*setup_timing_gen)(struct sde_hw_intf *intf,
			const struct intf_timing_params *p,
			const struct sde_format *fmt);

	void (*setup_prg_fetch)(struct sde_hw_intf *intf,
			const struct intf_prog_fetch *fetch);

	void (*setup_rot_start)(struct sde_hw_intf *intf,
			const struct intf_prog_fetch *fetch);

	void (*enable_timing)(struct sde_hw_intf *intf,
			u8 enable);

	void (*get_status)(struct sde_hw_intf *intf,
			struct intf_status *status);

	void (*setup_misr)(struct sde_hw_intf *intf,
			bool enable, u32 frame_count);

	u32 (*collect_misr)(struct sde_hw_intf *intf);

	u32 (*get_line_count)(struct sde_hw_intf *intf);
};

struct sde_hw_intf {
	struct sde_hw_blk base;
	struct sde_hw_blk_reg_map hw;

	/* intf */
	enum sde_intf idx;
	const struct sde_intf_cfg *cap;
	const struct sde_mdss_cfg *mdss;

	/* ops */
	struct sde_hw_intf_ops ops;
};

/**
 * to_sde_hw_intf - convert base object sde_hw_base to container
 * @hw: Pointer to base hardware block
 * return: Pointer to hardware block container
 */
static inline struct sde_hw_intf *to_sde_hw_intf(struct sde_hw_blk *hw)
{
	return container_of(hw, struct sde_hw_intf, base);
}

/**
 * sde_hw_intf_init(): Initializes the intf driver for the passed
 * interface idx.
 * @idx:  interface index for which driver object is required
 * @addr: mapped register io address of MDP
 * @m :   pointer to mdss catalog data
 */
struct sde_hw_intf *sde_hw_intf_init(enum sde_intf idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m);

/**
 * sde_hw_intf_destroy(): Destroys INTF driver context
 * @intf:   Pointer to INTF driver context
 */
void sde_hw_intf_destroy(struct sde_hw_intf *intf);

#endif /*_SDE_HW_INTF_H */

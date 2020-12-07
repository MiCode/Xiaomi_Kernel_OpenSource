/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_HW_INTF_H
#define _SDE_HW_INTF_H

#include "sde_hw_catalog.h"
#include "sde_hw_mdss.h"
#include "sde_hw_util.h"
#include "sde_hw_blk.h"
#include "sde_kms.h"

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
	bool wide_bus_en;	/* for DP only */
	bool compression_en;	/* for DP only */
	u32 extra_dto_cycles;	/* for DP only */
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

struct intf_tear_status {
	u32 read_count;		/* frame & line count for tear init value */
	u32 write_count;	/* frame & line count for tear write */
};

struct intf_avr_params {
	u32 default_fps;
	u32 min_fps;
	u32 avr_mode; /* 0 - disable, 1 - continuous, 2 - one-shot */
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
 * @bind_pingpong_blk: enable/disable the connection with pingpong which will
 *                     feed pixels to this interface
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

	int (*collect_misr)(struct sde_hw_intf *intf,
			bool nonblock, u32 *misr_value);

	/**
	 * returns the current scan line count of the display
	 * video mode panels use get_line_count whereas get_vsync_info
	 * is used for command mode panels
	 */
	u32 (*get_line_count)(struct sde_hw_intf *intf);

	void (*bind_pingpong_blk)(struct sde_hw_intf *intf,
			bool enable,
			const enum sde_pingpong pp);

	/**
	 * enables vysnc generation and sets up init value of
	 * read pointer and programs the tear check cofiguration
	 */
	int (*setup_tearcheck)(struct sde_hw_intf *intf,
			struct sde_hw_tear_check *cfg);

	/**
	 * enables tear check block
	 */
	int (*enable_tearcheck)(struct sde_hw_intf *intf,
			bool enable);

	/**
	 * updates tearcheck configuration
	 */
	void (*update_tearcheck)(struct sde_hw_intf *intf,
			struct sde_hw_tear_check *cfg);

	/**
	 * read, modify, write to either set or clear listening to external TE
	 * @Return: 1 if TE was originally connected, 0 if not, or -ERROR
	 */
	int (*connect_external_te)(struct sde_hw_intf *intf,
			bool enable_external_te);

	/**
	 * provides the programmed and current
	 * line_count
	 */
	int (*get_vsync_info)(struct sde_hw_intf *intf,
			struct sde_hw_pp_vsync_info  *info);

	/**
	 * configure and enable the autorefresh config
	 */
	int (*setup_autorefresh)(struct sde_hw_intf *intf,
			struct sde_hw_autorefresh *cfg);

	/**
	 * retrieve autorefresh config from hardware
	 */
	int (*get_autorefresh)(struct sde_hw_intf *intf,
			struct sde_hw_autorefresh *cfg);

	/**
	 * poll until write pointer transmission starts
	 * @Return: 0 on success, -ETIMEDOUT on timeout
	 */
	int (*poll_timeout_wr_ptr)(struct sde_hw_intf *intf, u32 timeout_us);

	/**
	 * Select vsync signal for tear-effect configuration
	 */
	void (*vsync_sel)(struct sde_hw_intf *intf, u32 vsync_source);

	/**
	 * Program the AVR_TOTAL for min fps rate
	 */
	int (*avr_setup)(struct sde_hw_intf *intf,
			const struct intf_timing_params *params,
			const struct intf_avr_params *avr_params);

	/**
	 * Signal the trigger on each commit for AVR
	 */
	void (*avr_trigger)(struct sde_hw_intf *ctx);

	/**
	 * Enable AVR and select the mode
	 */
	void (*avr_ctrl)(struct sde_hw_intf *intf,
			const struct intf_avr_params *avr_params);

	/**
	 * Check the intf tear check status and reset it to start_pos
	 */
	int (*check_and_reset_tearcheck)(struct sde_hw_intf *intf,
			struct intf_tear_status *status);
};

struct sde_hw_intf {
	struct sde_hw_blk base;
	struct sde_hw_blk_reg_map hw;

	/* intf */
	enum sde_intf idx;
	const struct sde_intf_cfg *cap;
	const struct sde_mdss_cfg *mdss;
	struct split_pipe_cfg cfg;

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

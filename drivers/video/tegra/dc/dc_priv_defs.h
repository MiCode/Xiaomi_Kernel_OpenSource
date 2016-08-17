
/*
 * drivers/video/tegra/dc/dc_priv.h
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
 *
 * Copyright (c) 2010-2012, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DRIVERS_VIDEO_TEGRA_DC_DC_PRIV_DEFS_H
#define __DRIVERS_VIDEO_TEGRA_DC_DC_PRIV_DEFS_H
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/switch.h>
#include <linux/nvhost.h>
#include <linux/types.h>

#include <mach/dc.h>

#include <mach/tegra_dc_ext.h>
#include <mach/clk.h>

#include "dc_reg.h"

#define WIN_IS_TILED(win)	((win)->flags & TEGRA_WIN_FLAG_TILED)
#define WIN_IS_ENABLED(win)	((win)->flags & TEGRA_WIN_FLAG_ENABLED)

#define NEED_UPDATE_EMC_ON_EVERY_FRAME (windows_idle_detection_time == 0)

/* 29 bit offset for window clip number */
#define CURSOR_CLIP_SHIFT_BITS(win)	(win << 29)
#define CURSOR_CLIP_GET_WINDOW(reg)	((reg >> 29) & 3)

#ifndef CONFIG_TEGRA_FPGA_PLATFORM
#define ALL_UF_INT (WIN_A_UF_INT | WIN_B_UF_INT | WIN_C_UF_INT)
#else
/* ignore underflows when on simulation and fpga platform */
#define ALL_UF_INT (0)
#endif

struct tegra_dc;

struct tegra_dc_blend {
	unsigned z[DC_N_WINDOWS];
	unsigned flags[DC_N_WINDOWS];
};

struct tegra_dc_out_ops {
	/* initialize output.  dc clocks are not on at this point */
	int (*init)(struct tegra_dc *dc);
	/* destroy output.  dc clocks are not on at this point */
	void (*destroy)(struct tegra_dc *dc);
	/* detect connected display.  can sleep.*/
	bool (*detect)(struct tegra_dc *dc);
	/* enable output.  dc clocks are on at this point */
	void (*enable)(struct tegra_dc *dc);
	/* disable output.  dc clocks are on at this point */
	void (*disable)(struct tegra_dc *dc);
	/* hold output.  keeps dc clocks on. */
	void (*hold)(struct tegra_dc *dc);
	/* release output.  dc clocks may turn off after this. */
	void (*release)(struct tegra_dc *dc);
	/* idle routine of output.  dc clocks may turn off after this. */
	void (*idle)(struct tegra_dc *dc);
	/* suspend output.  dc clocks are on at this point */
	void (*suspend)(struct tegra_dc *dc);
	/* resume output.  dc clocks are on at this point */
	void (*resume)(struct tegra_dc *dc);
	/* mode filter. to provide a list of supported modes*/
	bool (*mode_filter)(const struct tegra_dc *dc,
			struct fb_videomode *mode);
	/* setup pixel clock and parent clock programming */
	long (*setup_clk)(struct tegra_dc *dc, struct clk *clk);
};

struct tegra_dc_shift_clk_div {
	unsigned long mul; /* numerator */
	unsigned long div; /* denominator */
};

struct tegra_dc {
	struct platform_device		*ndev;
	struct tegra_dc_platform_data	*pdata;

	struct resource			*base_res;
	void __iomem			*base;
	int				irq;

	struct clk			*clk;
	struct clk			*emc_clk;
	int				emc_clk_rate;
	int				new_emc_clk_rate;
	struct tegra_dc_shift_clk_div	shift_clk_div;

	u32				powergate_id;

	bool				connected;
	bool				enabled;
	bool				suspended;

	struct tegra_dc_out		*out;
	struct tegra_dc_out_ops		*out_ops;
	void				*out_data;

	struct tegra_dc_mode		mode;
	s64				frametime_ns;

	struct tegra_dc_win		windows[DC_N_WINDOWS];
	struct tegra_dc_blend		blend;
	int				n_windows;
#ifdef CONFIG_TEGRA_DC_CMU
	struct tegra_dc_cmu		cmu;
#endif
	wait_queue_head_t		wq;
	wait_queue_head_t		timestamp_wq;

	struct mutex			lock;
	struct mutex			one_shot_lock;

	struct resource			*fb_mem;
	struct tegra_fb_info		*fb;

	struct {
		u32			id;
		u32			min;
		u32			max;
	} syncpt[DC_N_WINDOWS];
	u32				vblank_syncpt;
	u32				win_syncpt[DC_N_WINDOWS];

	unsigned long			underflow_mask;
	struct work_struct		reset_work;

#ifdef CONFIG_SWITCH
	struct switch_dev		modeset_switch;
#endif

	struct completion		frame_end_complete;

	struct work_struct		vblank_work;
	long				vblank_ref_count;
	struct work_struct		vpulse2_work;
	long				vpulse2_ref_count;

	struct {
		u64			underflows;
		u64			underflows_a;
		u64			underflows_b;
		u64			underflows_c;
	} stats;

	struct tegra_dc_ext		*ext;

	struct tegra_dc_feature		*feature;
	int				gen1_blend_num;

#ifdef CONFIG_DEBUG_FS
	struct dentry			*debugdir;
#endif
	struct tegra_dc_lut		fb_lut;
	struct delayed_work		underflow_work;
	u32				one_shot_delay_ms;
	struct delayed_work		one_shot_work;
	s64				frame_end_timestamp;

	bool				mode_dirty;
};

#endif

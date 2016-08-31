/*
 * drivers/video/tegra/dc/dc_priv.h
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION, All rights reserved.
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
#include <linux/clk/tegra.h>
#include <linux/tegra-soc.h>

#include <mach/dc.h>

#include <mach/tegra_dc_ext.h>
#include <mach/isomgr.h>

#include "dc_reg.h"


#define NEED_UPDATE_EMC_ON_EVERY_FRAME (windows_idle_detection_time == 0)

/* 29 bit offset for window clip number */
#define CURSOR_CLIP_SHIFT_BITS(win)	(win << 29)
#define CURSOR_CLIP_GET_WINDOW(reg)	((reg >> 29) & 3)

static inline u32 ALL_UF_INT(void)
{
	if (tegra_platform_is_fpga())
		return 0;
#if defined(CONFIG_ARCH_TEGRA_14x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
	return WIN_A_UF_INT | WIN_B_UF_INT | WIN_C_UF_INT | HC_UF_INT |
		WIN_D_UF_INT | WIN_T_UF_INT;
#else
	return WIN_A_UF_INT | WIN_B_UF_INT | WIN_C_UF_INT;
#endif
}

#if defined(CONFIG_TEGRA_EMC_TO_DDR_CLOCK)
#define EMC_BW_TO_FREQ(bw) (DDR_BW_TO_FREQ(bw) * CONFIG_TEGRA_EMC_TO_DDR_CLOCK)
#else
#define EMC_BW_TO_FREQ(bw) (DDR_BW_TO_FREQ(bw) * 2)
#endif

struct tegra_dc;

struct tegra_dc_blend {
	unsigned z[DC_N_WINDOWS];
	unsigned flags[DC_N_WINDOWS];
	u8 alpha[DC_N_WINDOWS];
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
	/* enable dc client.  Panel is enable at this point */
	void (*postpoweron)(struct tegra_dc *dc);
	/* disable output.  dc clocks are on at this point */
	void (*disable)(struct tegra_dc *dc);
	/* dc client is disabled.  dc clocks are on at this point */
	void (*postpoweroff) (struct tegra_dc *dc);
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
	/*
	 * return true if display client is suspended during OSidle.
	 * If true, dc will not wait on any display client event
	 * during OSidle.
	 */
	bool (*osidle)(struct tegra_dc *dc);
	/* callback after new mode is programmed.
	 * dc clocks are on at this point */
	void (*modeset_notifier)(struct tegra_dc *dc);
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
#ifdef CONFIG_TEGRA_ISOMGR
	tegra_isomgr_handle		isomgr_handle;
#else
	struct clk			*emc_clk;
#endif
	long				bw_kbps; /* bandwidth in KBps */
	long				new_bw_kbps;
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
	struct mutex			one_shot_lp_lock;

	struct resource			*fb_mem;
	struct tegra_fb_info		*fb;

	struct {
		u32			id;
		u32			min;
		u32			max;
	} syncpt[DC_N_WINDOWS];
	u32				vblank_syncpt;
	u32				win_syncpt[DC_N_WINDOWS];

	unsigned long int		valid_windows;
	unsigned long int		win_status;

	unsigned long			underflow_mask;
	struct work_struct		reset_work;

#ifdef CONFIG_SWITCH
	struct switch_dev		modeset_switch;
#endif

	struct completion		frame_end_complete;
	struct completion		crc_complete;
	bool				crc_pending;

	struct work_struct		vblank_work;
	long				vblank_ref_count;
	struct work_struct		vpulse2_work;
	long				vpulse2_ref_count;

	struct {
		u64			underflows;
		u64			underflows_a;
		u64			underflows_b;
		u64			underflows_c;
#if defined(CONFIG_ARCH_TEGRA_14x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
		u64			underflows_d;
		u64			underflows_h;
		u64			underflows_t;
#endif
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
	atomic_t			frame_end_ref;

	bool				mode_dirty;

	u32				reserved_bw;
	u32				available_bw;
	struct tegra_dc_win		tmp_wins[DC_N_WINDOWS];

	int				win_blank_saved_flag;
	struct tegra_dc_win		win_blank_saved;
};

#endif

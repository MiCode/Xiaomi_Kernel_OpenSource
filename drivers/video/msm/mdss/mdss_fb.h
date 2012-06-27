/* Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef MDSS_FB_H
#define MDSS_FB_H

#include <linux/ion.h>
#include <linux/list.h>
#include <linux/msm_mdp.h>
#include <linux/types.h>

#include "mdss_mdp.h"
#include "mdss_panel.h"

#define MSM_FB_DEFAULT_PAGE_SIZE 2
#define MFD_KEY  0x11161126
#define MSM_FB_MAX_DEV_LIST 32

#define MSM_FB_ENABLE_DBGFS

#ifndef MAX
#define  MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef MIN
#define  MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

struct disp_info_type_suspend {
	int op_enable;
	int panel_power_on;
};

struct msm_fb_data_type {
	u32 key;
	u32 index;
	u32 ref_cnt;
	u32 fb_page;

	struct panel_id panel;
	struct mdss_panel_info panel_info;

	u32 dest;
	struct fb_info *fbi;

	int op_enable;
	u32 fb_imgType;
	u32 dst_format;

	int hw_refresh;

	int overlay_play_enable;

	int panel_power_on;
	struct disp_info_type_suspend suspend;

	int (*on_fnc) (struct msm_fb_data_type *mfd);
	int (*off_fnc) (struct msm_fb_data_type *mfd);
	int (*kickoff_fnc) (struct mdss_mdp_ctl *ctl);
	int (*ioctl_handler) (struct msm_fb_data_type *mfd, u32 cmd, void *arg);
	void (*dma_fnc) (struct msm_fb_data_type *mfd);
	int (*cursor_update) (struct fb_info *info,
			      struct fb_cursor *cursor);
	int (*lut_update) (struct fb_info *info,
			   struct fb_cmap *cmap);
	int (*do_histogram) (struct fb_info *info,
			     struct mdp_histogram *hist);
	void *cursor_buf;
	void *cursor_buf_phys;

	u32 bl_level;
	struct mutex lock;

	struct platform_device *pdev;

	u32 var_xres;
	u32 var_yres;
	u32 var_pixclock;

	u32 mdp_fb_page_protection;
	struct ion_client *iclient;

	struct mdss_mdp_ctl *ctl;
	struct mdss_mdp_wb *wb;
};

int mdss_fb_get_phys_info(unsigned long *start, unsigned long *len, int fb_num);
void mdss_fb_set_backlight(struct msm_fb_data_type *mfd, u32 bkl_lvl);
void mdss_fb_update_backlight(struct msm_fb_data_type *mfd);
#endif /* MDSS_FB_H */

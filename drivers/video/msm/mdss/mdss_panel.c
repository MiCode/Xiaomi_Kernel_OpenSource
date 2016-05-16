/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/fb.h>

#include "mdss_panel.h"

#define NUM_DSI_INTF 2

int mdss_panel_debugfs_setup(struct mdss_panel_info *panel_info, struct dentry
		*parent, char *dsi_str)
{
	struct mdss_panel_debugfs_info *debugfs_info;
	debugfs_info = kzalloc(sizeof(*debugfs_info), GFP_KERNEL);
	if (!debugfs_info) {
		pr_err("No memory to create panel debugfs info");
		return -ENOMEM;
	}

	debugfs_info->root = debugfs_create_dir(dsi_str, parent);
	if (IS_ERR_OR_NULL(debugfs_info->root)) {
		pr_err("Debugfs create dir failed with error: %ld\n",
					PTR_ERR(debugfs_info->root));
		kfree(debugfs_info);
		return -ENODEV;
	}

	debugfs_create_u32("override_flag", 0644, parent,
			(u32 *)&debugfs_info->override_flag);

	debugfs_create_u32("xres", 0644, debugfs_info->root,
			(u32 *)&debugfs_info->xres);
	debugfs_create_u32("yres", 0644, debugfs_info->root,
					(u32 *)&debugfs_info->yres);

	debugfs_create_u32("h_back_porch", 0644, debugfs_info->root,
			(u32 *)&debugfs_info->lcdc.h_back_porch);
	debugfs_create_u32("h_front_porch", 0644, debugfs_info->root,
			(u32 *)&debugfs_info->lcdc.h_front_porch);
	debugfs_create_u32("h_pulse_width", 0644, debugfs_info->root,
			(u32 *)&debugfs_info->lcdc.h_pulse_width);

	debugfs_create_u32("v_back_porch", 0644, debugfs_info->root,
			(u32 *)&debugfs_info->lcdc.v_back_porch);
	debugfs_create_u32("v_front_porch", 0644, debugfs_info->root,
			(u32 *)&debugfs_info->lcdc.v_front_porch);
	debugfs_create_u32("v_pulse_width", 0644, debugfs_info->root,
			(u32 *)&debugfs_info->lcdc.v_pulse_width);

	debugfs_create_u32("frame_rate", 0644, parent,
			(u32 *)&debugfs_info->frame_rate);

	debugfs_info->xres = panel_info->xres;
	debugfs_info->yres = panel_info->yres;
	debugfs_info->lcdc = panel_info->lcdc;
	debugfs_info->frame_rate = panel_info->mipi.frame_rate;
	debugfs_info->override_flag = 0;

	panel_info->debugfs_info = debugfs_info;
	return 0;
}

int mdss_panel_debugfs_init(struct mdss_panel_info *panel_info)
{
	struct mdss_panel_data *pdata;
	struct dentry *parent;
	char dsi_str[10];
	int dsi_index = 0;
	int rc = 0;

	if (panel_info->type != MIPI_VIDEO_PANEL
		&& panel_info->type != MIPI_CMD_PANEL)
		return -ENOTSUPP;

	pdata = container_of(panel_info, struct mdss_panel_data, panel_info);
	parent = debugfs_create_dir("mdss_panel", NULL);
	if (IS_ERR_OR_NULL(parent)) {
		pr_err("Debugfs create dir failed with error: %ld\n",
			PTR_ERR(parent));
		return -ENODEV;
	}

	do {
		snprintf(dsi_str, sizeof(dsi_str), "dsi%d", dsi_index++);
		rc = mdss_panel_debugfs_setup(&pdata->panel_info, parent,
				dsi_str);
		if (rc) {
			pr_err("error in initilizing panel debugfs\n");
			return rc;
		}
		pdata = pdata->next;
	} while (pdata && dsi_index < NUM_DSI_INTF);

	pr_debug("Initilized mdss_panel_debugfs_info\n");
	return 0;
}

void mdss_panel_debugfs_cleanup(struct mdss_panel_info *panel_info)
{
	struct mdss_panel_data *pdata;
	struct mdss_panel_debugfs_info *debugfs_info;
	pdata = container_of(panel_info, struct mdss_panel_data, panel_info);
	do {
		debugfs_info = pdata->panel_info.debugfs_info;
		if (debugfs_info && debugfs_info->root)
			debugfs_remove_recursive(debugfs_info->root);
		pdata = pdata->next;
	} while (pdata);
	pr_debug("Cleaned up mdss_panel_debugfs_info\n");
}

void mdss_panel_debugfsinfo_to_panelinfo(struct mdss_panel_info *panel_info)
{
	struct mdss_panel_data *pdata;
	struct mdss_panel_info *pinfo;
	pdata = container_of(panel_info, struct mdss_panel_data, panel_info);

	do {
		pinfo = &pdata->panel_info;
		pinfo->xres = pinfo->debugfs_info->xres;
		pinfo->yres = pinfo->debugfs_info->yres;
		pinfo->lcdc = pinfo->debugfs_info->lcdc;
		pinfo->mipi.frame_rate = pinfo->debugfs_info->frame_rate;
		pdata = pdata->next;
	} while (pdata);
}

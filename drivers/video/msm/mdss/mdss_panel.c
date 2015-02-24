/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

int mdss_panel_debugfs_fbc_setup(struct mdss_panel_debugfs_info *debugfs_info,
	struct mdss_panel_info *panel_info, struct dentry *parent)
{
	struct dentry *fbc_root;

	fbc_root = debugfs_create_dir("fbc", parent);
	if (IS_ERR_OR_NULL(fbc_root)) {
		pr_err("Debugfs create fbc dir failed with error: %ld\n",
					PTR_ERR(fbc_root));
		return -ENODEV;
	}

	debugfs_create_bool("enable", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.enabled);
	debugfs_create_u32("bpp", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.target_bpp);
	debugfs_create_u32("packing", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.comp_mode);
	debugfs_create_bool("quant_err", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.qerr_enable);
	debugfs_create_u32("bias", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.cd_bias);
	debugfs_create_bool("pat_mode", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.pat_enable);
	debugfs_create_bool("vlc_mode", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.vlc_enable);
	debugfs_create_bool("bflc_mode", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.bflc_enable);
	debugfs_create_u32("hline_budget", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.line_x_budget);
	debugfs_create_u32("budget_ctrl", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.block_x_budget);
	debugfs_create_u32("block_budget", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.block_budget);
	debugfs_create_u32("lossless_thd", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.lossless_mode_thd);
	debugfs_create_u32("lossy_thd", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.lossy_mode_thd);
	debugfs_create_u32("rgb_thd", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.lossy_rgb_thd);
	debugfs_create_u32("lossy_mode_idx", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.lossy_mode_idx);
	debugfs_create_u32("slice_height", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.slice_height);
	debugfs_create_u32("pred_mode", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.pred_mode);
	debugfs_create_u32("enc_mode", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.enc_mode);
	debugfs_create_u32("max_pred_err", 0644, fbc_root,
			(u32 *)&debugfs_info->fbc.max_pred_err);

	debugfs_info->fbc  = panel_info->fbc;

	return 0;

}

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

	mdss_panel_debugfs_fbc_setup(debugfs_info, panel_info,
		debugfs_info->root);

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
		pinfo->panel_max_vtotal = mdss_panel_get_vtotal(pinfo);
		pinfo->fbc = pinfo->debugfs_info->fbc;
		pinfo->mipi.frame_rate = pinfo->debugfs_info->frame_rate;
		pdata = pdata->next;
	} while (pdata);
}

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
#include <linux/uaccess.h>

#include "mdss_panel.h"

#define NUM_DSI_INTF 2

int mdss_panel_debugfs_fbc_setup(struct mdss_panel_debugfs_info *debugfs_info,
	struct mdss_panel_info *panel_info, struct dentry *parent)
{
	struct dentry *fbc_root;
	struct fbc_panel_info *fbc = &debugfs_info->panel_info.fbc;

	fbc_root = debugfs_create_dir("fbc", parent);
	if (IS_ERR_OR_NULL(fbc_root)) {
		pr_err("Debugfs create fbc dir failed with error: %ld\n",
					PTR_ERR(fbc_root));
		return -ENODEV;
	}

	debugfs_create_bool("enable", 0644, fbc_root,
			(u32 *)&fbc->enabled);
	debugfs_create_u32("bpp", 0644, fbc_root,
			(u32 *)&fbc->target_bpp);
	debugfs_create_u32("packing", 0644, fbc_root,
			(u32 *)&fbc->comp_mode);
	debugfs_create_bool("quant_err", 0644, fbc_root,
			(u32 *)&fbc->qerr_enable);
	debugfs_create_u32("bias", 0644, fbc_root,
			(u32 *)&fbc->cd_bias);
	debugfs_create_bool("pat_mode", 0644, fbc_root,
			(u32 *)&fbc->pat_enable);
	debugfs_create_bool("vlc_mode", 0644, fbc_root,
			(u32 *)&fbc->vlc_enable);
	debugfs_create_bool("bflc_mode", 0644, fbc_root,
			(u32 *)&fbc->bflc_enable);
	debugfs_create_u32("hline_budget", 0644, fbc_root,
			(u32 *)&fbc->line_x_budget);
	debugfs_create_u32("budget_ctrl", 0644, fbc_root,
			(u32 *)&fbc->block_x_budget);
	debugfs_create_u32("block_budget", 0644, fbc_root,
			(u32 *)&fbc->block_budget);
	debugfs_create_u32("lossless_thd", 0644, fbc_root,
			(u32 *)&fbc->lossless_mode_thd);
	debugfs_create_u32("lossy_thd", 0644, fbc_root,
			(u32 *)&fbc->lossy_mode_thd);
	debugfs_create_u32("rgb_thd", 0644, fbc_root,
			(u32 *)&fbc->lossy_rgb_thd);
	debugfs_create_u32("lossy_mode_idx", 0644, fbc_root,
			(u32 *)&fbc->lossy_mode_idx);
	debugfs_create_u32("slice_height", 0644, fbc_root,
			(u32 *)&fbc->slice_height);
	debugfs_create_u32("pred_mode", 0644, fbc_root,
			(u32 *)&fbc->pred_mode);
	debugfs_create_u32("enc_mode", 0644, fbc_root,
			(u32 *)&fbc->enc_mode);
	debugfs_create_u32("max_pred_err", 0644, fbc_root,
			(u32 *)&fbc->max_pred_err);

	debugfs_info->panel_info.fbc = panel_info->fbc;

	return 0;
}

struct array_data {
	void *array;
	u32 elements;
	size_t size; /* size of each data in array */
};

static int panel_debugfs_array_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return nonseekable_open(inode, file);
}

static ssize_t panel_debugfs_array_read(struct file *file, char __user *buf,
				size_t len, loff_t *ppos)
{
	char *buffer, *bufp;
	int buf_size;
	struct array_data *data = file->private_data;
	int i = 0, elements = data->elements;
	ssize_t ret = 0;

	/*
	 * Max size:
	 *  - 10 digits ("0x" + 8 digits value) + ' '/'\n' = 11 bytes per number
	 *  - terminating NUL character
	 */
	buf_size = elements*11 + 1;
	buffer = kmalloc(buf_size, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	bufp = buffer;
	while (i < elements) {
		char term = (i < elements-1) ? ' ' : '\n';

		if (data->size == sizeof(u8)) {
			u8 *array = (u8 *)data->array;
			bufp += snprintf(bufp, buf_size-(bufp-buffer),
						"0x%02x%c", array[i], term);
		} else if (data->size == sizeof(u16)) {
			u16 *array = (u16 *)data->array;
			bufp += snprintf(bufp, buf_size-(bufp-buffer),
						"0x%02x%c", array[i], term);
		} else {
			u32 *array = (u32 *)data->array;
			bufp += snprintf(bufp, buf_size-(bufp-buffer),
						"0x%02x%c", array[i], term);
		}
		i++;
	}
	*bufp = '\0';
	ret = simple_read_from_buffer(buf, len, ppos,
					buffer, bufp-buffer);

	kfree(buffer);
	return ret;
}

static ssize_t panel_debugfs_array_write(struct file *file,
	const char __user *p, size_t count, loff_t *ppos)
{
	struct array_data *data = file->private_data;
	char *buffer, *bufp;
	int buf_size;
	ssize_t res;
	int i = 0, elements = data->elements;

	/*
	 * Max size:
	 *  - 10 digits ("0x" + 8 digits value) + ' '/'\n' = 11 bytes per number
	 *  - terminating NUL character
	 */
	buf_size = elements*11 + 1;
	buffer = kmalloc(buf_size, GFP_KERNEL);
	if (!buffer) {
		pr_err("Failed to allocate memory\n");
		return -ENOMEM;
	}
	res = simple_write_to_buffer(buffer, buf_size, ppos, p, count);
	if (res)
		*ppos += res;

	buffer[buf_size-1] = '\0';
	bufp = buffer;

	while (i < elements) {
		uint32_t value = 0;
		int step = 0;
		if (sscanf(bufp, "%x%n", &value, &step) > 0) {
			if (data->size == sizeof(u8)) {
				u8 *array = (u8 *)data->array;
				*(array+i) = (u8)value;
			} else if (data->size == sizeof(u16)) {
				u16 *array = (u16 *)data->array;
				*(array+i) = (u16)value;
			} else {
				u32 *array = (u32 *)data->array;
				*(array+i) = (u32)value;
			}
			bufp += step;
		}
		i++;
	}
	kfree(buffer);
	return res;
}

static const struct file_operations panel_debugfs_array_fops = {
	.owner = THIS_MODULE,
	.open = panel_debugfs_array_open,
	.read = panel_debugfs_array_read,
	.write = panel_debugfs_array_write,
};

struct dentry *panel_debugfs_create_array(const char *name, umode_t mode,
				struct dentry *parent,
				void *array, size_t size, u32 elements)
{
	struct array_data *data = kmalloc(sizeof(*data), GFP_KERNEL);

	if (data == NULL) {
		pr_err("Failed to allocate memory\n");
		return NULL;
	}

	/* only support integer of 3 kinds of length format */
	if ((size != sizeof(u8)) &&
	    (size != sizeof(u16)) &&
	    (size != sizeof(u32))) {
		pr_warn("Value size %zu bytes is not supported\n", size);
		return NULL;
	}

	data->array = array;
	data->size = size;
	data->elements = elements;

	return debugfs_create_file(name, mode, parent, data,
				&panel_debugfs_array_fops);
}

#define DEBUGFS_CREATE_ARRAY(name, node, array) \
	panel_debugfs_create_array(name, 0644, node, array, \
				   sizeof(array[0]), ARRAY_SIZE(array))

static int _create_phy_ctrl_nodes(struct mdss_panel_debugfs_info *debugfs_info,
	struct dentry *node) {

	struct mdss_panel_info *pinfo = &debugfs_info->panel_info;
	struct dentry *phy_node;

	phy_node = debugfs_create_dir("dsi_phy_ctrl", node);
	if (IS_ERR_OR_NULL(phy_node)) {
		pr_err("Debugfs create phy ctrl node failed with error: %ld\n",
					PTR_ERR(phy_node));
		return -ENODEV;
	}

	DEBUGFS_CREATE_ARRAY("regulator", phy_node,
			     pinfo->mipi.dsi_phy_db.regulator);
	DEBUGFS_CREATE_ARRAY("strength", phy_node,
			     pinfo->mipi.dsi_phy_db.strength);
	DEBUGFS_CREATE_ARRAY("bistctrl", phy_node,
			     pinfo->mipi.dsi_phy_db.bistctrl);
	DEBUGFS_CREATE_ARRAY("lanecfg", phy_node,
			     pinfo->mipi.dsi_phy_db.lanecfg);
	DEBUGFS_CREATE_ARRAY("timing", phy_node,
			     pinfo->mipi.dsi_phy_db.timing);

	return 0;
}

static int _create_dsi_panel_nodes(struct mdss_panel_debugfs_info *dfs,
	struct dentry *parent)
{
	struct dentry *lcdc_root, *mipi_root, *te_root;
	struct mdss_panel_info *pinfo = &dfs->panel_info;

	lcdc_root = debugfs_create_dir("lcdc", parent);
	if (IS_ERR_OR_NULL(lcdc_root)) {
		pr_err("Debugfs create lcdc dir failed with error: %ld\n",
					PTR_ERR(lcdc_root));
		return -ENODEV;
	}
	mipi_root = debugfs_create_dir("mipi", parent);
	if (IS_ERR_OR_NULL(mipi_root)) {
		pr_err("Debugfs create mipi dir failed with error: %ld\n",
					PTR_ERR(mipi_root));
		return -ENODEV;
	}
	te_root = debugfs_create_dir("te", parent);
	if (IS_ERR_OR_NULL(te_root)) {
		pr_err("Debugfs create te check dir failed with error: %ld\n",
					PTR_ERR(te_root));
		return -ENODEV;
	}

	debugfs_create_u32("partial_update_enabled", 0644, dfs->root,
			(u32 *)&pinfo->partial_update_enabled);
	debugfs_create_u32("partial_update_roi_merge", 0644, dfs->root,
			(u32 *)&pinfo->partial_update_roi_merge);
	debugfs_create_u32("dcs_cmd_by_left", 0644, dfs->root,
			(u32 *)&pinfo->dcs_cmd_by_left);
	debugfs_create_bool("ulps_feature_enabled", 0644, dfs->root,
			(u32 *)&pinfo->ulps_feature_enabled);
	debugfs_create_bool("ulps_suspend_enabled", 0644, dfs->root,
			(u32 *)&pinfo->ulps_suspend_enabled);
	debugfs_create_bool("esd_check_enabled", 0644, dfs->root,
			(u32 *)&pinfo->esd_check_enabled);
	debugfs_create_bool("panel_ack_disabled", 0644, dfs->root,
			(u32 *)&pinfo->panel_ack_disabled);

	debugfs_create_u32("hsync_skew", 0644, lcdc_root,
			(u32 *)&pinfo->lcdc.hsync_skew);
	debugfs_create_u32("underflow_clr", 0644, lcdc_root,
			(u32 *)&pinfo->lcdc.underflow_clr);
	debugfs_create_u32("border_clr", 0644, lcdc_root,
			(u32 *)&pinfo->lcdc.border_clr);
	debugfs_create_u32("h_back_porch", 0644, lcdc_root,
			(u32 *)&pinfo->lcdc.h_back_porch);
	debugfs_create_u32("h_front_porch", 0644, lcdc_root,
			(u32 *)&pinfo->lcdc.h_front_porch);
	debugfs_create_u32("h_pulse_width", 0644, lcdc_root,
			(u32 *)&pinfo->lcdc.h_pulse_width);
	debugfs_create_u32("v_back_porch", 0644, lcdc_root,
			(u32 *)&pinfo->lcdc.v_back_porch);
	debugfs_create_u32("v_front_porch", 0644, lcdc_root,
			(u32 *)&pinfo->lcdc.v_front_porch);
	debugfs_create_u32("v_pulse_width", 0644, lcdc_root,
			(u32 *)&pinfo->lcdc.v_pulse_width);

	/* Create mipi related nodes */
	debugfs_create_u8("frame_rate", 0644, mipi_root,
			(char *)&pinfo->mipi.frame_rate);
	debugfs_create_u8("hfp_power_stop", 0644, mipi_root,
			(char *)&pinfo->mipi.hfp_power_stop);
	debugfs_create_u8("hsa_power_stop", 0644, mipi_root,
			(char *)&pinfo->mipi.hsa_power_stop);
	debugfs_create_u8("hbp_power_stop", 0644, mipi_root,
			(char *)&pinfo->mipi.hbp_power_stop);
	debugfs_create_u8("last_line_interleave_en", 0644, mipi_root,
			(char *)&pinfo->mipi.last_line_interleave_en);
	debugfs_create_u8("bllp_power_stop", 0644, mipi_root,
			(char *)&pinfo->mipi.bllp_power_stop);
	debugfs_create_u8("eof_bllp_power_stop", 0644, mipi_root,
			(char *)&pinfo->mipi.eof_bllp_power_stop);
	debugfs_create_u8("data_lane0", 0644, mipi_root,
			(char *)&pinfo->mipi.data_lane0);
	debugfs_create_u8("data_lane1", 0644, mipi_root,
			(char *)&pinfo->mipi.data_lane1);
	debugfs_create_u8("data_lane2", 0644, mipi_root,
			(char *)&pinfo->mipi.data_lane2);
	debugfs_create_u8("data_lane3", 0644, mipi_root,
			(char *)&pinfo->mipi.data_lane3);
	debugfs_create_u8("t_clk_pre", 0644, mipi_root,
			(char *)&pinfo->mipi.t_clk_pre);
	debugfs_create_u8("t_clk_post", 0644, mipi_root,
			(char *)&pinfo->mipi.t_clk_post);
	debugfs_create_u8("stream", 0644, mipi_root,
			(char *)&pinfo->mipi.stream);
	debugfs_create_u8("interleave_mode", 0644, mipi_root,
			(char *)&pinfo->mipi.interleave_mode);
	debugfs_create_u8("vsync_enable", 0644, mipi_root,
			(char *)&pinfo->mipi.vsync_enable);
	debugfs_create_u8("hw_vsync_mode", 0644, mipi_root,
			(char *)&pinfo->mipi.hw_vsync_mode);
	debugfs_create_u8("te_sel", 0644, mipi_root,
			(char *)&pinfo->mipi.te_sel);
	debugfs_create_u8("insert_dcs_cmd", 0644, mipi_root,
			(char *)&pinfo->mipi.insert_dcs_cmd);
	debugfs_create_u8("wr_mem_start", 0644, mipi_root,
			(char *)&pinfo->mipi.wr_mem_start);
	debugfs_create_u8("wr_mem_continue", 0644, mipi_root,
			(char *)&pinfo->mipi.wr_mem_continue);
	debugfs_create_u8("pulse_mode_hsa_he", 0644, mipi_root,
			(char *)&pinfo->mipi.pulse_mode_hsa_he);
	debugfs_create_u8("vc", 0644, mipi_root, (char *)&pinfo->mipi.vc);
	debugfs_create_u8("lp11_init", 0644, mipi_root,
			(char *)&pinfo->mipi.lp11_init);
	debugfs_create_u32("init_delay", 0644, mipi_root,
			(u32 *)&pinfo->mipi.init_delay);
	debugfs_create_u8("rx_eot_ignore", 0644, mipi_root,
			(char *)&pinfo->mipi.rx_eot_ignore);
	debugfs_create_u8("tx_eot_append", 0644, mipi_root,
			(char *)&pinfo->mipi.tx_eot_append);

	/* TE reltaed nodes */
	debugfs_create_u32("te_tear_check_en", 0644, te_root,
			(u32 *)&pinfo->te.tear_check_en);
	debugfs_create_u32("te_sync_cfg_height", 0644, te_root,
			(u32 *)&pinfo->te.sync_cfg_height);
	debugfs_create_u32("te_vsync_init_val", 0644, te_root,
			(u32 *)&pinfo->te.vsync_init_val);
	debugfs_create_u32("te_sync_threshold_start", 0644, te_root,
			(u32 *)&pinfo->te.sync_threshold_start);
	debugfs_create_u32("te_sync_threshold_continue", 0644, te_root,
			(u32 *)&pinfo->te.sync_threshold_continue);
	debugfs_create_u32("te_start_pos", 0644, te_root,
			(u32 *)&pinfo->te.sync_threshold_continue);
	debugfs_create_u32("te_rd_ptr_irq", 0644, te_root,
			(u32 *)&pinfo->te.rd_ptr_irq);
	debugfs_create_u32("te_refx100", 0644, te_root,
			(u32 *)&pinfo->te.refx100);

	return 0;
}

int mdss_panel_debugfs_panel_setup(struct mdss_panel_debugfs_info *debugfs_info,
	struct mdss_panel_info *panel_info, struct dentry *parent)
{
	/* create panel info nodes */
	debugfs_create_u32("xres", 0644, debugfs_info->root,
		(u32 *)&debugfs_info->panel_info.xres);
	debugfs_create_u32("yres", 0644, debugfs_info->root,
		(u32 *)&debugfs_info->panel_info.yres);
	debugfs_create_u32("dynamic_fps", 0644, debugfs_info->root,
		(u32 *)&debugfs_info->panel_info.dynamic_fps);
	debugfs_create_u32("physical_width", 0644, debugfs_info->root,
		(u32 *)&debugfs_info->panel_info.physical_width);
	debugfs_create_u32("physical_height", 0644, debugfs_info->root,
		(u32 *)&debugfs_info->panel_info.physical_height);
	debugfs_create_u32("min_refresh_rate", 0644, debugfs_info->root,
		(u32 *)&debugfs_info->panel_info.min_fps);
	debugfs_create_u32("max_refresh_rate", 0644, debugfs_info->root,
		(u32 *)&debugfs_info->panel_info.max_fps);
	debugfs_create_u32("clk_rate", 0644, debugfs_info->root,
		(u32 *)&debugfs_info->panel_info.clk_rate);
	debugfs_create_u32("bl_min", 0644, debugfs_info->root,
		(u32 *)&debugfs_info->panel_info.bl_min);
	debugfs_create_u32("bl_max", 0644, debugfs_info->root,
		(u32 *)&debugfs_info->panel_info.bl_max);
	debugfs_create_u32("brightness_max", 0644, debugfs_info->root,
		(u32 *)&debugfs_info->panel_info.brightness_max);

	if ((panel_info->type == MIPI_CMD_PANEL) ||
	    (panel_info->type == MIPI_VIDEO_PANEL)) {
		_create_dsi_panel_nodes(debugfs_info, debugfs_info->root);
		_create_phy_ctrl_nodes(debugfs_info, debugfs_info->root);
	}

	debugfs_info->panel_info = *panel_info;
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

	mdss_panel_debugfs_fbc_setup(debugfs_info, panel_info,
				debugfs_info->root);
	mdss_panel_debugfs_panel_setup(debugfs_info, panel_info,
				debugfs_info->root);

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

void mdss_panel_override_te_params(struct mdss_panel_info *pinfo)
{
	pinfo->te.sync_cfg_height = mdss_panel_get_vtotal(pinfo);
	pinfo->te.vsync_init_val = 0;
	pinfo->te.start_pos = 5;
	pinfo->te.rd_ptr_irq = 1;
	pr_debug("SW TE override: read_ptr:%d,start_pos:%d,height:%d,init_val:%d\n",
		pinfo->te.rd_ptr_irq, pinfo->te.start_pos,
		pinfo->te.sync_cfg_height,
		pinfo->te.vsync_init_val);
}

void mdss_panel_debugfsinfo_to_panelinfo(struct mdss_panel_info *panel_info)
{
	struct mdss_panel_data *pdata;
	struct mdss_panel_info *pinfo;
	struct mdss_panel_debugfs_info *dfs_info;
	pdata = container_of(panel_info, struct mdss_panel_data, panel_info);

	do {
		pinfo = &pdata->panel_info;
		dfs_info = pinfo->debugfs_info;

		pinfo->xres = dfs_info->panel_info.xres;
		pinfo->yres = dfs_info->panel_info.yres;
		pinfo->dynamic_fps = dfs_info->panel_info.dynamic_fps;
		pinfo->physical_width = dfs_info->panel_info.physical_width;
		pinfo->physical_height = dfs_info->panel_info.physical_height;
		pinfo->min_fps = dfs_info->panel_info.min_fps;
		pinfo->max_fps = dfs_info->panel_info.max_fps;
		pinfo->clk_rate = dfs_info->panel_info.clk_rate;
		pinfo->bl_min = dfs_info->panel_info.bl_min;
		pinfo->bl_max = dfs_info->panel_info.bl_max;
		pinfo->brightness_max = dfs_info->panel_info.brightness_max;

		if ((pinfo->type == MIPI_CMD_PANEL) ||
		    (pinfo->type == MIPI_VIDEO_PANEL)) {
			pinfo->fbc = dfs_info->panel_info.fbc;
			pinfo->lcdc = dfs_info->panel_info.lcdc;
			pinfo->mipi = dfs_info->panel_info.mipi;
			pinfo->te = dfs_info->panel_info.te;
			pinfo->partial_update_enabled =
				dfs_info->panel_info.partial_update_enabled;
			pinfo->partial_update_roi_merge =
				dfs_info->panel_info.partial_update_roi_merge;
			pinfo->dcs_cmd_by_left =
				dfs_info->panel_info.dcs_cmd_by_left;
			pinfo->ulps_feature_enabled =
				dfs_info->panel_info.ulps_feature_enabled;
			pinfo->ulps_suspend_enabled =
				dfs_info->panel_info.ulps_suspend_enabled;
			pinfo->esd_check_enabled =
				dfs_info->panel_info.esd_check_enabled;
			pinfo->panel_ack_disabled =
				dfs_info->panel_info.panel_ack_disabled;
		}

		pinfo->panel_max_vtotal = mdss_panel_get_vtotal(pinfo);

		/* override te parameters if panel is in sw te mode */
		if (panel_info->sim_panel_mode == SIM_SW_TE_MODE)
			mdss_panel_override_te_params(panel_info);

		pdata = pdata->next;
	} while (pdata);
}

struct mdss_panel_timing *mdss_panel_get_timing_by_name(
		struct mdss_panel_data *pdata,
		const char *name)
{
	struct mdss_panel_timing *pt;

	if (name) {
		list_for_each_entry(pt, &pdata->timings_list, list)
			if (pt->name && !strcmp(pt->name, name))
				return pt;
	}

	return NULL;
}

void mdss_panel_info_from_timing(struct mdss_panel_timing *pt,
		struct mdss_panel_info *pinfo)
{
	pinfo->clk_rate = pt->clk_rate;
	pinfo->xres = pt->xres;
	pinfo->lcdc.h_front_porch = pt->h_front_porch;
	pinfo->lcdc.h_back_porch = pt->h_back_porch;
	pinfo->lcdc.h_pulse_width = pt->h_pulse_width;

	pinfo->yres = pt->yres;
	pinfo->lcdc.v_front_porch = pt->v_front_porch;
	pinfo->lcdc.v_back_porch = pt->v_back_porch;
	pinfo->lcdc.v_pulse_width = pt->v_pulse_width;

	pinfo->lcdc.border_bottom = pt->border_bottom;
	pinfo->lcdc.border_top = pt->border_top;
	pinfo->lcdc.border_left = pt->border_left;
	pinfo->lcdc.border_right = pt->border_right;
	pinfo->lcdc.xres_pad = pt->border_left + pt->border_right;
	pinfo->lcdc.yres_pad = pt->border_top + pt->border_bottom;

	pinfo->mipi.frame_rate = pt->frame_rate;
	pinfo->edp.frame_rate = pinfo->mipi.frame_rate;

	pinfo->fbc = pt->fbc;
	pinfo->te = pt->te;
}

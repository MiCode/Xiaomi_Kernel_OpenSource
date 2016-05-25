/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

#define NUM_INTF 2

/*
 * rc_buf_thresh = {896, 1792, 2688, 3548, 4480, 5376, 6272, 6720,
 *		7168, 7616, 7744, 7872, 8000, 8064, 8192};
 *	(x >> 6) & 0x0ff)
 */
static u32 dsc_rc_buf_thresh[] = {0x0e, 0x1c, 0x2a, 0x38, 0x46, 0x54,
		0x62, 0x69, 0x70, 0x77, 0x79, 0x7b, 0x7d, 0x7e};
static char dsc_rc_range_min_qp_1_1[] = {0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5,
				5, 5, 7, 13};
static char dsc_rc_range_min_qp_1_1_scr1[] = {0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5,
				5, 5, 9, 12};
static char dsc_rc_range_max_qp_1_1[] = {4, 4, 5, 6, 7, 7, 7, 8, 9, 10, 11,
			 12, 13, 13, 15};
static char dsc_rc_range_max_qp_1_1_scr1[] = {4, 4, 5, 6, 7, 7, 7, 8, 9, 10, 10,
			 11, 11, 12, 13};
static char dsc_rc_range_bpg_offset[] = {2, 0, 0, -2, -4, -6, -8, -8,
			-8, -10, -10, -12, -12, -12, -12};

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
		kfree(data);
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
	debugfs_create_u32("adjust_timer_ms", 0644, mipi_root,
			(u32 *)&pinfo->adjust_timer_delay_ms);

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
	debugfs_create_u64("clk_rate", 0644, debugfs_info->root,
		(u64 *)&debugfs_info->panel_info.clk_rate);
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
		*parent, char *intf_str)
{
	struct mdss_panel_debugfs_info *debugfs_info;
	debugfs_info = kzalloc(sizeof(*debugfs_info), GFP_KERNEL);
	if (!debugfs_info) {
		pr_err("No memory to create panel debugfs info");
		return -ENOMEM;
	}

	debugfs_info->root = debugfs_create_dir(intf_str, parent);
	if (IS_ERR_OR_NULL(debugfs_info->root)) {
		pr_err("Debugfs create dir failed with error: %ld\n",
					PTR_ERR(debugfs_info->root));
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

int mdss_panel_debugfs_init(struct mdss_panel_info *panel_info,
		char const *panel_name)
{
	struct mdss_panel_data *pdata;
	struct dentry *parent;
	char intf_str[10];
	int intf_index = 0;
	int rc = 0;

	if (panel_info->type != MIPI_VIDEO_PANEL
		&& panel_info->type != MIPI_CMD_PANEL)
		return -ENOTSUPP;

	pdata = container_of(panel_info, struct mdss_panel_data, panel_info);
	parent = debugfs_create_dir(panel_name, NULL);
	if (IS_ERR_OR_NULL(parent)) {
		pr_err("Debugfs create dir failed with error: %ld\n",
			PTR_ERR(parent));
		return -ENODEV;
	}

	do {
		snprintf(intf_str, sizeof(intf_str), "intf%d", intf_index++);
		rc = mdss_panel_debugfs_setup(&pdata->panel_info, parent,
				intf_str);
		if (rc) {
			pr_err("error in initilizing panel debugfs\n");
			return rc;
		}
		pdata = pdata->next;
	} while (pdata && intf_index < NUM_INTF);

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
	pinfo->te.vsync_init_val = pinfo->yres;
	pinfo->te.start_pos = pinfo->yres;
	pinfo->te.rd_ptr_irq = pinfo->yres + 1;
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
		pinfo->adjust_timer_delay_ms =
			dfs_info->panel_info.adjust_timer_delay_ms;

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

	if (pdata && name) {
		list_for_each_entry(pt, &pdata->timings_list, list)
			if (pt->name && !strcmp(pt->name, name))
				return pt;
	}

	return NULL;
}

void mdss_panel_info_from_timing(struct mdss_panel_timing *pt,
		struct mdss_panel_info *pinfo)
{
	if (!pt || !pinfo)
		return;

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

	pinfo->lm_widths[0] = pt->lm_widths[0];
	pinfo->lm_widths[1] = pt->lm_widths[1];

	pinfo->mipi.frame_rate = pt->frame_rate;
	pinfo->edp.frame_rate = pinfo->mipi.frame_rate;

	pinfo->dsc = pt->dsc;
	pinfo->dsc_enc_total = pt->dsc_enc_total;
	pinfo->fbc = pt->fbc;
	pinfo->compression_mode = pt->compression_mode;

	pinfo->te = pt->te;

	/* override te parameters if panel is in sw te mode */
	if (pinfo->sim_panel_mode == SIM_SW_TE_MODE)
		mdss_panel_override_te_params(pinfo);
}

/*
 * All the calculations done by this routine only depend on slice_width
 * and slice_height. They are independent of picture dimesion and dsc_merge.
 * Thus this function should be called only when slice dimension changes.
 * Since currently we don't support dynamic slice dimension changes, this
 * routine shall be called only during probe.
 */
void mdss_panel_dsc_parameters_calc(struct dsc_desc *dsc)
{
	int bpp, bpc;
	int mux_words_size;
	int groups_per_line, groups_total;
	int min_rate_buffer_size;
	int hrd_delay;
	int pre_num_extra_mux_bits, num_extra_mux_bits;
	int slice_bits;
	int target_bpp_x16;
	int data;
	int final_value, final_scale;

	dsc->rc_model_size = 8192;	/* rate_buffer_size */
	if (dsc->version == 0x11 && dsc->scr_rev == 0x1)
		dsc->first_line_bpg_offset = 15;
	else
		dsc->first_line_bpg_offset = 12;
	dsc->min_qp_flatness = 3;
	dsc->max_qp_flatness = 12;
	dsc->line_buf_depth = 9;

	dsc->edge_factor = 6;
	dsc->quant_incr_limit0 = 11;
	dsc->quant_incr_limit1 = 11;
	dsc->tgt_offset_hi = 3;
	dsc->tgt_offset_lo = 3;

	dsc->buf_thresh = dsc_rc_buf_thresh;
	if (dsc->version == 0x11 && dsc->scr_rev == 0x1) {
		dsc->range_min_qp = dsc_rc_range_min_qp_1_1_scr1;
		dsc->range_max_qp = dsc_rc_range_max_qp_1_1_scr1;
	} else {
		dsc->range_min_qp = dsc_rc_range_min_qp_1_1;
		dsc->range_max_qp = dsc_rc_range_max_qp_1_1;
	}
	dsc->range_bpg_offset = dsc_rc_range_bpg_offset;

	bpp = dsc->bpp;
	bpc = dsc->bpc;

	if (bpp == 8)
		dsc->initial_offset = 6144;
	else
		dsc->initial_offset = 2048;	/* bpp = 12 */

	if (bpc == 8)
		mux_words_size = 48;
	else
		mux_words_size = 64;	/* bpc == 12 */

	dsc->slice_last_group_size = 3 - (dsc->slice_width % 3);

	dsc->det_thresh_flatness = 7 + 2*(bpc - 8);

	dsc->initial_xmit_delay = dsc->rc_model_size / (2 * bpp);

	groups_per_line = DIV_ROUND_UP(dsc->slice_width, 3);

	dsc->chunk_size = dsc->slice_width * bpp / 8;
	if ((dsc->slice_width * bpp) % 8)
		dsc->chunk_size++;

	/* rbs-min */
	min_rate_buffer_size =  dsc->rc_model_size - dsc->initial_offset +
			dsc->initial_xmit_delay * bpp +
			groups_per_line * dsc->first_line_bpg_offset;

	hrd_delay = DIV_ROUND_UP(min_rate_buffer_size, bpp);

	dsc->initial_dec_delay = hrd_delay - dsc->initial_xmit_delay;

	dsc->initial_scale_value = 8 * dsc->rc_model_size /
			(dsc->rc_model_size - dsc->initial_offset);

	slice_bits = 8 * dsc->chunk_size * dsc->slice_height;

	groups_total = groups_per_line * dsc->slice_height;

	data = dsc->first_line_bpg_offset * 2048;

	dsc->nfl_bpg_offset = DIV_ROUND_UP(data, (dsc->slice_height - 1));

	pre_num_extra_mux_bits = 3 * (mux_words_size + (4 * bpc + 4) - 2);

	num_extra_mux_bits = pre_num_extra_mux_bits - (mux_words_size -
		((slice_bits - pre_num_extra_mux_bits) % mux_words_size));

	data = 2048 * (dsc->rc_model_size - dsc->initial_offset
		+ num_extra_mux_bits);
	dsc->slice_bpg_offset = DIV_ROUND_UP(data, groups_total);

	/* bpp * 16 + 0.5 */
	data = bpp * 16;
	data *= 2;
	data++;
	data /= 2;
	target_bpp_x16 = data;

	data = (dsc->initial_xmit_delay * target_bpp_x16) / 16;
	final_value =  dsc->rc_model_size - data + num_extra_mux_bits;

	final_scale = 8 * dsc->rc_model_size /
		(dsc->rc_model_size - final_value);

	dsc->final_offset = final_value;

	data = (final_scale - 9) * (dsc->nfl_bpg_offset +
		dsc->slice_bpg_offset);
	dsc->scale_increment_interval = (2048 * dsc->final_offset) / data;

	dsc->scale_decrement_interval = groups_per_line /
		(dsc->initial_scale_value - 8);

	pr_debug("initial_xmit_delay=%d\n", dsc->initial_xmit_delay);
	pr_debug("bpg_offset, nfl=%d slice=%d\n",
		dsc->nfl_bpg_offset, dsc->slice_bpg_offset);
	pr_debug("groups_per_line=%d chunk_size=%d\n",
		groups_per_line, dsc->chunk_size);
	pr_debug("min_rate_buffer_size=%d hrd_delay=%d\n",
		min_rate_buffer_size, hrd_delay);
	pr_debug("initial_dec_delay=%d initial_scale_value=%d\n",
		dsc->initial_dec_delay, dsc->initial_scale_value);
	pr_debug("slice_bits=%d, groups_total=%d\n", slice_bits, groups_total);
	pr_debug("first_line_bgp_offset=%d slice_height=%d\n",
		dsc->first_line_bpg_offset, dsc->slice_height);
	pr_debug("final_value=%d final_scale=%d\n", final_value, final_scale);
	pr_debug("sacle_increment_interval=%d scale_decrement_interval=%d\n",
		dsc->scale_increment_interval, dsc->scale_decrement_interval);
}

void mdss_panel_dsc_update_pic_dim(struct dsc_desc *dsc,
	int pic_width, int pic_height)
{
	if (!dsc || !pic_width || !pic_height) {
		pr_err("Error: invalid input. pic_width=%d pic_height=%d\n",
			pic_width, pic_height);
		return;
	}

	if ((pic_width % dsc->slice_width) ||
	    (pic_height % dsc->slice_height)) {
		pr_err("Error: pic_dim=%dx%d has to be multiple of slice_dim=%dx%d\n",
			pic_width, pic_height,
			dsc->slice_width, dsc->slice_height);
		return;
	}

	dsc->pic_width = pic_width;
	dsc->pic_height = pic_height;
}

void mdss_panel_dsc_initial_line_calc(struct dsc_desc *dsc, int enc_ip_width)
{
	int ssm_delay, total_pixels, soft_slice_per_enc;

#define MAX_XMIT_DELAY 512
	if (!dsc || !enc_ip_width || !dsc->slice_width ||
	    (enc_ip_width < dsc->slice_width) ||
	    (dsc->initial_xmit_delay > MAX_XMIT_DELAY)) {
		pr_err("Error: invalid input\n");
		return;
	}
#undef MAX_XMIT_DELAY

	soft_slice_per_enc = enc_ip_width / dsc->slice_width;
	ssm_delay = ((dsc->bpc < 10) ? 84 : 92);
	total_pixels = ssm_delay * 3 + dsc->initial_xmit_delay + 47;
	if (soft_slice_per_enc > 1)
		total_pixels += (ssm_delay * 3);

	dsc->initial_lines = DIV_ROUND_UP(total_pixels, dsc->slice_width);
}

void mdss_panel_dsc_pclk_param_calc(struct dsc_desc *dsc, int intf_width)
{
	int slice_per_pkt, slice_per_intf;
	int bytes_in_slice, total_bytes_per_intf;

	if (!dsc || !dsc->slice_width || !dsc->slice_per_pkt ||
	    (intf_width < dsc->slice_width)) {
		pr_err("Error: invalid input. intf_width=%d slice_width=%d\n",
			intf_width,
			dsc ? dsc->slice_width : -1);
		return;
	}

	slice_per_pkt = dsc->slice_per_pkt;
	slice_per_intf = DIV_ROUND_UP(intf_width, dsc->slice_width);

	/*
	 * If slice_per_pkt is greater than slice_per_intf then default to 1.
	 * This can happen during partial update.
	 */
	if (slice_per_pkt > slice_per_intf)
		slice_per_pkt = 1;

	bytes_in_slice = DIV_ROUND_UP(dsc->slice_width * dsc->bpp, 8);
	total_bytes_per_intf = bytes_in_slice * slice_per_intf;

	dsc->eol_byte_num = total_bytes_per_intf % 3;
	dsc->pclk_per_line =  DIV_ROUND_UP(total_bytes_per_intf, 3);
	dsc->bytes_in_slice = bytes_in_slice;
	dsc->bytes_per_pkt = bytes_in_slice * slice_per_pkt;
	dsc->pkt_per_line = slice_per_intf / slice_per_pkt;

	pr_debug("slice_per_pkt=%d slice_per_intf=%d bytes_in_slice=%d total_bytes_per_intf=%d\n",
		slice_per_pkt, slice_per_intf,
		bytes_in_slice, total_bytes_per_intf);
}

int mdss_panel_dsc_prepare_pps_buf(struct dsc_desc *dsc, char *buf,
	int pps_id)
{
	char *bp;
	char data;
	int i, bpp;

	bp = buf;
	*bp++ = (dsc->version & 0xff);	/* pps0 */
	*bp++ = (pps_id & 0xff);		/* pps1 */
	bp++;					/* pps2, reserved */

	data = dsc->line_buf_depth & 0x0f;
	data |= ((dsc->bpc & 0xf) << 4);
	*bp++ = data;				 /* pps3 */

	bpp = dsc->bpp;
	bpp <<= 4;	/* 4 fraction bits */
	data = (bpp >> 8);
	data &= 0x03;		/* upper two bits */
	data |= ((dsc->block_pred_enable & 0x1) << 5);
	data |= ((dsc->convert_rgb & 0x1) << 4);
	data |= ((dsc->enable_422 & 0x1) << 3);
	data |= ((dsc->vbr_enable & 0x1) << 2);
	*bp++ = data;				/* pps4 */
	*bp++ = (bpp & 0xff);			/* pps5 */

	*bp++ = ((dsc->pic_height >> 8) & 0xff); /* pps6 */
	*bp++ = (dsc->pic_height & 0x0ff);	/* pps7 */
	*bp++ = ((dsc->pic_width >> 8) & 0xff);	/* pps8 */
	*bp++ = (dsc->pic_width & 0x0ff);	/* pps9 */

	*bp++ = ((dsc->slice_height >> 8) & 0xff);/* pps10 */
	*bp++ = (dsc->slice_height & 0x0ff);	/* pps11 */
	*bp++ = ((dsc->slice_width >> 8) & 0xff); /* pps12 */
	*bp++ = (dsc->slice_width & 0x0ff);	/* pps13 */

	*bp++ = ((dsc->chunk_size >> 8) & 0xff);/* pps14 */
	*bp++ = (dsc->chunk_size & 0x0ff);	/* pps15 */

	*bp++ = (dsc->initial_xmit_delay >> 8) & 0x3; /* pps16, bit 0, 1 */
	*bp++ = (dsc->initial_xmit_delay & 0xff);/* pps17 */

	*bp++ = ((dsc->initial_dec_delay >> 8) & 0xff);	/* pps18 */
	*bp++ = (dsc->initial_dec_delay & 0xff);/* pps19 */

	bp++;					/* pps20, reserved */

	*bp++ = (dsc->initial_scale_value & 0x3f); /* pps21 */

	*bp++ = ((dsc->scale_increment_interval >> 8) & 0xff); /* pps22 */
	*bp++ = (dsc->scale_increment_interval & 0xff);	/* pps23 */

	*bp++ = ((dsc->scale_decrement_interval >> 8) & 0xf); /* pps24 */
	*bp++ = (dsc->scale_decrement_interval & 0x0ff);/* pps25 */

	bp++;			/* pps26, reserved */

	*bp++ = (dsc->first_line_bpg_offset & 0x1f);/* pps27 */

	*bp++ = ((dsc->nfl_bpg_offset >> 8) & 0xff);/* pps28 */
	*bp++ = (dsc->nfl_bpg_offset & 0x0ff);	/* pps29 */
	*bp++ = ((dsc->slice_bpg_offset >> 8) & 0xff);/* pps30 */
	*bp++ = (dsc->slice_bpg_offset & 0x0ff);/* pps31 */

	*bp++ = ((dsc->initial_offset >> 8) & 0xff);/* pps32 */
	*bp++ = (dsc->initial_offset & 0x0ff);	/* pps33 */

	*bp++ = ((dsc->final_offset >> 8) & 0xff);/* pps34 */
	*bp++ = (dsc->final_offset & 0x0ff);	/* pps35 */

	*bp++ = (dsc->min_qp_flatness & 0x1f);	/* pps36 */
	*bp++ = (dsc->max_qp_flatness & 0x1f);	/* pps37 */

	*bp++ = ((dsc->rc_model_size >> 8) & 0xff);/* pps38 */
	*bp++ = (dsc->rc_model_size & 0x0ff);	/* pps39 */

	*bp++ = (dsc->edge_factor & 0x0f);	/* pps40 */

	*bp++ = (dsc->quant_incr_limit0 & 0x1f);	/* pps41 */
	*bp++ = (dsc->quant_incr_limit1 & 0x1f);	/* pps42 */

	data = ((dsc->tgt_offset_hi & 0xf) << 4);
	data |= (dsc->tgt_offset_lo & 0x0f);
	*bp++ = data;				/* pps43 */

	for (i = 0; i < 14; i++)
		*bp++ = (dsc->buf_thresh[i] & 0xff);/* pps44 - pps57 */

	for (i = 0; i < 15; i++) {		/* pps58 - pps87 */
		data = (dsc->range_min_qp[i] & 0x1f); /* 5 bits */
		data <<= 3;
		data |= ((dsc->range_max_qp[i] >> 2) & 0x07); /* 3 bits */
		*bp++ = data;
		data = (dsc->range_max_qp[i] & 0x03); /* 2 bits */
		data <<= 6;
		data |= (dsc->range_bpg_offset[i] & 0x3f); /* 6 bits */
		*bp++ = data;
	}

	/* pps88 to pps127 are reserved */

	return DSC_PPS_LEN;	/* 128 */
}

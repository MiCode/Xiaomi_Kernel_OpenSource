/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#define pr_fmt(fmt)	"mi-dsi-panel:[%s:%d] " fmt, __func__, __LINE__
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <video/mipi_display.h>

#include "dsi_panel.h"
#include "dsi_display.h"
#include "dsi_ctrl_hw.h"
#include "dsi_parser.h"
#include "dsi_mi_feature.h"
#include "xiaomi_frame_stat.h"

#if DSI_READ_WRITE_PANEL_DEBUG
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static struct proc_dir_entry *mipi_proc_entry = NULL;
#define MIPI_PROC_NAME "mipi_reg"
#endif

#define to_dsi_display(x) container_of(x, struct dsi_display, host)
#define XY_COORDINATE_NUM    2
#define MAX_LUMINANCE_NUM    2

extern bool g_idleflag;
extern struct frame_stat fm_stat;
static struct dsi_read_config g_dsi_read_cfg;
struct dsi_panel *g_panel;
static void panelon_dimming_enable_delayed_work(struct work_struct *work);

int dsi_panel_parse_mi_config(struct dsi_panel *panel,
				struct device_node *of_node)
{
	int rc = 0;
	u32 xy_coordinate[XY_COORDINATE_NUM] = {0};
	u32 max_luminance[MAX_LUMINANCE_NUM] = {0};

	struct dsi_parser_utils *utils = &panel->utils;
	struct dsi_panel_mi_cfg *mi_cfg = &panel->mi_cfg;
	u32 length = 0;
	const u32 *arr;

	mi_cfg->dsi_panel = panel;
	g_panel = panel;

	mi_cfg->bl_is_big_endian = utils->read_bool(utils->data,
			"mi,mdss-dsi-bl-dcs-big-endian-type");

	mi_cfg->mi_feature_enabled = utils->read_bool(of_node,
			"mi,feature-enabled");
	if (mi_cfg->mi_feature_enabled) {
		DSI_INFO("mi feature enabled\n");
	} else {
		DSI_INFO("mi feature disabled\n");
		return 0;
	}

	mi_cfg->hbm_51_ctrl_flag = utils->read_bool(utils->data,
		"mi,mdss-dsi-panel-hbm-51-ctrl-flag");
	if (mi_cfg->hbm_51_ctrl_flag) {
		rc = utils->read_u32(of_node,
			"mi,mdss-dsi-panel-hbm-off-51-index", &mi_cfg->hbm_off_51_index);
		if (rc) {
			pr_err("mi,mdss-dsi-panel-hbm-off-51-index not defined,but need\n");
		}
		rc = utils->read_u32(of_node,
			"mi,mdss-dsi-panel-fod-off-51-index", &mi_cfg->fod_off_51_index);
		if (rc) {
			pr_err("mi,mdss-dsi-panel-fod-off-51-index not defined,but need\n");
		}
		mi_cfg->vi_setting_enabled = utils->read_bool(of_node,
			"mi,mdss-dsi-panel-vi-setting-enabled");
		DSI_INFO("mi vi_setting_enabled = %d\n", mi_cfg->vi_setting_enabled);
	}

	mi_cfg->dimming_on_enable = utils->read_bool(of_node,
		"mi,mdss-panel-on-dimming-enable");
	if (mi_cfg->dimming_on_enable) {
		pr_info("dimming on enable after panel on.\n");
	} else {
		pr_info("dimming on disable after panel on.\n");
	}

	mi_cfg->fod_dimlayer_enabled = utils->read_bool(of_node,
		"mi,mdss-dsi-panel-fod-dimlayer-enabled");
	if (mi_cfg->fod_dimlayer_enabled) {
		pr_info("fod dimlayer enabled.\n");
	} else {
		pr_info("fod dimlayer disabled.\n");
	}

	mi_cfg->nolp_command_set_backlight_enabled = utils->read_bool(of_node,
		"qcom,mdss-dsi-nolp-command-set-backlight-enabled");
	if (mi_cfg->nolp_command_set_backlight_enabled) {
		pr_info("nolp command set backlight enabled.\n");
	} else {
		pr_info("nolp command set backlight disabled.\n");
	}

	rc = utils->read_u32(of_node,
		"mi,mdss-dsi-doze-hbm-brightness-value",
		&mi_cfg->doze_hbm_brightness);
	if (rc || mi_cfg->doze_hbm_brightness <= 0) {
		pr_err("can't get doze hbm brightness\n");
	}

	rc = utils->read_u32(of_node,
		"mi,mdss-dsi-doze-lbm-brightness-value",
		&mi_cfg->doze_lbm_brightness);
	if (rc || mi_cfg->doze_lbm_brightness <= 0) {
		pr_err("can't get doze lbm brightness\n");
	}

	if (mi_cfg->fod_dimlayer_enabled) {
		mi_cfg->prepare_before_fod_hbm_on = utils->read_bool(of_node,
			"mi,mdss-panel-prepare-before-fod-hbm-on");
		if (mi_cfg->prepare_before_fod_hbm_on) {
			pr_info("fod hbm on need prepare.\n");
		} else {
			pr_info("fod hbm on doesn't need prepare.\n");
		}

		mi_cfg->fod_hbm_off_delay = utils->read_bool(of_node,
			"mi,mdss-panel-fod-hbm-off-delay");
		if (mi_cfg->fod_hbm_off_delay) {
			pr_info("fod hbm off delay.\n");
		} else {
			pr_info("fod hbm off no delay.\n");
		}

		rc = utils->read_u32(of_node,
			"mi,mdss-dsi-dimlayer-brightness-alpha-lut-item-count",
			&mi_cfg->brightnes_alpha_lut_item_count);
		if (rc || mi_cfg->brightnes_alpha_lut_item_count <= 0) {
			pr_err("can't get brightnes_alpha_lut_item_count\n");
			mi_cfg->fod_dimlayer_enabled = false;
			goto skip_dimlayer_parse;
		}

		arr = utils->get_property(utils->data,
				"mi,mdss-dsi-dimlayer-brightness-alpha-lut", &length);

		length = length / sizeof(u32);

		pr_info("length: %d\n", length);
		if (!arr || length & 0x1 || length != mi_cfg->brightnes_alpha_lut_item_count * 2) {
			pr_err("read mi,mdss-dsi-dimlayer-brightness-alpha-lut failed\n");
			mi_cfg->fod_dimlayer_enabled = false;
			goto skip_dimlayer_parse;
		}

		mi_cfg->brightness_alpha_lut = (struct brightness_alpha *)kzalloc(length * sizeof(u32), GFP_KERNEL);
		if (!mi_cfg->brightness_alpha_lut) {
			pr_err("no memory for brightnes alpha lut\n");
			mi_cfg->fod_dimlayer_enabled = false;
			goto skip_dimlayer_parse;
		}

		rc = utils->read_u32_array(utils->data, "mi,mdss-dsi-dimlayer-brightness-alpha-lut",
			(u32 *)mi_cfg->brightness_alpha_lut, length);
		if (rc) {
			pr_err("cannot read mi,mdss-dsi-dimlayer-brightness-alpha-lut\n");
			mi_cfg->fod_dimlayer_enabled = false;
			kfree(mi_cfg->brightness_alpha_lut);
			goto skip_dimlayer_parse;
		}
	}

skip_dimlayer_parse:
	rc = utils->read_u32(of_node,
		"mi,mdss-panel-on-dimming-delay", &mi_cfg->panel_on_dimming_delay);
	if (rc) {
		mi_cfg->panel_on_dimming_delay = 0;
		pr_info("panel on dimming delay disabled\n");
	} else {
		pr_info("panel on dimming delay %d ms\n", mi_cfg->panel_on_dimming_delay);
	}

	rc = utils->read_u32(of_node,
			"mi,disp-doze-backlight-threshold", &mi_cfg->doze_backlight_threshold);
	if (rc) {
		mi_cfg->doze_backlight_threshold = DOZE_MIN_BRIGHTNESS_LEVEL;
		pr_info("default doze backlight threshold is %d\n", DOZE_MIN_BRIGHTNESS_LEVEL);
	} else {
		pr_info("doze backlight threshold %d \n", mi_cfg->doze_backlight_threshold);
	}

	rc = utils->read_u32(of_node,
			"mi,disp-fod-off-dimming-delay", &mi_cfg->fod_off_dimming_delay);
	if (rc) {
		mi_cfg->fod_off_dimming_delay = DEFAULT_FOD_OFF_DIMMING_DELAY;
		pr_info("default fod_off_dimming_delay %d\n", DEFAULT_FOD_OFF_DIMMING_DELAY);
	} else {
		pr_info("fod_off_dimming_delay %d\n", mi_cfg->fod_off_dimming_delay);
	}

	mi_cfg->is_tddi_flag = utils->read_bool(of_node,
			"mi,is-tddi-flag");
	if (mi_cfg->is_tddi_flag)
		pr_info("panel is tddi\n");

	rc = utils->read_u32_array(of_node,
		"mi,mdss-dsi-panel-xy-coordinate",
		xy_coordinate, XY_COORDINATE_NUM);

	if (rc) {
		pr_info("%s:%d, Unable to read panel xy coordinate\n",
		       __func__, __LINE__);
		mi_cfg->xy_coordinate_cmds.is_read = false;
	} else {
		mi_cfg->xy_coordinate_cmds.cmds_rlen = xy_coordinate[0];
		mi_cfg->xy_coordinate_cmds.valid_bits = xy_coordinate[1];
		mi_cfg->xy_coordinate_cmds.is_read = true;
	}
	pr_info("0x%x 0x%x enabled:%d\n",
		xy_coordinate[0], xy_coordinate[1], mi_cfg->xy_coordinate_cmds.is_read);

	rc = of_property_read_u32(of_node,
			"mi,mdss-dsi-panel-dc-threshold", &mi_cfg->dc_threshold);
	if (rc) {
		mi_cfg->dc_threshold = 440;
		pr_info("default dc backlight threshold is %d\n", mi_cfg->dc_threshold);
	} else {
		pr_info("dc backlight threshold %d \n", mi_cfg->dc_threshold);
	}

	rc = of_property_read_u32(of_node,
			"mi,mdss-dsi-panel-dc-type", &mi_cfg->dc_type);
	if (rc) {
		mi_cfg->dc_type = 1;
		pr_info("default dc backlight type is %d\n", mi_cfg->dc_type);
	} else {
		pr_info("dc backlight type %d \n", mi_cfg->dc_type);
	}

	mi_cfg->dc_on_cmd_hs_enable = utils->read_bool(of_node,
		"mi,mdss-dsi-panel-dc-on-hs-enable");
	if (mi_cfg->dc_on_cmd_hs_enable) {
		pr_info("send dc on command in hs mode.\n");
	} else {
		pr_info("send dc on command in lp mode.\n");
	}

	rc = utils->read_u32_array(of_node,
			"mi,mdss-dsi-panel-max-luminance",
			max_luminance, MAX_LUMINANCE_NUM);

	if (rc) {
		pr_info("%s:%d, Unable to read panel max luminance\n",
			   __func__, __LINE__);
		mi_cfg->max_luminance_cmds.is_read = false;
	} else {
		mi_cfg->max_luminance_cmds.cmds_rlen = max_luminance[0];
		mi_cfg->max_luminance_cmds.valid_bits = max_luminance[1];
		mi_cfg->max_luminance_cmds.is_read = true;
	}
	pr_info("0x%x 0x%x enabled:%d\n",
		max_luminance[0], max_luminance[1], mi_cfg->max_luminance_cmds.is_read);

	mi_cfg->fod_hbm_enabled = false;
	mi_cfg->skip_dimmingon = STATE_NONE;
	mi_cfg->fod_backlight_flag = false;
	mi_cfg->fod_flag = false;
	mi_cfg->in_aod = false;
	mi_cfg->fod_hbm_off_time = ktime_get();
	mi_cfg->fod_backlight_off_time = ktime_get();
	mi_cfg->panel_dead_flag = false;
	mi_cfg->tddi_doubleclick_flag = false;
	mi_cfg->dc_enable = false;
	mi_cfg->doze_brightness_state = DOZE_TO_NORMAL;
	mi_cfg->unset_doze_brightness = DOZE_TO_NORMAL;

	INIT_DELAYED_WORK(&mi_cfg->cmds_work, panelon_dimming_enable_delayed_work);
	return rc;
}

int dsi_panel_write_cmd_set(struct dsi_panel *panel,
				struct dsi_panel_cmd_set *cmd_sets)
{
	int rc = 0, i = 0;
	ssize_t len;
	struct dsi_cmd_desc *cmds;
	u32 count;
	enum dsi_cmd_set_state state;
	struct dsi_display_mode *mode;
	const struct mipi_dsi_host_ops *ops = panel->host->ops;

	if (!panel || !panel->cur_mode)
		return -EINVAL;

	mode = panel->cur_mode;

	cmds = cmd_sets->cmds;
	count = cmd_sets->count;
	state = cmd_sets->state;

	if (count == 0) {
		pr_debug("[%s] No commands to be sent for state\n", panel->name);
		goto error;
	}

	for (i = 0; i < count; i++) {
		if (state == DSI_CMD_SET_STATE_LP)
			cmds->msg.flags |= MIPI_DSI_MSG_USE_LPM;

		if (cmds->last_command)
			cmds->msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;

		len = ops->transfer(panel->host, &cmds->msg);
		if (len < 0) {
			rc = len;
			pr_err("failed to set cmds, rc=%d\n", rc);
			goto error;
		}
		if (cmds->post_wait_ms)
			usleep_range(cmds->post_wait_ms*1000,
					((cmds->post_wait_ms*1000)+10));
		cmds++;
	}
error:
	return rc;
}

int dsi_panel_read_cmd_set(struct dsi_panel *panel,
				struct dsi_read_config *read_config)
{
	struct mipi_dsi_host *host;
	struct dsi_display *display;
	struct dsi_display_ctrl *ctrl;
	struct dsi_cmd_desc *cmds;
	enum dsi_cmd_set_state state;
	int i, rc = 0, count = 0;
	u32 flags = 0;

	if (panel == NULL || read_config == NULL)
		return -EINVAL;

	host = panel->host;
	if (host) {
		display = to_dsi_display(host);
		if (display == NULL)
			return -EINVAL;
	} else
		return -EINVAL;

	if (!panel->panel_initialized) {
		pr_info("Panel not initialized\n");
		return -EINVAL;
	}

	if (!read_config->is_read) {
		pr_info("read operation was not permitted\n");
		return -EPERM;
	}

	dsi_display_clk_ctrl(display->dsi_clk_handle,
		DSI_ALL_CLKS, DSI_CLK_ON);

	ctrl = &display->ctrl[display->cmd_master_idx];

	rc = dsi_display_cmd_engine_enable(display);
	if (rc) {
		pr_err("cmd engine enable failed\n");
		rc = -EPERM;
		goto exit_ctrl;
	}

	if (display->tx_cmd_buf == NULL) {
		rc = dsi_host_alloc_cmd_tx_buffer(display);
		if (rc) {
			pr_err("failed to allocate cmd tx buffer memory\n");
			goto exit;
		}
	}

	count = read_config->read_cmd.count;
	cmds = read_config->read_cmd.cmds;
	state = read_config->read_cmd.state;
	if (count == 0) {
		pr_err("No commands to be sent\n");
		goto exit;
	}
	if (cmds->last_command) {
		cmds->msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;
		flags |= DSI_CTRL_CMD_LAST_COMMAND;
	}
	if (state == DSI_CMD_SET_STATE_LP)
		cmds->msg.flags |= MIPI_DSI_MSG_USE_LPM;
	flags |= (DSI_CTRL_CMD_FETCH_MEMORY | DSI_CTRL_CMD_READ |
		  DSI_CTRL_CMD_CUSTOM_DMA_SCHED);

	memset(read_config->rbuf, 0x0, sizeof(read_config->rbuf));
	cmds->msg.rx_buf = read_config->rbuf;
	cmds->msg.rx_len = read_config->cmds_rlen;

	rc = dsi_ctrl_cmd_transfer(ctrl->ctrl, &(cmds->msg), &flags);
	if (rc <= 0) {
		pr_err("rx cmd transfer failed rc=%d\n", rc);
		goto exit;
	}

	for (i = 0; i < read_config->cmds_rlen; i++) //debug
		pr_info("0x%x ", read_config->rbuf[i]);
	pr_info("\n");

exit:
	dsi_display_cmd_engine_disable(display);
exit_ctrl:
	dsi_display_clk_ctrl(display->dsi_clk_handle,
		DSI_ALL_CLKS, DSI_CLK_OFF);

	return rc;
}

ssize_t dsi_panel_mipi_reg_write(struct dsi_panel *panel,
				char *buf, size_t count)
{
	struct dsi_panel_cmd_set cmd_sets = {0};
	int retval = 0, dlen = 0;
	u32 packet_count = 0;
	char *token, *input_copy, *input_dup = NULL;
	const char *delim = " ";
	char *buffer = NULL;
	u32 buf_size = 0;
	u32 tmp_data = 0;

	mutex_lock(&panel->panel_lock);

	if (!panel || !panel->panel_initialized) {
		pr_err("[LCD] panel not ready!\n");
		retval = -EAGAIN;
		goto exit_unlock;
	}

	pr_debug("input buffer:{%s}\n", buf);

	input_copy = kstrdup(buf, GFP_KERNEL);
	if (!input_copy) {
		retval = -ENOMEM;
		goto exit_unlock;
	}

	input_dup = input_copy;
	/* removes leading and trailing whitespace from input_copy */
	input_copy = strim(input_copy);

	/* Split a string into token */
	token = strsep(&input_copy, delim);
	if (token) {
		retval = kstrtoint(token, 10, &tmp_data);
		if (retval) {
			pr_err("input buffer conversion failed\n");
			goto exit_free0;
		}
		g_dsi_read_cfg.is_read= !!tmp_data;
	}

	/* Removes leading whitespace from input_copy */
	if (input_copy)
		input_copy = skip_spaces(input_copy);
	else
		goto exit_free0;

	token = strsep(&input_copy, delim);
	if (token) {
		retval = kstrtoint(token, 10, &tmp_data);
		if (retval) {
			pr_err("input buffer conversion failed\n");
			goto exit_free0;
		}
		if (tmp_data > sizeof(g_dsi_read_cfg.rbuf)) {
			pr_err("read size exceeding the limit %d\n",
					sizeof(g_dsi_read_cfg.rbuf));
			goto exit_free0;
		}
		g_dsi_read_cfg.cmds_rlen = tmp_data;
	}

	/* Removes leading whitespace from input_copy */
	if (input_copy)
		input_copy = skip_spaces(input_copy);
	else
		goto exit_free0;

	buffer = kzalloc(strlen(input_copy), GFP_KERNEL);
	if (!buffer) {
		retval = -ENOMEM;
		goto exit_free0;
	}

	token = strsep(&input_copy, delim);
	while (token) {
		retval = kstrtoint(token, 16, &tmp_data);
		if (retval) {
			pr_err("input buffer conversion failed\n");
			goto exit_free1;
		}
		pr_debug("[lzl-test]buffer[%d] = 0x%02x\n", buf_size, tmp_data);
		buffer[buf_size++] = (tmp_data & 0xff);
		/* Removes leading whitespace from input_copy */
		if (input_copy) {
			input_copy = skip_spaces(input_copy);
			token = strsep(&input_copy, delim);
		} else {
			token = NULL;
		}
	}

	retval = dsi_panel_get_cmd_pkt_count(buffer, buf_size, &packet_count);
	if (!packet_count) {
		pr_err("get pkt count failed!\n");
		goto exit_free1;
	}

	retval = dsi_panel_alloc_cmd_packets(&cmd_sets, packet_count);
	if (retval) {
		pr_err("failed to allocate cmd packets, ret=%d\n", retval);
		goto exit_free1;
	}

	retval = dsi_panel_create_cmd_packets(buffer, dlen, packet_count,
						  cmd_sets.cmds);
	if (retval) {
		pr_err("failed to create cmd packets, ret=%d\n", retval);
		goto exit_free2;
	}

	if (g_dsi_read_cfg.is_read) {
		g_dsi_read_cfg.read_cmd = cmd_sets;
		retval = dsi_panel_read_cmd_set(panel, &g_dsi_read_cfg);
		if (retval <= 0) {
			pr_err("[%s]failed to read cmds, rc=%d\n", panel->name, retval);
			goto exit_free3;
		}
	} else {
		g_dsi_read_cfg.read_cmd = cmd_sets;
		if (panel->mi_cfg.dc_on_cmd_hs_enable)
			cmd_sets.state = DSI_CMD_SET_STATE_HS;
		retval = dsi_panel_write_cmd_set(panel, &cmd_sets);
		if (retval) {
			pr_err("[%s] failed to send cmds, rc=%d\n", panel->name, retval);
			goto exit_free3;
		}
	}

	pr_debug("[%s]: done!\n", panel->name);
	retval = count;

exit_free3:
	dsi_panel_destroy_cmd_packets(&cmd_sets);
exit_free2:
	dsi_panel_dealloc_cmd_packets(&cmd_sets);
exit_free1:
	kfree(buffer);
exit_free0:
	kfree(input_dup);
exit_unlock:
	mutex_unlock(&panel->panel_lock);
	return retval;
}

ssize_t dsi_panel_mipi_reg_read(struct dsi_panel *panel, char *buf)
{
	int i = 0;
	ssize_t count = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	mutex_lock(&panel->panel_lock);

	if (g_dsi_read_cfg.is_read) {
		for (i = 0; i < g_dsi_read_cfg.cmds_rlen; i++) {
			if (i == g_dsi_read_cfg.cmds_rlen - 1) {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02X\n",
				     g_dsi_read_cfg.rbuf[i]);
			} else {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02X,",
				     g_dsi_read_cfg.rbuf[i]);
			}
		}
	}

	mutex_unlock(&panel->panel_lock);

	return count;
}

ssize_t dsi_panel_lockdown_info_read(unsigned char *plockdowninfo)
{
	int rc = 0;
	int i = 0;
	struct dsi_read_config ld_read_config;
	struct dsi_panel_cmd_set cmd_sets = {0};

	if (!g_panel || !plockdowninfo) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	while(!g_panel->cur_mode || !g_panel->cur_mode->priv_info || !g_panel->panel_initialized) {
		pr_debug("[%s][%s] waitting for panel priv_info initialized!\n", __func__, g_panel->name);
		msleep_interruptible(1000);
	}

	mutex_lock(&g_panel->panel_lock);
	if (g_panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_MI_READ_LOCKDOWN_INFO].cmds) {
		cmd_sets.cmds = g_panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_MI_READ_LOCKDOWN_INFO].cmds;
		cmd_sets.count = 1;
		cmd_sets.state = g_panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_MI_READ_LOCKDOWN_INFO].state;
		rc = dsi_panel_write_cmd_set(g_panel, &cmd_sets);
		if (rc) {
			pr_err("[%s][%s] failed to send cmds, rc=%d\n", __func__, g_panel->name, rc);
			rc = -EIO;
			goto done;
		}

		ld_read_config.is_read = 1;
		ld_read_config.cmds_rlen = 8;
		ld_read_config.read_cmd = cmd_sets;
		ld_read_config.read_cmd.cmds = &cmd_sets.cmds[1];
		rc = dsi_panel_read_cmd_set(g_panel, &ld_read_config);
		if (rc <= 0) {
			pr_err("[%s][%s] failed to read cmds, rc=%d\n", __func__, g_panel->name, rc);
			rc = -EIO;
			goto done;
		}

		for(i = 0; i < 8; i++) {
			pr_info("[%s][%d]0x%x", __func__, __LINE__, ld_read_config.rbuf[i]);
			plockdowninfo[i] = ld_read_config.rbuf[i];
		}

		if (!strcmp(g_panel->name,"xiaomi 37 02 0b video mode dsc dsi panel")) {
			plockdowninfo[7] = 0x01;
			pr_info("[%s] plockdowninfo[7] = 0x%d \n", __func__, plockdowninfo[7]);
		}
	}

done:
	mutex_unlock(&g_panel->panel_lock);
	return rc;
}
EXPORT_SYMBOL(dsi_panel_lockdown_info_read);

static void handle_dsi_read_data(struct dsi_panel_mi_cfg *mi_panel, struct dsi_read_config *read_config)
{
	int i = 0;
	int param_nb = 0, write_len = 0;
	u32 read_cnt = 0, bit_valide = 0;
	u8 *pRead_data = NULL;

	if (!mi_panel || !read_config) {
		pr_err("invalid params\n");
		return;
	}

	pRead_data = mi_panel->panel_read_data;
	read_cnt = read_config->cmds_rlen;
	bit_valide = read_config->valid_bits;

	for (i = 0; i < read_cnt; i++) {
		if ((bit_valide & 0x1) && ((pRead_data + 8) < (mi_panel->panel_read_data + BUF_LEN_MAX))) {
			write_len = scnprintf(pRead_data, 8, "p%d=%d", param_nb,  read_config->rbuf[i]);
			pRead_data += write_len;
			param_nb ++;
		}
		bit_valide = bit_valide >> 1;
	}
	pr_info("read %s from panel\n", mi_panel->panel_read_data);

	return;
}

static ssize_t dsi_panel_xy_coordinate_read(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_panel_mi_cfg *mi_cfg  = NULL;
	struct dsi_panel_cmd_set cmd_sets = {0};

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;
	if (!mi_cfg) {
		pr_err("mi panel is validate\n");
		return -EINVAL;
	}

	while(!panel->cur_mode || !panel->cur_mode->priv_info || !panel->panel_initialized) {
		pr_debug("[%s][%s] waitting for panel priv_info initialized!\n", __func__, panel->name);
		msleep_interruptible(1000);
	}

	if (panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_READ_XY_COORDINATE].cmds) {
		cmd_sets.cmds = panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_READ_XY_COORDINATE].cmds;
		cmd_sets.count = 1;
		cmd_sets.state = panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_READ_XY_COORDINATE].state;
		rc = dsi_panel_write_cmd_set(panel, &cmd_sets);
		if (rc) {
			pr_err("[%s][%s] failed to send cmds, rc=%d\n", __func__, panel->name, rc);
			rc = -EIO;
			return rc;
		}

		mi_cfg->xy_coordinate_cmds.read_cmd = cmd_sets;
		mi_cfg->xy_coordinate_cmds.read_cmd.cmds = &cmd_sets.cmds[1];
		rc = dsi_panel_read_cmd_set(panel, &mi_cfg->xy_coordinate_cmds);
		if (rc > 0)
			handle_dsi_read_data(mi_cfg, &mi_cfg->xy_coordinate_cmds);
		else
			pr_err("[%s][%s] failed to read cmds, rc=%d\n", __func__, panel->name, rc);
	}

	return rc;
}

static ssize_t dsi_panel_max_luminance_read(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_panel_mi_cfg *mi_cfg  = NULL;
	struct dsi_panel_cmd_set cmd_sets = {0};
	struct dsi_read_config r_cfg = {0};

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;
	if (!mi_cfg) {
		pr_err("mi panel is validate\n");
		return -EINVAL;
	}

	while(!panel->cur_mode || !panel->cur_mode->priv_info || !panel->panel_initialized) {
		pr_debug("[%s][%s] waitting for panel priv_info initialized!\n", __func__, panel->name);
		msleep_interruptible(1000);
	}

	if (panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_MI_MAX_LUMINANCE_WRITE].cmds) {
		cmd_sets.cmds = panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_MI_MAX_LUMINANCE_WRITE].cmds;
		cmd_sets.count = 1;
		cmd_sets.state = panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_MI_MAX_LUMINANCE_WRITE].state;
		rc = dsi_panel_write_cmd_set(panel, &cmd_sets);
		if (rc) {
			pr_err("[%s][%s] failed to send cmds, rc=%d\n", __func__, panel->name, rc);
			rc = -EIO;
			return rc;
		}
	}

	if (panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_MI_MAX_LUMINANCE_READ].cmds) {
		r_cfg = mi_cfg->max_luminance_cmds;
		r_cfg.read_cmd.cmds = panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_MI_MAX_LUMINANCE_READ].cmds;
		r_cfg.read_cmd.count = 1;
		r_cfg.read_cmd.state = panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_MI_MAX_LUMINANCE_READ].state;

		rc = dsi_panel_read_cmd_set(panel, &r_cfg);
		if (rc > 0)
			handle_dsi_read_data(mi_cfg, &r_cfg);
		else
			pr_err("[%s][%s] failed to read cmds, rc=%d\n", __func__, panel->name, rc);
	}

	return rc;
}

void dsi_panel_doubleclick_enable(bool on)
{
	g_panel->mi_cfg.tddi_doubleclick_flag = on;
}
EXPORT_SYMBOL(dsi_panel_doubleclick_enable);

#if DSI_READ_WRITE_PANEL_DEBUG
static ssize_t mipi_reg_procfs_write(struct file *filp,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	int retval = 0;
	char *input = NULL;
	struct dsi_panel *panel = PDE_DATA(file_inode(filp));

	input = kmalloc(count, GFP_KERNEL);
	if (!input) {
		return -ENOMEM;
	}
	if (copy_from_user(input, buf, count)) {
		pr_err("copy from user failed\n");
		retval = -EFAULT;
		goto exit;
	}
	input[count-1] = '\0';
	pr_debug("copy_from_user input: %s\n", input);

	retval = dsi_panel_mipi_reg_write(panel, input, count);

exit:
	kfree(input);
	return retval ? retval : count;
}

static int mipi_reg_procfs_show(struct seq_file *m, void *v)
{
	struct dsi_panel *panel = (struct dsi_panel *)m->private;
	int i = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	mutex_lock(&panel->panel_lock);

	if (g_dsi_read_cfg.is_read) {
		seq_printf(m, "return value: ");
		for (i = 0; i < g_dsi_read_cfg.cmds_rlen; i++) {
			printk("0x%02X ", g_dsi_read_cfg.rbuf[i]);
			seq_printf(m, "0x%02X ", g_dsi_read_cfg.rbuf[i]);
		}
	}
	seq_printf(m,"\n");
	mutex_unlock(&panel->panel_lock);

	return 0;
}

static int mipi_reg_procfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, mipi_reg_procfs_show, PDE_DATA(inode));
}

const struct file_operations mipi_reg_proc_fops = {
	.owner   = THIS_MODULE,
	.open    = mipi_reg_procfs_open,
	.write   = mipi_reg_procfs_write,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

int dsi_panel_procfs_init(struct dsi_panel *panel)
{
	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	mipi_proc_entry = proc_create_data(MIPI_PROC_NAME, S_IRUGO | S_IWUSR,
			NULL, &mipi_reg_proc_fops, panel);
	if (!mipi_proc_entry) {
		printk(KERN_WARNING "mipi_reg: unable to create proc entry.\n");
		return -ENODEV;
	}
	return 0;
}

int dsi_panel_procfs_deinit(struct dsi_panel *panel)
{
	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (mipi_proc_entry) {
		remove_proc_entry(MIPI_PROC_NAME, NULL);
		mipi_proc_entry = NULL;
	}
	return 0;
}
#endif

int dsi_panel_disp_param_set(struct dsi_panel *panel, u32 param)
{
	int rc = 0;
	uint32_t temp = 0;
	u32 fod_backlight = 0;
	struct dsi_panel_mi_cfg *mi_cfg  = NULL;
	struct dsi_cmd_desc *cmds = NULL;
	struct dsi_display_mode_priv_info *priv_info;
	static u8 backlight_delta = 0;
	u32 resend_backlight;
	u32 count;
	u8 *tx_buf;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	mi_cfg = &panel->mi_cfg;
	if (!mi_cfg->mi_feature_enabled) {
		pr_err("mi feature not enable, exit!\n");
		goto exit;
	}

	DSI_INFO("param_type = 0x%x \n", param);

	if (!panel->panel_initialized
		&& (param & 0x0F000000) != DISPPARAM_FOD_BACKLIGHT_ON
		&& (param & 0x0F000000) != DISPPARAM_FOD_BACKLIGHT_OFF) {
		pr_err("panel not ready!\n");
		goto exit;
	}

	priv_info = panel->cur_mode->priv_info;

	if ((param & 0x00F00000) == 0xD00000) {
		fod_backlight = (param & 0x01FFF);
		param = (param & 0x0FF00000);
	}

	/* set smart fps status */
	if (param & 0xF0000000) {
		fm_stat.enabled = param & 0x01;
		DSI_INFO("smart dfps enable = [%d]\n", fm_stat.enabled);
	}

	temp = param & 0x0000000F;
	switch (temp) {
	case DISPPARAM_WARM:
	case DISPPARAM_DEFAULT:
	case DISPPARAM_COLD:
	case DISPPARAM_PAPERMODE8:
	case DISPPARAM_PAPERMODE1:
	case DISPPARAM_PAPERMODE2:
	case DISPPARAM_PAPERMODE3:
	case DISPPARAM_PAPERMODE4:
	case DISPPARAM_PAPERMODE5:
	case DISPPARAM_PAPERMODE6:
	case DISPPARAM_PAPERMODE7:
		DSI_INFO("DISPPARAM_WARM[0x1]~DISPPARAM_PAPERMODE7[0xc] not supported!\n");
		break;
	case DISPPARAM_WHITEPOINT_XY:
		DSI_INFO("read xy coordinate\n");
		dsi_panel_xy_coordinate_read(panel);
		break;
	case DISPPARAM_MAX_LUMINANCE_READ:
		DSI_INFO("read max luminance nit value");
		dsi_panel_max_luminance_read(panel);
		break;
	default:
		break;
	}

	temp = param & 0x000000F0;
	switch (temp) {
	case DISPPARAM_CE_ON:
		DSI_INFO("ceon\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CEON);
		break;
	case DISPPARAM_CE_OFF:
		DSI_INFO("ceoff\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CEOFF);
		break;
	default:
		break;
	}

	temp = param & 0x00000F00;
	switch (temp) {
	case DISPPARAM_CABCUI_ON:
		DSI_INFO("cabcuion\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CABCUION);
		break;
	case DISPPARAM_CABCSTILL_ON:
		DSI_INFO("cabcstillon\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CABCSTILLON);
		break;
	case DISPPARAM_CABCMOVIE_ON:
		DSI_INFO("cabcmovieon\n");
		dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CABCMOVIEON);
		break;
	case DISPPARAM_CABC_OFF:
		DSI_INFO("cabcoff\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CABCOFF);
		break;
	case DISPPARAM_SKIN_CE_CABCUI_ON:
		DSI_INFO("skince cabcuion\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_SKINCE_CABCUION);
		break;
	case DISPPARAM_SKIN_CE_CABCSTILL_ON:
		DSI_INFO("skince cabcstillon\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_SKINCE_CABCSTILLON);
		break;
	case DISPPARAM_SKIN_CE_CABCMOVIE_ON:
		DSI_INFO("skince cabcmovieon\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_SKINCE_CABCMOVIEON);
		break;
	case DISPPARAM_SKIN_CE_CABC_OFF:
		DSI_INFO("skince cabcoff\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_SKINCE_CABCOFF);
		break;
	case DISPPARAM_CABC_DIMMING:
		DSI_INFO("panel cabc dimming\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CABC_DIMMING);
		break;
	case DISPPARAM_DIMMING_OFF:
		if (mi_cfg->dimming_mode_state == MI_FEATURE_STATE_ON) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGOFF);
			mi_cfg->dimming_mode_state = MI_FEATURE_STATE_OFF;
			DSI_INFO("dimmingoff\n");
		}
		break;
	case DISPPARAM_DIMMING:
		if (mi_cfg->dimming_mode_state == MI_FEATURE_STATE_OFF)
		{
			if (mi_cfg->skip_dimmingon != STATE_DIM_BLOCK) {
				if (ktime_after(ktime_get(), mi_cfg->fod_hbm_off_time)
					&& ktime_after(ktime_get(), mi_cfg->fod_backlight_off_time)) {
					dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGON);
					mi_cfg->dimming_mode_state = MI_FEATURE_STATE_ON;
					DSI_INFO("dimmingon\n");
				} else {
					DSI_INFO("skip dimmingon due to hbm off\n");
				}
			} else {
				DSI_INFO("skip dimmingon due to hbm on\n");
			}
		}
		break;
	default:
		break;
	}

	temp = param & 0x0000F000;
	switch (temp) {
	case DISPPARAM_ACL_L1:
		DSI_INFO("acl level 1\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_ACL_L1);
		break;
	case DISPPARAM_ACL_L2:
		DSI_INFO("acl level 2\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_ACL_L2);
		break;
	case DISPPARAM_ACL_L3:
		DSI_INFO("acl level 3\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_ACL_L3);
		break;
	case DISPPARAM_ACL_OFF:
		DSI_INFO("acl off\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_ACL_OFF);
		break;
	default:
		break;
	}

	temp = param & 0x000F0000;
	switch (temp) {
	case DISPPARAM_LCD_HBM_L1_ON:
		DSI_INFO("lcd hbm l1 on\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_LCD_HBM_L1_ON);
		break;
	case DISPPARAM_LCD_HBM_L2_ON:
		DSI_INFO("lcd hbm  l2 on\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_LCD_HBM_L2_ON);
		break;
	case DISPPARAM_LCD_HBM_OFF:
		DSI_INFO("lcd hbm off\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_LCD_HBM_OFF);
		break;
	case DISPPARAM_HBM_ON:
		DSI_INFO("hbm on\n");
		mi_cfg->hbm_enabled = true;
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_ON);
		mi_cfg->skip_dimmingon = STATE_DIM_BLOCK;
		break;
	case DISPPARAM_HBM_FOD_ON:
		mi_cfg->fod_hbm_enabled = true;
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_FOD_ON);
		mi_cfg->skip_dimmingon = STATE_DIM_BLOCK;
		DSI_INFO("hbm fod on\n");
		break;
	case DISPPARAM_HBM_FOD2NORM:
		DSI_INFO("hbm fod to normal mode\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_FOD2NORM);
		break;
	case DISPPARAM_HBM_FOD_OFF:
		if (mi_cfg->hbm_51_ctrl_flag) {
			cmds = priv_info->cmd_sets[DSI_CMD_SET_MI_HBM_FOD_OFF].cmds;
			count = priv_info->cmd_sets[DSI_CMD_SET_MI_HBM_FOD_OFF].count;
			if (cmds && count >= mi_cfg->fod_off_51_index) {
				tx_buf = (u8 *)cmds[mi_cfg->fod_off_51_index].msg.tx_buf;
				if (mi_cfg->in_aod) {
					/* aod brightness restore when hbm off */
					if (mi_cfg->doze_brightness_state == DOZE_BRIGHTNESS_HBM || mi_cfg->unset_doze_brightness== DOZE_BRIGHTNESS_HBM) {
						tx_buf[1] = (mi_cfg->doze_hbm_brightness >> 8) & 0x07;
						tx_buf[2] = mi_cfg->doze_hbm_brightness & 0xff;
					} else if (mi_cfg->doze_brightness_state == DOZE_BRIGHTNESS_LBM) {
						tx_buf[1] = (mi_cfg->doze_lbm_brightness >> 8) & 0x07;
						tx_buf[2] = mi_cfg->doze_lbm_brightness & 0xff;
					}
				} else {
					tx_buf[1] = (mi_cfg->bl_last_level_unsetted >> 8) & 0x07;
					tx_buf[2] = mi_cfg->bl_last_level_unsetted & 0xff;
				}
			}
		}
		/* HBM on， fod_hbm_off, only backlight recovery */
		if (mi_cfg->hbm_enabled) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_ON);
			mi_cfg->skip_dimmingon = STATE_DIM_BLOCK;
		} else {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_FOD_OFF);
			mi_cfg->skip_dimmingon = STATE_DIM_RESTORE;
		}
		mi_cfg->fod_hbm_enabled = false;
		DSI_INFO("hbm fod off， fod：%x, hbm:%x, doze_bl_state:%x\n",
				mi_cfg->fod_hbm_enabled , mi_cfg->hbm_enabled,
				mi_cfg->doze_brightness_state);
		break;
	case DISPPARAM_HBM_OFF:
		if (!mi_cfg->fod_hbm_enabled) {
			if (mi_cfg->hbm_51_ctrl_flag) {
				cmds = priv_info->cmd_sets[DSI_CMD_SET_MI_HBM_OFF].cmds;
				count = priv_info->cmd_sets[DSI_CMD_SET_MI_HBM_OFF].count;
				if (cmds && count >= mi_cfg->hbm_off_51_index) {
					tx_buf = (u8 *)cmds[mi_cfg->hbm_off_51_index].msg.tx_buf;
					tx_buf[1] = (mi_cfg->bl_last_level_unsetted >> 8) & 0x07;
					tx_buf[2] = mi_cfg->bl_last_level_unsetted & 0xff;
				}
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_OFF);
			} else {
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_OFF);
			}
			mi_cfg->skip_dimmingon = STATE_DIM_RESTORE;
		}
		mi_cfg->hbm_enabled = false;
		break;
	case DISPPARAM_DC_ON:
		DSI_INFO("DC on\n");
		if (mi_cfg->dc_type == 0) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DC_ON);
			if (rc)
				pr_err("[%s] failed to send DSI_CMD_SET_MI_DC_ON cmd, rc=%d\n",
						panel->name, rc);
			else
				rc = dsi_panel_update_backlight(panel, mi_cfg->bl_last_level);
		}
		mi_cfg->dc_enable = true;
		break;
	case DISPPARAM_DC_OFF:
		DSI_INFO("DC off\n");
		if (mi_cfg->dc_type == 0) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DC_OFF);
			if (rc)
				pr_err("[%s] failed to send DSI_CMD_SET_MI_DC_OFF cmd, rc=%d\n",
						panel->name, rc);
			else
				rc = dsi_panel_update_backlight(panel, mi_cfg->bl_last_level);
		}
		mi_cfg->dc_enable = false;
		break;
	default:
		break;
	}

	temp = param & 0x00F00000;
	switch (temp) {
	case DISPPARAM_NORMALMODE1:
		DSI_INFO("normal mode1\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_NORMAL1);
		break;
	case DISPPARAM_P3:
		DSI_INFO("dci p3 mode\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CRC_DCIP3);
		break;
	case DISPPARAM_SRGB:
		DSI_INFO("sRGB\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_SRGB);
		break;
	case DISPPARAM_DOZE_BRIGHTNESS_HBM:
#ifdef CONFIG_FACTORY_BUILD
		DSI_INFO("doze hbm On\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM);
		mi_cfg->skip_dimmingon = STATE_DIM_BLOCK;
#else
		if (mi_cfg->in_aod) {
			DSI_INFO("doze hbm On\n");
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM);
			mi_cfg->skip_dimmingon = STATE_DIM_BLOCK;
		}
#endif
		break;
	case DISPPARAM_DOZE_BRIGHTNESS_LBM:
#ifdef CONFIG_FACTORY_BUILD
		DSI_INFO("doze lbm On\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM);
		mi_cfg->skip_dimmingon = STATE_DIM_BLOCK;
#else
		if (mi_cfg->in_aod) {
			DSI_INFO("doze lbm On\n");
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM);
			mi_cfg->skip_dimmingon = STATE_DIM_BLOCK;
		}
#endif
		break;
	case DISPPARAM_DOZE_OFF:
		DSI_INFO("doze Off\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_NOLP);
		break;
	case DISPPARAM_HBM_BACKLIGHT_RESEND:
		backlight_delta++;
		if (mi_cfg->bl_last_level >= panel->bl_config.bl_max_level - 1)
			resend_backlight = mi_cfg->bl_last_level -
				((backlight_delta%2 == 0) ? 1 : 2);
		else
			resend_backlight = mi_cfg->bl_last_level +
				((backlight_delta%2 == 0) ? 1 : 2);

		DSI_INFO("backlight resend: bl_last_level = %d; resend_backlight = %d\n",
				mi_cfg->bl_last_level, resend_backlight);
		rc = dsi_panel_update_backlight(panel, resend_backlight);
		break;
	case DISPPARAM_FOD_BACKLIGHT:
		if (fod_backlight == 0x7FF)
			fod_backlight = panel->bl_config.bl_max_level;

		if (fod_backlight == 0x1000) {
			DSI_INFO("FOD backlight restore bl_last_level=%d\n",
				mi_cfg->bl_last_level);
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGOFF);
			if (mi_cfg->dc_enable && mi_cfg->dc_type) {
				DSI_INFO("FOD backlight restore dc_threshold=%d",
				mi_cfg->dc_threshold);
				rc = dsi_panel_update_backlight(panel, mi_cfg->dc_threshold);
			} else {
				DSI_INFO("FOD backlight restore bl_last_level=%d",
				mi_cfg->bl_last_level);
				rc = dsi_panel_update_backlight(panel, mi_cfg->bl_last_level);
			}
		} else if (fod_backlight >= 0) {
			DSI_INFO("FOD backlight set");
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGOFF);
			rc = dsi_panel_update_backlight(panel, fod_backlight);
			mi_cfg->fod_target_backlight = fod_backlight;
			mi_cfg->skip_dimmingon = STATE_NONE;
		}
		break;
	case DISPPARAM_CRC_OFF:
		DSI_INFO("crc off\n");
		mi_cfg->crc_off_pending = false;
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CRC_OFF);
		break;
	default:
		break;
	}

	temp = param & 0x0F000000;
	switch (temp) {
	case DISPPARAM_FOD_BACKLIGHT_ON:
		DSI_INFO("fod_backlight_flag on\n");
		mi_cfg->fod_backlight_flag = true;
		break;
	case DISPPARAM_FOD_BACKLIGHT_OFF:
		DSI_INFO("fod_backlight_flag off\n");
		mi_cfg->fod_backlight_flag = false;
		break;
	case DISPPARAM_ELVSS_DIMMING_ON:
		DSI_INFO("elvss dimming on\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_ELVSS_DIMMING_OFF);
		break;
	case DISPPARAM_ELVSS_DIMMING_OFF:
		DSI_INFO("elvss dimming off\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_ELVSS_DIMMING_OFF);
		break;
	case DISPPARAM_FLAT_MODE_ON:
		if(mi_cfg->flat_mode_state == MI_FEATURE_STATE_OFF) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_ON);
			mi_cfg->flat_mode_state = MI_FEATURE_STATE_ON;
			DSI_INFO("flat mode on\n");
		}
		break;
	case DISPPARAM_FLAT_MODE_OFF:
		if(mi_cfg->flat_mode_state == MI_FEATURE_STATE_ON) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_OFF);
			mi_cfg->flat_mode_state = MI_FEATURE_STATE_OFF;
			DSI_INFO("flat mode off\n");
		}
		break;
	case DISPPARAM_IDLE_ON:
		DSI_INFO("idle on\n");
		g_idleflag = true;
		break;
	case DISPPARAM_IDLE_OFF:
		DSI_INFO("idle off\n");
		g_idleflag = false;
		break;
	default:
		break;
	}

	temp = param & 0xF0000000;
	switch (temp) {
	case DISPPARAM_DFPS_LEVEL1:
		DSI_INFO("DFPS:30fps\n");
		panel->mi_cfg.panel_max_frame_rate = false;
		break;
	case DISPPARAM_DFPS_LEVEL2:
		DSI_INFO("DFPS:50fps\n");
		panel->mi_cfg.panel_max_frame_rate = false;
		break;
	case DISPPARAM_DFPS_LEVEL3:
		DSI_INFO("DFPS:60fps\n");
		panel->mi_cfg.panel_max_frame_rate = false;
		break;
	case DISPPARAM_DFPS_LEVEL4:
		DSI_INFO("DFPS:90fps\n");
		panel->mi_cfg.panel_max_frame_rate = false;
		break;
	case DISPPARAM_DFPS_LEVEL5:
		DSI_INFO("DFPS:120fps\n");
		panel->mi_cfg.panel_max_frame_rate = true;
		break;
	default:
		break;
	}

exit:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

static void panelon_dimming_enable_delayed_work(struct work_struct *work)
{
	struct dsi_panel *panel = NULL;
	struct dsi_panel_mi_cfg *mi_cfg = container_of(work, struct dsi_panel_mi_cfg, cmds_work.work);

	if (mi_cfg)
		panel = container_of(mi_cfg, struct dsi_panel, mi_cfg);

	if (mi_cfg->dimming_on_enable)
		dsi_panel_disp_param_set(panel, DISPPARAM_DIMMING);
	else
		dsi_panel_disp_param_set(panel, DISPPARAM_CABC_DIMMING);
}

int dsi_panel_set_doze_brightness(struct dsi_panel *panel,
			int doze_brightness, bool need_panel_lock)
{
	int rc = 0;
	struct dsi_panel_mi_cfg *mi_cfg;
	const char *doze_brightness_str[] = {
		[DOZE_TO_NORMAL] = "DOZE_TO_NORMAL",
		[DOZE_BRIGHTNESS_HBM] = "DOZE_BRIGHTNESS_HBM",
		[DOZE_BRIGHTNESS_LBM] = "DOZE_BRIGHTNESS_LBM",
	};

	if (need_panel_lock)
		mutex_lock(&panel->panel_lock);

	mi_cfg = &panel->mi_cfg;

	if (!panel->panel_initialized) {
		mi_cfg->unset_doze_brightness = doze_brightness;
		DSI_INFO("Panel not initialized! save unset_doze_brightness = %s",
				doze_brightness_str[mi_cfg->unset_doze_brightness]);
		goto exit;
	}

	if (mi_cfg->fod_hbm_enabled) {
		mi_cfg->unset_doze_brightness = doze_brightness;
		if (mi_cfg->unset_doze_brightness == DOZE_TO_NORMAL) {
			mi_cfg->doze_brightness_state = DOZE_TO_NORMAL;
			mi_cfg->skip_dimmingon = STATE_DIM_BLOCK;
		}
		DSI_INFO("fod_hbm_enabled set, save unset_doze_brightness = %s",
				doze_brightness_str[mi_cfg->unset_doze_brightness]);
		goto exit;
	}

	if (mi_cfg->in_aod) {
		if (mi_cfg->doze_brightness_state != doze_brightness ||
			mi_cfg->unset_doze_brightness != DOZE_TO_NORMAL) {
			if (mi_cfg->fod_hbm_enabled) {
				DSI_INFO(" fod hbm on, skip to enter aod mode\n");
			} else {
				if (doze_brightness == DOZE_BRIGHTNESS_HBM ||
					mi_cfg->unset_doze_brightness == DOZE_BRIGHTNESS_HBM) {
					rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM);
					mi_cfg->aod_backlight = mi_cfg->doze_hbm_brightness;
					DSI_INFO("[%s] send DSI_CMD_SET_MI_DOZE_HBM cmd.\n", panel->name);
					mi_cfg->aod_backlight = 170;
				} else if (doze_brightness == DOZE_BRIGHTNESS_LBM ||
					mi_cfg->unset_doze_brightness == DOZE_BRIGHTNESS_LBM) {
					rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM);
					mi_cfg->aod_backlight = mi_cfg->doze_hbm_brightness;
					DSI_INFO("[%s] send DSI_CMD_SET_MI_DOZE_LBM cmd.\n", panel->name);
					mi_cfg->aod_backlight = 10;
				}
			}
			mi_cfg->unset_doze_brightness = DOZE_TO_NORMAL;
			mi_cfg->skip_dimmingon = STATE_DIM_BLOCK;
			mi_cfg->doze_brightness_state = doze_brightness;
			DSI_INFO("set doze brightness to %s", doze_brightness_str[doze_brightness]);
		} else
			DSI_INFO("%s has been set, skip\n", doze_brightness_str[doze_brightness]);
	} else {
		mi_cfg->unset_doze_brightness = doze_brightness;
		if (mi_cfg->unset_doze_brightness != DOZE_TO_NORMAL)
			DSI_INFO("Not in Doze mode! save unset_doze_brightness = %s",
					doze_brightness_str[mi_cfg->unset_doze_brightness]);
	}

exit:
	if (need_panel_lock)
		mutex_unlock(&panel->panel_lock);

	return rc;
}

ssize_t dsi_panel_get_doze_brightness(struct dsi_panel *panel, char *buf)
{
	ssize_t count = 0;
	struct dsi_panel_mi_cfg *mi_cfg;

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	mutex_lock(&panel->panel_lock);

	mi_cfg = &panel->mi_cfg;
	count = snprintf(buf, PAGE_SIZE, "%d\n", mi_cfg->doze_brightness_state);

	mutex_unlock(&panel->panel_lock);

	return count;
}

/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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

#define pr_fmt(fmt)	"msm-dsi-panel:[%s:%d] " fmt, __func__, __LINE__
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include <linux/firmware.h>

#include "dsi_panel.h"
#include "dsi_display.h"
#include "dsi_ctrl_hw.h"

#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/fcntl.h>

#include <drm/drm_notifier.h>
#include <soc/qcom/socinfo.h>

#define DSI_READ_WRITE_PANEL_DEBUG 1
#if DSI_READ_WRITE_PANEL_DEBUG
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

/**
 * topology is currently defined by a set of following 3 values:
 * 1. num of layer mixers
 * 2. num of compression encoders
 * 3. num of interfaces
 */
#define TOPOLOGY_SET_LEN 3
#define MAX_TOPOLOGY 5

#define DSI_PANEL_DEFAULT_LABEL  "Default dsi panel"
#define EXT_BRIDGE_DEFAULT_LABEL  "Default ext bridge"

#define DEFAULT_MDP_TRANSFER_TIME 14000

#define DEFAULT_PANEL_JITTER_NUMERATOR		2
#define DEFAULT_PANEL_JITTER_DENOMINATOR	1
#define DEFAULT_PANEL_JITTER_ARRAY_SIZE		2
#define MAX_PANEL_JITTER		10
#define DEFAULT_PANEL_PREFILL_LINES	25

#define DISPLAY_OFF_MODE 0x60000
#define DISPLAY_ON_MODE 0x70000

#define HWMON_CONPONENT_NAME   "display"
#define HWMON_KEY_ACTIVE       "panel_active"
#define HWMON_KEY_REFRESH      "panel_refresh"
#define HWMON_KEY_BOOTTIME     "kernel_boottime"
#define HWMON_KEY_DAYS         "kernel_days"
#define HWMON_KEY_BL_AVG       "bl_level_avg"
#define HWMON_KEY_BL_HIGH      "bl_level_high"
#define HWMON_KEY_BL_LOW       "bl_level_low"
#define HWMON_KEY_HBM_DRUATION "hbm_duration"
#define HWMON_KEY_HBM_TIMES    "hbm_times"
#define HWMON_KEY_PANEL_DEAD   "panel_dead"

#define DAY_SECS (60*60*24)

static struct dsi_panel *g_panel;
static int panel_disp_param_send_lock(struct dsi_panel *panel, int param);
int dsi_display_read_panel(struct dsi_panel *panel, struct dsi_read_config *read_config);

static int string_merge_into_buf(const char *str, int len, char *buf);
static struct dsi_read_config read_reg;
#if DSI_READ_WRITE_PANEL_DEBUG
static struct proc_dir_entry *mipi_proc_entry;
#define MIPI_PROC_NAME "mipi_reg"
#endif
enum bkl_dimming_state {
	STATE_NONE,
	STATE_DIM_BLOCK,
	STATE_DIM_RESTORE,
	STATE_ALL
};

enum dsi_dsc_ratio_type {
	DSC_8BPC_8BPP,
	DSC_10BPC_8BPP,
	DSC_12BPC_8BPP,
	DSC_RATIO_TYPE_MAX
};

static u32 dsi_dsc_rc_buf_thresh[] = {0x0e, 0x1c, 0x2a, 0x38, 0x46, 0x54,
		0x62, 0x69, 0x70, 0x77, 0x79, 0x7b, 0x7d, 0x7e};

/*
 * DSC 1.1
 * Rate control - Min QP values for each ratio type in dsi_dsc_ratio_type
 */
static char dsi_dsc_rc_range_min_qp_1_1[][15] = {
	{0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 5, 7, 13},
	{0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9, 9, 9, 11, 17},
	{0, 4, 9, 9, 11, 11, 11, 11, 11, 11, 13, 13, 13, 15, 21},
	};

/*
 * DSC 1.1 SCR
 * Rate control - Min QP values for each ratio type in dsi_dsc_ratio_type
 */
static char dsi_dsc_rc_range_min_qp_1_1_scr1[][15] = {
	{0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 5, 9, 12},
	{0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9, 9, 9, 13, 16},
	{0, 4, 9, 9, 11, 11, 11, 11, 11, 11, 13, 13, 13, 17, 20},
	};

/*
 * DSC 1.1
 * Rate control - Max QP values for each ratio type in dsi_dsc_ratio_type
 */
static char dsi_dsc_rc_range_max_qp_1_1[][15] = {
	{4, 4, 5, 6, 7, 7, 7, 8, 9, 10, 11, 12, 13, 13, 15},
	{8, 8, 9, 10, 11, 11, 11, 12, 13, 14, 15, 16, 17, 17, 19},
	{12, 12, 13, 14, 15, 15, 15, 16, 17, 18, 19, 20, 21, 21, 23},
	};

/*
 * DSC 1.1 SCR
 * Rate control - Max QP values for each ratio type in dsi_dsc_ratio_type
 */
static char dsi_dsc_rc_range_max_qp_1_1_scr1[][15] = {
	{4, 4, 5, 6, 7, 7, 7, 8, 9, 10, 10, 11, 11, 12, 13},
	{8, 8, 9, 10, 11, 11, 11, 12, 13, 14, 14, 15, 15, 16, 17},
	{12, 12, 13, 14, 15, 15, 15, 16, 17, 18, 18, 19, 19, 20, 21},
	};

/*
 * DSC 1.1 and DSC 1.1 SCR
 * Rate control - bpg offset values
 */
static char dsi_dsc_rc_range_bpg_offset[] = {2, 0, 0, -2, -4, -6, -8, -8,
		-8, -10, -10, -12, -12, -12, -12};

static int max_lum;

int dsi_dsc_create_pps_buf_cmd(struct msm_display_dsc_info *dsc, char *buf,
				int pps_id)
{
	char *bp;
	char data;
	int i, bpp;
	char *dbgbp;

	dbgbp = buf;
	bp = buf;
	/* First 7 bytes are cmd header */
	*bp++ = 0x0A;
	*bp++ = 1;
	*bp++ = 0;
	*bp++ = 0;
	*bp++ = 10;
	*bp++ = 0;
	*bp++ = 128;

	*bp++ = (dsc->version & 0xff);		/* pps0 */
	*bp++ = (pps_id & 0xff);		/* pps1 */
	bp++;					/* pps2, reserved */

	data = dsc->line_buf_depth & 0x0f;
	data |= ((dsc->bpc & 0xf) << 4);
	*bp++ = data;				/* pps3 */

	bpp = dsc->bpp;
	bpp <<= 4;				/* 4 fraction bits */
	data = (bpp >> 8);
	data &= 0x03;				/* upper two bits */
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

	*bp++ = ((dsc->initial_dec_delay >> 8) & 0xff); /* pps18 */
	*bp++ = (dsc->initial_dec_delay & 0xff);/* pps19 */

	bp++;					/* pps20, reserved */

	*bp++ = (dsc->initial_scale_value & 0x3f); /* pps21 */

	*bp++ = ((dsc->scale_increment_interval >> 8) & 0xff); /* pps22 */
	*bp++ = (dsc->scale_increment_interval & 0xff); /* pps23 */

	*bp++ = ((dsc->scale_decrement_interval >> 8) & 0xf); /* pps24 */
	*bp++ = (dsc->scale_decrement_interval & 0x0ff);/* pps25 */

	bp++;					/* pps26, reserved */

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
		*bp++ = (dsc->buf_thresh[i] & 0xff); /* pps44 - pps57 */

	for (i = 0; i < 15; i++) {		/* pps58 - pps87 */
		data = (dsc->range_min_qp[i] & 0x1f);
		data <<= 3;
		data |= ((dsc->range_max_qp[i] >> 2) & 0x07);
		*bp++ = data;
		data = (dsc->range_max_qp[i] & 0x03);
		data <<= 6;
		data |= (dsc->range_bpg_offset[i] & 0x3f);
		*bp++ = data;
	}

	return 128;
}

static int dsi_panel_vreg_get(struct dsi_panel *panel)
{
	int rc = 0;
	int i;
	struct regulator *vreg = NULL;

	for (i = 0; i < panel->power_info.count; i++) {
		vreg = devm_regulator_get(panel->parent,
					  panel->power_info.vregs[i].vreg_name);
		rc = PTR_RET(vreg);
		if (rc) {
			pr_err("failed to get %s regulator\n",
			       panel->power_info.vregs[i].vreg_name);
			goto error_put;
		}
		panel->power_info.vregs[i].vreg = vreg;
	}

	return rc;
error_put:
	for (i = i - 1; i >= 0; i--) {
		devm_regulator_put(panel->power_info.vregs[i].vreg);
		panel->power_info.vregs[i].vreg = NULL;
	}
	return rc;
}

static int dsi_panel_vreg_put(struct dsi_panel *panel)
{
	int rc = 0;
	int i;

	for (i = panel->power_info.count - 1; i >= 0; i--)
		devm_regulator_put(panel->power_info.vregs[i].vreg);

	return rc;
}

static int dsi_panel_gpio_request(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_panel_reset_config *r_config = &panel->reset_config;

	if (gpio_is_valid(r_config->reset_gpio)) {
		rc = gpio_request(r_config->reset_gpio, "reset_gpio");
		if (rc) {
			pr_err("request for reset_gpio failed, rc=%d\n", rc);
			goto error;
		}
	}

	if (gpio_is_valid(r_config->disp_en_gpio)) {
		rc = gpio_request(r_config->disp_en_gpio, "disp_en_gpio");
		if (rc) {
			pr_err("request for disp_en_gpio failed, rc=%d\n", rc);
			goto error_release_reset;
		}
	}

	if (gpio_is_valid(panel->bl_config.en_gpio)) {
		rc = gpio_request(panel->bl_config.en_gpio, "bklt_en_gpio");
		if (rc) {
			pr_err("request for bklt_en_gpio failed, rc=%d\n", rc);
			goto error_release_disp_en;
		}
	}

	if (gpio_is_valid(r_config->lcd_mode_sel_gpio)) {
		rc = gpio_request(r_config->lcd_mode_sel_gpio, "mode_gpio");
		if (rc) {
			pr_err("request for mode_gpio failed, rc=%d\n", rc);
			goto error_release_mode_sel;
		}
	}

	goto error;
error_release_mode_sel:
	if (gpio_is_valid(panel->bl_config.en_gpio))
		gpio_free(panel->bl_config.en_gpio);
error_release_disp_en:
	if (gpio_is_valid(r_config->disp_en_gpio))
		gpio_free(r_config->disp_en_gpio);
error_release_reset:
	if (gpio_is_valid(r_config->reset_gpio))
		gpio_free(r_config->reset_gpio);
error:
	return rc;
}

static int dsi_panel_gpio_release(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_panel_reset_config *r_config = &panel->reset_config;

	if (gpio_is_valid(r_config->reset_gpio))
		gpio_free(r_config->reset_gpio);

	if (gpio_is_valid(r_config->disp_en_gpio))
		gpio_free(r_config->disp_en_gpio);

	if (gpio_is_valid(panel->bl_config.en_gpio))
		gpio_free(panel->bl_config.en_gpio);

	if (gpio_is_valid(panel->reset_config.lcd_mode_sel_gpio))
		gpio_free(panel->reset_config.lcd_mode_sel_gpio);

	return rc;
}

 void drm_panel_reset_skip_enable(bool enable)
{
	if (g_panel)
		g_panel->panel_reset_skip = enable;
}

 void drm_dsi_ulps_enable(bool enable)
{
	if (g_panel) {
		g_panel->ulps_enabled = enable;
		g_panel->ulps_suspend_enabled = enable;
	}
}

int dsi_panel_trigger_esd_attack(struct dsi_panel *panel)
{
	struct dsi_panel_reset_config *r_config;

	if (!panel) {
		pr_err("Invalid panel param\n");
		return -EINVAL;
	}

	r_config = &panel->reset_config;
	if (!r_config) {
		pr_err("Invalid panel reset configuration\n");
		return -EINVAL;
	}

	if (gpio_is_valid(r_config->reset_gpio)) {
		gpio_set_value(r_config->reset_gpio, 0);
		pr_info("GPIO pulled low to simulate ESD\n");
		return 0;
	}
	pr_err("failed to pull down gpio\n");
	return -EINVAL;
}

 void drm_dsi_ulps_suspend_enable(bool enable)
{
	if (g_panel)
		g_panel->ulps_suspend_enabled = enable;
}

static int dsi_panel_reset(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_panel_reset_config *r_config = &panel->reset_config;
	int i;

	if (gpio_is_valid(panel->reset_config.disp_en_gpio)) {
		rc = gpio_direction_output(panel->reset_config.disp_en_gpio, 1);
		if (rc) {
			pr_err("unable to set dir for disp gpio rc=%d\n", rc);
			goto exit;
		}
	}

	if (r_config->count) {
		rc = gpio_direction_output(r_config->reset_gpio,
			r_config->sequence[0].level);
		if (rc) {
			pr_err("unable to set dir for rst gpio rc=%d\n", rc);
			goto exit;
		}
	}

	for (i = 0; i < r_config->count; i++) {
		gpio_set_value(r_config->reset_gpio,
			       r_config->sequence[i].level);


		if (r_config->sequence[i].sleep_ms)
			usleep_range(r_config->sequence[i].sleep_ms * 1000,
				(r_config->sequence[i].sleep_ms * 1000) + 100);
	}

	if (gpio_is_valid(panel->bl_config.en_gpio)) {
		rc = gpio_direction_output(panel->bl_config.en_gpio, 1);
		if (rc)
			pr_err("unable to set dir for bklt gpio rc=%d\n", rc);
	}

	if (gpio_is_valid(panel->reset_config.lcd_mode_sel_gpio)) {
		bool out = true;

		if ((panel->reset_config.mode_sel_state == MODE_SEL_DUAL_PORT)
				|| (panel->reset_config.mode_sel_state
					== MODE_GPIO_LOW))
			out = false;
		else if ((panel->reset_config.mode_sel_state
				== MODE_SEL_SINGLE_PORT) ||
				(panel->reset_config.mode_sel_state
				 == MODE_GPIO_HIGH))
			out = true;

		rc = gpio_direction_output(
			panel->reset_config.lcd_mode_sel_gpio, out);
		if (rc)
			pr_err("unable to set dir for mode gpio rc=%d\n", rc);
	}
exit:
	return rc;
}

static int dsi_panel_set_pinctrl_state(struct dsi_panel *panel, bool enable)
{
	int rc = 0;
	struct pinctrl_state *state;

	if (enable)
		state = panel->pinctrl.active;
	else
		state = panel->pinctrl.suspend;

	rc = pinctrl_select_state(panel->pinctrl.pinctrl, state);
	if (rc)
		pr_err("[%s] failed to set pin state, rc=%d\n", panel->name,
		       rc);

	return rc;
}


static int dsi_panel_power_on(struct dsi_panel *panel)
{
	int rc = 0;

	if (g_panel->panel_reset_skip) {
		pr_info("%s: panel reset skip\n", __func__);

		if (panel->off_keep_reset) {
			rc = dsi_panel_reset(panel);
			if (rc) {
				pr_err("[%s] failed to reset panel, rc=%d\n", panel->name, rc);
			}
		}
		return rc;
	}

	rc = dsi_pwr_enable_regulator(&panel->power_info, true);
	if (rc) {
		pr_err("[%s] failed to enable vregs, rc=%d\n", panel->name, rc);
		goto exit;
	}

	rc = dsi_panel_set_pinctrl_state(panel, true);
	if (rc) {
		pr_err("[%s] failed to set pinctrl, rc=%d\n", panel->name, rc);
		goto error_disable_vregs;
	}

	/* If LP11_INIT is set, skip panel reset here */
	if (panel->lp11_init)
		goto exit;

	rc = dsi_panel_reset(panel);
	if (rc) {
		pr_err("[%s] failed to reset panel, rc=%d\n", panel->name, rc);
		goto error_disable_gpio;
	}

	goto exit;

error_disable_gpio:
	if (gpio_is_valid(panel->reset_config.disp_en_gpio))
		gpio_set_value(panel->reset_config.disp_en_gpio, 0);

	if (gpio_is_valid(panel->bl_config.en_gpio))
		gpio_set_value(panel->bl_config.en_gpio, 0);

	(void)dsi_panel_set_pinctrl_state(panel, false);

error_disable_vregs:
	(void)dsi_pwr_enable_regulator(&panel->power_info, false);

exit:
	return rc;
}

static int dsi_panel_power_off(struct dsi_panel *panel)
{
	int rc = 0;

	if (g_panel->panel_reset_skip) {
			pr_info("%s: panel reset skip\n", __func__);
			return rc;
	}

	if (!panel->off_keep_reset) {
		if (gpio_is_valid(panel->reset_config.reset_gpio))
			gpio_set_value(panel->reset_config.reset_gpio, 0);
	}

	if (gpio_is_valid(panel->reset_config.disp_en_gpio))
		gpio_set_value(panel->reset_config.disp_en_gpio, 0);

	if (gpio_is_valid(panel->reset_config.lcd_mode_sel_gpio))
		gpio_set_value(panel->reset_config.lcd_mode_sel_gpio, 0);

	rc = dsi_panel_set_pinctrl_state(panel, false);
	if (rc) {
		pr_err("[%s] failed set pinctrl state, rc=%d\n", panel->name,
		       rc);
	}

	rc = dsi_pwr_enable_regulator(&panel->power_info, false);
	if (rc)
		pr_err("[%s] failed to enable vregs, rc=%d\n", panel->name, rc);

	return rc;
}
static int dsi_panel_tx_cmd_set(struct dsi_panel *panel,
				enum dsi_cmd_set_type type)
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

	if (panel->type == EXT_BRIDGE)
		return 0;

	mode = panel->cur_mode;

	cmds = mode->priv_info->cmd_sets[type].cmds;
	count = mode->priv_info->cmd_sets[type].count;
	state = mode->priv_info->cmd_sets[type].state;

	if (count == 0) {
		pr_debug("[%s] No commands to be sent for state(%d)\n",
			 panel->name, type);
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
			pr_err("failed to set cmds(%d), rc=%d\n", type, rc);
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

static int dsi_panel_pinctrl_deinit(struct dsi_panel *panel)
{
	int rc = 0;

	devm_pinctrl_put(panel->pinctrl.pinctrl);

	return rc;
}

static int dsi_panel_pinctrl_init(struct dsi_panel *panel)
{
	int rc = 0;

	/* TODO:  pinctrl is defined in dsi dt node */
	panel->pinctrl.pinctrl = devm_pinctrl_get(panel->parent);
	if (IS_ERR_OR_NULL(panel->pinctrl.pinctrl)) {
		rc = PTR_ERR(panel->pinctrl.pinctrl);
		pr_err("failed to get pinctrl, rc=%d\n", rc);
		goto error;
	}

	panel->pinctrl.active = pinctrl_lookup_state(panel->pinctrl.pinctrl,
						       "panel_active");
	if (IS_ERR_OR_NULL(panel->pinctrl.active)) {
		rc = PTR_ERR(panel->pinctrl.active);
		pr_err("failed to get pinctrl active state, rc=%d\n", rc);
		goto error;
	}

	panel->pinctrl.suspend =
		pinctrl_lookup_state(panel->pinctrl.pinctrl, "panel_suspend");

	if (IS_ERR_OR_NULL(panel->pinctrl.suspend)) {
		rc = PTR_ERR(panel->pinctrl.suspend);
		pr_err("failed to get pinctrl suspend state, rc=%d\n", rc);
		goto error;
	}

error:
	return rc;
}

#ifdef CONFIG_LEDS_TRIGGERS
static int dsi_panel_led_bl_register(struct dsi_panel *panel,
				struct dsi_backlight_config *bl)
{
	int rc = 0;

	led_trigger_register_simple("bkl-trigger", &bl->wled);

	/* LED APIs don't tell us directly whether a classdev has yet
	 * been registered to service this trigger. Until classdev is
	 * registered, calling led_trigger has no effect, and doesn't
	 * fail. Classdevs are associated with any registered triggers
	 * when they do register, but that is too late for FBCon.
	 * Check the cdev list directly and defer if appropriate.
	 */
	if (!bl->wled) {
		pr_err("[%s] backlight registration failed\n", panel->name);
		rc = -EINVAL;
	} else {
		read_lock(&bl->wled->leddev_list_lock);
		if (list_empty(&bl->wled->led_cdevs))
			rc = -EPROBE_DEFER;
		read_unlock(&bl->wled->leddev_list_lock);

		if (rc) {
			pr_info("[%s] backlight %s not ready, defer probe\n",
				panel->name, bl->wled->name);
			led_trigger_unregister_simple(bl->wled);
		}
	}

	return rc;
}
#else
static int dsi_panel_led_bl_register(struct dsi_panel *panel,
				struct dsi_backlight_config *bl)
{
	return 0;
}
#endif

enum {
	DDIC_VER_0,
	DDIC_VER_1 = 0x01,
	DDIC_VER_2 = 0x02,
};

#define NIGHT_MODE_BRIGHT_THRESHOLD (12)
static int do_night_bright_map(struct dsi_panel *panel,
	u32 bl_lvl)
{
	if (!panel || (bl_lvl > 0xffff)) {
		pr_err("%s: invalid params\n", __func__);
		return bl_lvl;
	}

	if (bl_lvl == 0 || bl_lvl >= NIGHT_MODE_BRIGHT_THRESHOLD)
		return bl_lvl;

	if (bl_lvl < 4)
		bl_lvl = 8;
	else if (bl_lvl < 7)
		bl_lvl = 9;
	else if (bl_lvl < 10)
		bl_lvl = 10;
	else
		bl_lvl = 11;

	pr_info("[LCD]%s: map_bl = %d\n", __func__, bl_lvl);
	return bl_lvl;
}

static int dsi_panel_update_backlight(struct dsi_panel *panel,
	u32 bl_lvl)
{
	int rc = 0;
	struct mipi_dsi_device *dsi;

	if (!panel || (bl_lvl > 0xffff)) {
		pr_err("invalid params\n");
		return -EINVAL;
	}
	if (!panel->panel_initialized) {
		pr_err("panel not initialized\n");
		return -EINVAL;
	}

	dsi = &panel->mipi_device;

	if (panel->bl_config.dcs_type_ss) {
		if (panel->bl_config.ss_panel_id == DDIC_VER_1)
			bl_lvl = do_night_bright_map(panel, bl_lvl);

		rc = mipi_dsi_dcs_set_display_brightness_ss(dsi, bl_lvl);
	} else
		rc = mipi_dsi_dcs_set_display_brightness(dsi, bl_lvl);

	if (rc < 0)
		pr_err("failed to update dcs backlight:%d\n", bl_lvl);

	return rc;
}

static void dsi_panel_wled_cabc_ctrl(struct led_trigger *trig, bool enable)
{
	struct led_classdev *led_cdev;

	if (!trig)
		return;

	//read_lock(&trig->leddev_list_lock);
	list_for_each_entry(led_cdev, &trig->led_cdevs, trig_list)
		qpnp_wled_cabc(led_cdev, enable);
	//read_unlock(&trig->leddev_list_lock);
}

static void dsi_panel_offon_mode_control(struct dsi_panel *panel, u32 bl_lvl)
{
	struct dsi_backlight_config *bl = &panel->bl_config;
	int ret;

	if (!panel->onoff_mode_enabled)
		return;

	if (bl_lvl == 0) {
		if (panel->last_bl_lvl != 0) {
			if (!panel->dsi_panel_off_mode) {
				pr_debug("%s: set display off when bl_level=0\n", __func__);
				panel->dsi_panel_off_mode = true;

				if (panel->disable_cabc) {
					panel->brightness_cmds.enabled = true;
					panel->brightness_cmds.cmds_rlen = 1;
					panel->brightness_cmds.read_cmd =
						panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_READ_BRIGHTNESS];

					ret = panel_disp_param_send_lock(panel, 0x10000000);

					if (ret > 0 && panel->last_bl_lvl * panel->brightness_cmds.rbuf[0] >= 255)
						led_trigger_event(bl->wled, panel->last_bl_lvl * panel->brightness_cmds.rbuf[0] / 255);

					dsi_panel_wled_cabc_ctrl(bl->wled, 0);
				}

				set_skip_panel_dead(true);
				pr_debug("%s: set set_skip_panel_dead = true \n", __func__);
				panel_disp_param_send_lock(panel, DISPLAY_OFF_MODE);

				if (panel->disable_cabc)
					dsi_panel_wled_cabc_ctrl(bl->wled, 1);
			}
		}
	} else {
		if (panel->last_bl_lvl == 0 && panel->dsi_panel_off_mode == true) {
			pr_debug("%s: set display on when last_bl_lvl=0\n", __func__);
			panel->dsi_panel_off_mode = false;

			set_skip_panel_dead(false);
			pr_debug("%s: set set_skip_panel_dead = false \n", __func__);
			panel_disp_param_send_lock(panel, DISPLAY_ON_MODE);
		}
	}

	return;
}

int dsi_panel_set_doze_backlight(struct dsi_display *display, u32 bl_lvl)
{
	int rc = 0;
	struct dsi_panel *panel = NULL;
	struct drm_device *drm_dev = NULL;

	if (!display || !display->panel || !display->drm_dev) {
		pr_err("invalid display/panel/drm_dev\n");
		return -EINVAL;
	}
	panel = display->panel;
	drm_dev = display->drm_dev;

	mutex_lock(&panel->panel_lock);

	if (panel->fod_hbm_enabled) {
		pr_info("%s FOD HBM open, skip value:%u\n", __func__, bl_lvl);
		mutex_unlock(&panel->panel_lock);
		return rc;
	}

	if (bl_lvl > panel->doze_backlight_threshold) {
		if (drm_dev->doze_brightness != DOZE_BRIGHTNESS_HBM) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DOZE_HBM);
			if (rc)
				pr_err("[%s] failed to send DSI_CMD_SET_DOZE_LBM cmd, rc=%d\n",
					   panel->name, rc);
			drm_dev->doze_brightness = DOZE_BRIGHTNESS_HBM;
		}
		panel->in_aod = true;

	} else if (bl_lvl <= panel->doze_backlight_threshold && bl_lvl > 0) {
		if (drm_dev->doze_brightness != DOZE_BRIGHTNESS_LBM) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DOZE_LBM);
			if (rc)
				pr_err("[%s] failed to send DSI_CMD_SET_DOZE_HBM cmd, rc=%d\n",
				       panel->name, rc);
			drm_dev->doze_brightness = DOZE_BRIGHTNESS_LBM;
		}
		panel->in_aod = true;
	} else {
		drm_dev->doze_brightness = DOZE_BRIGHTNESS_INVALID;
	}

	mutex_unlock(&panel->panel_lock);

	pr_info("%s value:%u\n", __func__, drm_dev->doze_brightness);
	return rc;
}

/*
Name: bl_mapping;
Function: map backlight value base on panel max backlight
and typical ax backlight, to make sure the same backlight
setting corresponds the same brightness(nit).
max_lum x 60% = nit_threshold, for this panel, nit below nit_threshold
							  is the same with typical panel
in_threshold/4095 = nitt_threshold/500  find the input index of nit_threshold
									   on typical panel
out_threshold = in_threshold*500/max_lum  find the out threshold index

then we get 3 points:(0,0),(in_threshold,out_threshold),(4095,4095)
for input smaller than in_threshold, mapping formula is
out = in*k1;  k1 = max_lum/500;
for input lager than in_threshold, mapping formula is
out = in*k2 + b;  k2 and b can calculate from (in_threshold,out_threshold) and (4095,4095)
*/
int bl_mapping(struct dsi_panel *panel, int in)
{
	int out;
	int threshold = 60;
	int max_lvl = panel->bl_config.brightness_max_level;
	int typ = panel->panel_bl_info[1];
	int nit_threshold = max_lum * threshold / 100;
	int in_threshold = max_lvl * nit_threshold / typ;
	int out_threshold = in_threshold * typ / max_lum;
	int delta_y = max_lvl - out_threshold;
	int delta_x = max_lvl - in_threshold;
	int b = max_lvl - delta_y * max_lvl / delta_x;

	if (in < in_threshold)
		out = in * typ / max_lum;
	else
		out = in * delta_y / delta_x + b;

	return out;
}

int set_panel_max_luminance(struct dsi_panel *panel)
{
	int panel_min_luminance = panel->panel_bl_info[2];
	int panel_max_luminance = panel->panel_bl_info[3];
	int reg_value = 0;
	int rc = 0;

	sscanf(panel->panel_read_data, "p0=%d", &reg_value);
	max_lum = panel->panel_bl_info[0] + reg_value*3/2;
	pr_info("%s:reg value %d, max bl %d\n", __func__, reg_value, max_lum);

	if (max_lum > panel_max_luminance || max_lum < panel_min_luminance) {
		pr_err("panel max brightness out of range!\n");
		max_lum = max_lum > panel_max_luminance ? panel_max_luminance : panel_min_luminance;
	}

	return rc;

}

void dsi_panel_backlight_consistency(struct dsi_panel *panel, u32 *bl_lvl)
{
	int new_bl_lvl;

	if (panel->panel_bl_info[0] != 0 && max_lum) {
		new_bl_lvl = bl_mapping(panel, *bl_lvl);
		if (new_bl_lvl < 0 || new_bl_lvl > panel->bl_config.brightness_max_level) {
			pr_err("mapped backlight %d out of range\n", new_bl_lvl);
			new_bl_lvl = *bl_lvl;
		}
		*bl_lvl = new_bl_lvl;
	}

	return;
}

int dsi_panel_enable_doze_backlight(struct dsi_panel *panel, u32 bl_lvl)
{
	int rc = 0;
	u32 bl_temp;
	struct dsi_backlight_config *bl = &panel->bl_config;

	pr_debug("enable doze backlight type:%d lvl:%d\n", bl->type, bl_lvl);

	mutex_lock(&panel->panel_lock);

	if (bl->doze_brightness_varible_flag) {
		if (bl_lvl == 0 && panel->bl_config.bl_remap_flag
				&& panel->bl_config.brightness_max_level && panel->bl_config.bl_max_level) {
			bl_temp = (panel->bl_config.bl_max_level - panel->bl_config.bl_min_level) * bl_lvl / panel->bl_config.brightness_max_level
						+ panel->bl_config.bl_min_level;
			rc = dsi_panel_update_backlight(panel, bl_temp);
		}
	} else {
		rc = dsi_panel_update_backlight(panel, bl_lvl);
	}

	panel->last_bl_lvl = bl_lvl;
	if (!bl_lvl && panel->hist_bl_offset)
		panel->hist_bl_offset = 0;

	mutex_unlock(&panel->panel_lock);

	return rc;
}

int dsi_panel_set_backlight(struct dsi_panel *panel, u32 bl_lvl)
{
	int rc = 0;
	u32 bl_temp = 0;
	struct dsi_backlight_config *bl = &panel->bl_config;

	if (panel->type == EXT_BRIDGE)
		return 0;

	dsi_panel_offon_mode_control(panel, bl_lvl);

	mutex_lock(&panel->panel_lock);
	if (bl_lvl && panel->hist_bl_offset && panel->hist_bl_offset < HIST_BL_OFFSET_LIMIT) {
		bl_lvl = bl_lvl + panel->hist_bl_offset;
		pr_info("set backlight offset:%d\n", bl_lvl);
	}

	/* add for backlight consistency */
	dsi_panel_backlight_consistency(panel, &bl_lvl);

	if (panel->bl_config.bl_remap_flag && panel->bl_config.brightness_max_level
		&& panel->bl_config.bl_max_level) {
		/*
		 * map UI brightness into driver backlight level
		 * y = kx+b;
		 */
		if (bl_lvl == 0)
			bl_temp = (panel->bl_config.bl_max_level - panel->bl_config.bl_min_level) * bl_lvl / panel->bl_config.brightness_max_level
						+ panel->bl_config.bl_min_level;
		else
			bl_temp = (panel->bl_config.bl_max_level - panel->bl_config.bl_typical_level) * bl_lvl / panel->bl_config.brightness_max_level
						+ panel->bl_config.bl_typical_level;
	} else {
		bl_temp = bl_lvl;
	}

	pr_debug("backlight type:%d lvl:%d\n", bl->type, bl_temp);
	if (panel->dc_enable && bl_temp < panel->dc_threshold && bl_temp != 0) {
		pr_info("skip set backlight bacase dc enable %d, bl %d, last_bl %d\n", panel->dc_enable, bl_temp, panel->last_bl_lvl);
		mutex_unlock(&panel->panel_lock);
		return rc;
	}
	switch (bl->type) {
	case DSI_BACKLIGHT_WLED:
		led_trigger_event(bl->wled, bl_temp);
		break;
	case DSI_BACKLIGHT_DCS:
		rc = dsi_panel_update_backlight(panel, bl_temp);
		break;
	default:
		pr_err("Backlight type(%d) not supported\n", bl->type);
		rc = -ENOTSUPP;
	}

	if ((panel->last_bl_lvl == 0 || (panel->skip_dimmingon == STATE_DIM_RESTORE)) && bl_lvl) {
		if (panel->panel_on_dimming_delay)
			schedule_delayed_work(&panel->cmds_work,
				msecs_to_jiffies(panel->panel_on_dimming_delay));

		if (panel->skip_dimmingon == STATE_DIM_RESTORE)
			panel->skip_dimmingon = STATE_NONE;
	}
	panel->last_bl_lvl = bl_lvl;
	if (!bl_lvl && panel->hist_bl_offset)
		panel->hist_bl_offset = 0;

	mutex_unlock(&panel->panel_lock);

	return rc;
}

static int dsi_panel_bl_register(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_backlight_config *bl = &panel->bl_config;

	switch (bl->type) {
	case DSI_BACKLIGHT_WLED:
		rc = dsi_panel_led_bl_register(panel, bl);
		break;
	case DSI_BACKLIGHT_DCS:
		break;
	default:
		pr_err("Backlight type(%d) not supported\n", bl->type);
		rc = -ENOTSUPP;
		goto error;
	}

error:
	return rc;
}

static int dsi_panel_bl_unregister(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_backlight_config *bl = &panel->bl_config;

	switch (bl->type) {
	case DSI_BACKLIGHT_WLED:
		led_trigger_unregister_simple(bl->wled);
		break;
	case DSI_BACKLIGHT_DCS:
		break;
	default:
		pr_err("Backlight type(%d) not supported\n", bl->type);
		rc = -ENOTSUPP;
		goto error;
	}

error:
	return rc;
}

static int dsi_panel_fw_parse(const struct firmware *fw_entry,
		char *id_match, u32 *param_value)
{
	int value, numlen = 1, index = 0;
	char id[SZ_256];

	while (sscanf(fw_entry->data + index,
			"%255s %d", id, &value) > 0) {
		if (!strcmp(id, id_match)) {
			*param_value = value;
			return 0;
		}

		while ((value / 10) > 0) {
			value /= 10;
			numlen++;
		}

		index += (strlen(id) + numlen + 1);
		numlen = 1;
	}

	return -EINVAL;
}

static int dsi_panel_parse(struct device_node *of_node,
	const struct firmware *fw_entry, char *id_match, u32 *val)
{
	if (fw_entry && fw_entry->data)
		return dsi_panel_fw_parse(fw_entry, id_match, val);
	else
		return of_property_read_u32(of_node, id_match, val);

	return 0;
}

static int dsi_panel_parse_timing(struct device *parent,
	struct dsi_mode_info *mode, const char *name,
	struct device_node *of_node)
{
	int fw = 0, rc = 0;
	u64 tmp64;
	struct dsi_display_mode *display_mode;
	const struct firmware *fw_entry = NULL;
	char *fw_name = "dsi_prop";

	if (strcmp(name, "Simulator video mode dsi panel") == 0)
		fw = request_firmware(&fw_entry, fw_name, parent);

	if (fw)
		fw_entry = NULL;

	display_mode = container_of(mode, struct dsi_display_mode, timing);

	rc = of_property_read_u64(of_node,
			"qcom,mdss-dsi-panel-clockrate", &tmp64);
	if (rc == -EOVERFLOW) {
		tmp64 = 0;
		rc = of_property_read_u32(of_node,
			"qcom,mdss-dsi-panel-clockrate", (u32 *)&tmp64);
	}

	mode->clk_rate_hz = !rc ? tmp64 : 0;
	display_mode->priv_info->clk_rate_hz = mode->clk_rate_hz;

	rc = dsi_panel_parse(of_node, fw_entry,
		"qcom,mdss-dsi-panel-framerate", &mode->refresh_rate);
	if (rc) {
		pr_err("failed to read qcom,mdss-dsi-panel-framerate, rc=%d\n",
		       rc);
		goto error;
	}

	rc = dsi_panel_parse(of_node, fw_entry,
		"qcom,mdss-dsi-panel-width", &mode->h_active);
	if (rc) {
		pr_err("failed to read qcom,mdss-dsi-panel-width, rc=%d\n", rc);
		goto error;
	}

	rc = dsi_panel_parse(of_node, fw_entry,
		"qcom,mdss-dsi-h-front-porch", &mode->h_front_porch);
	if (rc) {
		pr_err("failed to read qcom,mdss-dsi-h-front-porch, rc=%d\n",
		       rc);
		goto error;
	}

	rc = dsi_panel_parse(of_node, fw_entry,
		"qcom,mdss-dsi-h-back-porch", &mode->h_back_porch);
	if (rc) {
		pr_err("failed to read qcom,mdss-dsi-h-back-porch, rc=%d\n",
		       rc);
		goto error;
	}

	rc = dsi_panel_parse(of_node, fw_entry,
		"qcom,mdss-dsi-h-pulse-width", &mode->h_sync_width);
	if (rc) {
		pr_err("failed to read qcom,mdss-dsi-h-pulse-width, rc=%d\n",
		       rc);
		goto error;
	}

	rc = dsi_panel_parse(of_node, fw_entry,
		"qcom,mdss-dsi-h-sync-skew", &mode->h_skew);
	if (rc)
		pr_err("qcom,mdss-dsi-h-sync-skew is not defined, rc=%d\n", rc);

	pr_debug("panel horz active:%d front_portch:%d back_porch:%d sync_skew:%d\n",
		mode->h_active, mode->h_front_porch, mode->h_back_porch,
		mode->h_sync_width);

	rc = dsi_panel_parse(of_node, fw_entry,
		"qcom,mdss-dsi-panel-height", &mode->v_active);
	if (rc) {
		pr_err("failed to read qcom,mdss-dsi-panel-height, rc=%d\n",
		       rc);
		goto error;
	}

	rc = dsi_panel_parse(of_node, fw_entry,
		"qcom,mdss-dsi-v-back-porch", &mode->v_back_porch);
	if (rc) {
		pr_err("failed to read qcom,mdss-dsi-v-back-porch, rc=%d\n",
		       rc);
		goto error;
	}

	rc = dsi_panel_parse(of_node, fw_entry,
		"qcom,mdss-dsi-v-front-porch", &mode->v_front_porch);
	if (rc) {
		pr_err("failed to read qcom,mdss-dsi-v-back-porch, rc=%d\n",
		       rc);
		goto error;
	}

	rc = dsi_panel_parse(of_node, fw_entry,
		"qcom,mdss-dsi-v-pulse-width", &mode->v_sync_width);
	if (rc) {
		pr_err("failed to read qcom,mdss-dsi-v-pulse-width, rc=%d\n",
		       rc);
		goto error;
	}
	pr_debug("panel vert active:%d front_portch:%d back_porch:%d pulse_width:%d\n",
		mode->v_active, mode->v_front_porch, mode->v_back_porch,
		mode->v_sync_width);

error:
	return rc;
}

static int dsi_panel_parse_pixel_format(struct dsi_host_common_cfg *host,
					struct device_node *of_node,
					const char *name)
{
	int rc = 0;
	u32 bpp = 0;
	enum dsi_pixel_format fmt;
	const char *packing;

	rc = of_property_read_u32(of_node, "qcom,mdss-dsi-bpp", &bpp);
	if (rc) {
		pr_err("[%s] failed to read qcom,mdss-dsi-bpp, rc=%d\n",
		       name, rc);
		return rc;
	}

	switch (bpp) {
	case 3:
		fmt = DSI_PIXEL_FORMAT_RGB111;
		break;
	case 8:
		fmt = DSI_PIXEL_FORMAT_RGB332;
		break;
	case 12:
		fmt = DSI_PIXEL_FORMAT_RGB444;
		break;
	case 16:
		fmt = DSI_PIXEL_FORMAT_RGB565;
		break;
	case 18:
		fmt = DSI_PIXEL_FORMAT_RGB666;
		break;
	case 24:
	default:
		fmt = DSI_PIXEL_FORMAT_RGB888;
		break;
	}

	if (fmt == DSI_PIXEL_FORMAT_RGB666) {
		packing = of_get_property(of_node,
					  "qcom,mdss-dsi-pixel-packing",
					  NULL);
		if (packing && !strcmp(packing, "loose"))
			fmt = DSI_PIXEL_FORMAT_RGB666_LOOSE;
	}

	host->dst_format = fmt;
	return rc;
}

static int dsi_panel_parse_lane_states(struct dsi_host_common_cfg *host,
				       struct device_node *of_node,
				       const char *name)
{
	int rc = 0;
	bool lane_enabled;

	lane_enabled = of_property_read_bool(of_node,
					    "qcom,mdss-dsi-lane-0-state");
	host->data_lanes |= (lane_enabled ? DSI_DATA_LANE_0 : 0);

	lane_enabled = of_property_read_bool(of_node,
					     "qcom,mdss-dsi-lane-1-state");
	host->data_lanes |= (lane_enabled ? DSI_DATA_LANE_1 : 0);

	lane_enabled = of_property_read_bool(of_node,
					    "qcom,mdss-dsi-lane-2-state");
	host->data_lanes |= (lane_enabled ? DSI_DATA_LANE_2 : 0);

	lane_enabled = of_property_read_bool(of_node,
					     "qcom,mdss-dsi-lane-3-state");
	host->data_lanes |= (lane_enabled ? DSI_DATA_LANE_3 : 0);

	if (host->data_lanes == 0) {
		pr_err("[%s] No data lanes are enabled, rc=%d\n", name, rc);
		rc = -EINVAL;
	}

	return rc;
}

static int dsi_panel_parse_color_swap(struct dsi_host_common_cfg *host,
				      struct device_node *of_node,
				      const char *name)
{
	int rc = 0;
	const char *swap_mode;

	swap_mode = of_get_property(of_node, "qcom,mdss-dsi-color-order", NULL);
	if (swap_mode) {
		if (!strcmp(swap_mode, "rgb_swap_rgb")) {
			host->swap_mode = DSI_COLOR_SWAP_RGB;
		} else if (!strcmp(swap_mode, "rgb_swap_rbg")) {
			host->swap_mode = DSI_COLOR_SWAP_RBG;
		} else if (!strcmp(swap_mode, "rgb_swap_brg")) {
			host->swap_mode = DSI_COLOR_SWAP_BRG;
		} else if (!strcmp(swap_mode, "rgb_swap_grb")) {
			host->swap_mode = DSI_COLOR_SWAP_GRB;
		} else if (!strcmp(swap_mode, "rgb_swap_gbr")) {
			host->swap_mode = DSI_COLOR_SWAP_GBR;
		} else {
			pr_err("[%s] Unrecognized color order-%s\n",
			       name, swap_mode);
			rc = -EINVAL;
		}
	} else {
		pr_debug("[%s] Falling back to default color order\n", name);
		host->swap_mode = DSI_COLOR_SWAP_RGB;
	}

	/* bit swap on color channel is not defined in dt */
	host->bit_swap_red = false;
	host->bit_swap_green = false;
	host->bit_swap_blue = false;
	return rc;
}

static int dsi_panel_parse_triggers(struct dsi_host_common_cfg *host,
				    struct device_node *of_node,
				    const char *name)
{
	const char *trig;
	int rc = 0;

	trig = of_get_property(of_node, "qcom,mdss-dsi-mdp-trigger", NULL);
	if (trig) {
		if (!strcmp(trig, "none")) {
			host->mdp_cmd_trigger = DSI_TRIGGER_NONE;
		} else if (!strcmp(trig, "trigger_te")) {
			host->mdp_cmd_trigger = DSI_TRIGGER_TE;
		} else if (!strcmp(trig, "trigger_sw")) {
			host->mdp_cmd_trigger = DSI_TRIGGER_SW;
		} else if (!strcmp(trig, "trigger_sw_te")) {
			host->mdp_cmd_trigger = DSI_TRIGGER_SW_TE;
		} else {
			pr_err("[%s] Unrecognized mdp trigger type (%s)\n",
			       name, trig);
			rc = -EINVAL;
		}

	} else {
		pr_debug("[%s] Falling back to default MDP trigger\n",
			 name);
		host->mdp_cmd_trigger = DSI_TRIGGER_SW;
	}

	trig = of_get_property(of_node, "qcom,mdss-dsi-dma-trigger", NULL);
	if (trig) {
		if (!strcmp(trig, "none")) {
			host->dma_cmd_trigger = DSI_TRIGGER_NONE;
		} else if (!strcmp(trig, "trigger_te")) {
			host->dma_cmd_trigger = DSI_TRIGGER_TE;
		} else if (!strcmp(trig, "trigger_sw")) {
			host->dma_cmd_trigger = DSI_TRIGGER_SW;
		} else if (!strcmp(trig, "trigger_sw_seof")) {
			host->dma_cmd_trigger = DSI_TRIGGER_SW_SEOF;
		} else if (!strcmp(trig, "trigger_sw_te")) {
			host->dma_cmd_trigger = DSI_TRIGGER_SW_TE;
		} else {
			pr_err("[%s] Unrecognized mdp trigger type (%s)\n",
			       name, trig);
			rc = -EINVAL;
		}

	} else {
		pr_debug("[%s] Falling back to default MDP trigger\n", name);
		host->dma_cmd_trigger = DSI_TRIGGER_SW;
	}

	rc = of_property_read_u32(of_node, "qcom,mdss-dsi-te-pin-select",
			&host->te_mode);
	if (rc) {
		pr_warn("[%s] fallback to default te-pin-select\n", name);
		host->te_mode = 1;
		rc = 0;
	}

	return rc;
}

static int dsi_panel_parse_misc_host_config(struct dsi_host_common_cfg *host,
					    struct device_node *of_node,
					    const char *name)
{
	u32 val = 0;
	int rc = 0;

	rc = of_property_read_u32(of_node, "qcom,mdss-dsi-t-clk-post", &val);
	if (rc) {
		pr_debug("[%s] Fallback to default t_clk_post value\n", name);
		host->t_clk_post = 0x03;
	} else {
		host->t_clk_post = val;
		pr_debug("[%s] t_clk_post = %d\n", name, val);
	}

	val = 0;
	rc = of_property_read_u32(of_node, "qcom,mdss-dsi-t-clk-pre", &val);
	if (rc) {
		pr_debug("[%s] Fallback to default t_clk_pre value\n", name);
		host->t_clk_pre = 0x24;
	} else {
		host->t_clk_pre = val;
		pr_debug("[%s] t_clk_pre = %d\n", name, val);
	}

	host->ignore_rx_eot = of_property_read_bool(of_node,
						"qcom,mdss-dsi-rx-eot-ignore");

	host->append_tx_eot = of_property_read_bool(of_node,
						"qcom,mdss-dsi-tx-eot-append");

	host->force_hs_clk_lane = of_property_read_bool(of_node,
					"qcom,mdss-dsi-force-clock-lane-hs");
	return 0;
}

static int dsi_panel_parse_host_config(struct dsi_panel *panel,
				       struct device_node *of_node)
{
	int rc = 0;

	rc = dsi_panel_parse_pixel_format(&panel->host_config, of_node,
					  panel->name);
	if (rc) {
		pr_err("[%s] failed to get pixel format, rc=%d\n",
		panel->name, rc);
		goto error;
	}

	rc = dsi_panel_parse_lane_states(&panel->host_config, of_node,
					 panel->name);
	if (rc) {
		pr_err("[%s] failed to parse lane states, rc=%d\n",
		       panel->name, rc);
		goto error;
	}

	rc = dsi_panel_parse_color_swap(&panel->host_config, of_node,
					panel->name);
	if (rc) {
		pr_err("[%s] failed to parse color swap config, rc=%d\n",
		       panel->name, rc);
		goto error;
	}

	rc = dsi_panel_parse_triggers(&panel->host_config, of_node,
				      panel->name);
	if (rc) {
		pr_err("[%s] failed to parse triggers, rc=%d\n",
		       panel->name, rc);
		goto error;
	}

	rc = dsi_panel_parse_misc_host_config(&panel->host_config, of_node,
					      panel->name);
	if (rc) {
		pr_err("[%s] failed to parse misc host config, rc=%d\n",
		       panel->name, rc);
		goto error;
	}

error:
	return rc;
}

static int dsi_panel_parse_dyn_clk_caps(struct dsi_dyn_clk_caps *dyn_clk_caps,
				     struct device_node *of_node,
				     const char *name)
{
	int rc = 0;
	bool supported = false;

	supported = of_property_read_bool(of_node, "qcom,dsi-dyn-clk-enable");

	if (!supported) {
		dyn_clk_caps->dyn_clk_support = false;
		return rc;
	}

	of_find_property(of_node, "qcom,dsi-dyn-clk-list",
			      &dyn_clk_caps->bit_clk_list_len);
	dyn_clk_caps->bit_clk_list_len /= sizeof(u32);
	if (dyn_clk_caps->bit_clk_list_len < 1) {
		pr_err("[%s] failed to get supported bit clk list\n", name);
		return -EINVAL;
	}

	dyn_clk_caps->bit_clk_list = kcalloc(dyn_clk_caps->bit_clk_list_len,
					     sizeof(u32), GFP_KERNEL);
	if (!dyn_clk_caps->bit_clk_list)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node, "qcom,dsi-dyn-clk-list",
				   dyn_clk_caps->bit_clk_list,
				   dyn_clk_caps->bit_clk_list_len);
	if (rc) {
		pr_err("[%s] failed to parse supported bit clk list\n", name);
		return -EINVAL;
	}

	dyn_clk_caps->dyn_clk_support = true;

	return 0;
}

static int dsi_panel_parse_dfps_caps(struct dsi_dfps_capabilities *dfps_caps,
				     struct device_node *of_node,
				     const char *name)
{
	int rc = 0;
	bool supported = false;
	const char *type;
	u32 i;

	supported = of_property_read_bool(of_node,
					"qcom,mdss-dsi-pan-enable-dynamic-fps");

	if (!supported) {
		pr_debug("[%s] DFPS is not supported\n", name);
		dfps_caps->dfps_support = false;
		return rc;
	}

	type = of_get_property(of_node,
			       "qcom,mdss-dsi-pan-fps-update",
			       NULL);
	if (!type) {
		pr_err("[%s] dfps type not defined\n", name);
		rc = -EINVAL;
		goto error;
	} else if (!strcmp(type, "dfps_suspend_resume_mode")) {
		dfps_caps->type = DSI_DFPS_SUSPEND_RESUME;
	} else if (!strcmp(type, "dfps_immediate_clk_mode")) {
		dfps_caps->type = DSI_DFPS_IMMEDIATE_CLK;
	} else if (!strcmp(type, "dfps_immediate_porch_mode_hfp")) {
		dfps_caps->type = DSI_DFPS_IMMEDIATE_HFP;
	} else if (!strcmp(type, "dfps_immediate_porch_mode_vfp")) {
		dfps_caps->type = DSI_DFPS_IMMEDIATE_VFP;
	} else {
		pr_err("[%s] dfps type is not recognized\n", name);
		rc = -EINVAL;
		goto error;
	}

	of_find_property(of_node, "qcom,dsi-supported-dfps-list",
			 &dfps_caps->dfps_list_len);
	dfps_caps->dfps_list_len /= sizeof(u32);
	if (dfps_caps->dfps_list_len < 1) {
		pr_err("[%s] dfps refresh list not present\n", name);
		rc = -EINVAL;
		goto error;
	}

	dfps_caps->dfps_list = kcalloc(dfps_caps->dfps_list_len, sizeof(u32),
				       GFP_KERNEL);
	if (!dfps_caps->dfps_list) {
		rc = -ENOMEM;
		goto error;
	}

	rc = of_property_read_u32_array(of_node, "qcom,dsi-supported-dfps-list",
					dfps_caps->dfps_list,
					dfps_caps->dfps_list_len);
	if (rc) {
		pr_err("[%s] dfps refresh rate list parse failed\n", name);
		rc = -EINVAL;
		goto error;
	}

	dfps_caps->dfps_support = true;

	/* calculate max and min fps */
	dfps_caps->max_refresh_rate = dfps_caps->dfps_list[0];
	dfps_caps->min_refresh_rate = dfps_caps->dfps_list[0];

	for (i = 1; i < dfps_caps->dfps_list_len; i++) {
		if (dfps_caps->dfps_list[i] < dfps_caps->min_refresh_rate)
			dfps_caps->min_refresh_rate = dfps_caps->dfps_list[i];
		else if (dfps_caps->dfps_list[i] > dfps_caps->max_refresh_rate)
			dfps_caps->max_refresh_rate = dfps_caps->dfps_list[i];
	}
error:
	return rc;
}

static int dsi_panel_parse_video_host_config(struct dsi_video_engine_cfg *cfg,
					     struct device_node *of_node,
					     const char *name)
{
	int rc = 0;
	const char *traffic_mode;
	u32 vc_id = 0;
	u32 val = 0;
	u32 line_no = 0;

	rc = of_property_read_u32(of_node, "qcom,mdss-dsi-h-sync-pulse", &val);
	if (rc) {
		pr_debug("[%s] fallback to default h-sync-pulse\n", name);
		cfg->pulse_mode_hsa_he = false;
	} else if (val == 1) {
		cfg->pulse_mode_hsa_he = true;
	} else if (val == 0) {
		cfg->pulse_mode_hsa_he = false;
	} else {
		pr_err("[%s] Unrecognized value for mdss-dsi-h-sync-pulse\n",
		       name);
		rc = -EINVAL;
		goto error;
	}

	cfg->hfp_lp11_en = of_property_read_bool(of_node,
						"qcom,mdss-dsi-hfp-power-mode");

	cfg->hbp_lp11_en = of_property_read_bool(of_node,
						"qcom,mdss-dsi-hbp-power-mode");

	cfg->hsa_lp11_en = of_property_read_bool(of_node,
						"qcom,mdss-dsi-hsa-power-mode");

	cfg->last_line_interleave_en = of_property_read_bool(of_node,
					"qcom,mdss-dsi-last-line-interleave");

	cfg->eof_bllp_lp11_en = of_property_read_bool(of_node,
					"qcom,mdss-dsi-bllp-eof-power-mode");

	cfg->bllp_lp11_en = of_property_read_bool(of_node,
					"qcom,mdss-dsi-bllp-power-mode");

	traffic_mode = of_get_property(of_node,
				       "qcom,mdss-dsi-traffic-mode",
				       NULL);
	if (!traffic_mode) {
		pr_debug("[%s] Falling back to default traffic mode\n", name);
		cfg->traffic_mode = DSI_VIDEO_TRAFFIC_SYNC_PULSES;
	} else if (!strcmp(traffic_mode, "non_burst_sync_pulse")) {
		cfg->traffic_mode = DSI_VIDEO_TRAFFIC_SYNC_PULSES;
	} else if (!strcmp(traffic_mode, "non_burst_sync_event")) {
		cfg->traffic_mode = DSI_VIDEO_TRAFFIC_SYNC_START_EVENTS;
	} else if (!strcmp(traffic_mode, "burst_mode")) {
		cfg->traffic_mode = DSI_VIDEO_TRAFFIC_BURST_MODE;
	} else {
		pr_err("[%s] Unrecognized traffic mode-%s\n", name,
		       traffic_mode);
		rc = -EINVAL;
		goto error;
	}

	rc = of_property_read_u32(of_node, "qcom,mdss-dsi-virtual-channel-id",
				  &vc_id);
	if (rc) {
		pr_debug("[%s] Fallback to default vc id\n", name);
		cfg->vc_id = 0;
	} else {
		cfg->vc_id = vc_id;
	}

	rc = of_property_read_u32(of_node, "qcom,mdss-dsi-dma-schedule-line",
				  &line_no);
	if (rc) {
		pr_debug("[%s] set default dma scheduling line no\n", name);
		cfg->dma_sched_line = 0x1;
		/* do not fail since we have default value */
		rc = 0;
	} else {
		cfg->dma_sched_line = line_no;
	}

error:
	return rc;
}

static int dsi_panel_parse_cmd_host_config(struct dsi_cmd_engine_cfg *cfg,
					   struct device_node *of_node,
					   const char *name)
{
	u32 val = 0;
	int rc = 0;

	rc = of_property_read_u32(of_node, "qcom,mdss-dsi-wr-mem-start", &val);
	if (rc) {
		pr_debug("[%s] Fallback to default wr-mem-start\n", name);
		cfg->wr_mem_start = 0x2C;
	} else {
		cfg->wr_mem_start = val;
	}

	val = 0;
	rc = of_property_read_u32(of_node, "qcom,mdss-dsi-wr-mem-continue",
				  &val);
	if (rc) {
		pr_debug("[%s] Fallback to default wr-mem-continue\n", name);
		cfg->wr_mem_continue = 0x3C;
	} else {
		cfg->wr_mem_continue = val;
	}

	/* TODO:  fix following */
	cfg->max_cmd_packets_interleave = 0;

	val = 0;
	rc = of_property_read_u32(of_node, "qcom,mdss-dsi-te-dcs-command",
				  &val);
	if (rc) {
		pr_debug("[%s] fallback to default te-dcs-cmd\n", name);
		cfg->insert_dcs_command = true;
	} else if (val == 1) {
		cfg->insert_dcs_command = true;
	} else if (val == 0) {
		cfg->insert_dcs_command = false;
	} else {
		pr_err("[%s] Unrecognized value for mdss-dsi-te-dcs-command\n",
		       name);
		rc = -EINVAL;
		goto error;
	}

	if (of_property_read_u32(of_node, "qcom,mdss-mdp-transfer-time-us",
				&val)) {
		pr_debug("[%s] Fallback to default transfer-time-us\n", name);
		cfg->mdp_transfer_time_us = DEFAULT_MDP_TRANSFER_TIME;
	} else {
		cfg->mdp_transfer_time_us = val;
	}

error:
	return rc;
}

static int dsi_panel_parse_panel_mode(struct dsi_panel *panel,
				      struct device_node *of_node)
{
	int rc = 0;
	enum dsi_op_mode panel_mode;
	const char *mode;

	mode = of_get_property(of_node, "qcom,mdss-dsi-panel-type", NULL);
	if (!mode) {
		pr_debug("[%s] Fallback to default panel mode\n", panel->name);
		panel_mode = DSI_OP_VIDEO_MODE;
	} else if (!strcmp(mode, "dsi_video_mode")) {
		panel_mode = DSI_OP_VIDEO_MODE;
	} else if (!strcmp(mode, "dsi_cmd_mode")) {
		panel_mode = DSI_OP_CMD_MODE;
	} else {
		pr_err("[%s] Unrecognized panel type-%s\n", panel->name, mode);
		rc = -EINVAL;
		goto error;
	}

	if (panel_mode == DSI_OP_VIDEO_MODE) {
		rc = dsi_panel_parse_video_host_config(&panel->video_config,
						       of_node,
						       panel->name);
		if (rc) {
			pr_err("[%s] Failed to parse video host cfg, rc=%d\n",
			       panel->name, rc);
			goto error;
		}
	}

	if (panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_panel_parse_cmd_host_config(&panel->cmd_config,
						     of_node,
						     panel->name);
		if (rc) {
			pr_err("[%s] Failed to parse cmd host config, rc=%d\n",
			       panel->name, rc);
			goto error;
		}
	}

	panel->panel_mode = panel_mode;
error:
	return rc;
}

static int dsi_panel_parse_phy_props(struct dsi_panel_phy_props *props,
				     struct device_node *of_node,
				     const char *name)
{
	int rc = 0;
	u32 val = 0;
	const char *str;

	rc = of_property_read_u32(of_node,
				  "qcom,mdss-pan-physical-width-dimension",
				  &val);
	if (rc) {
		pr_debug("[%s] Physical panel width is not defined\n", name);
		props->panel_width_mm = 0;
		rc = 0;
	} else {
		props->panel_width_mm = val;
	}

	rc = of_property_read_u32(of_node,
				  "qcom,mdss-pan-physical-height-dimension",
				  &val);
	if (rc) {
		pr_debug("[%s] Physical panel height is not defined\n", name);
		props->panel_height_mm = 0;
		rc = 0;
	} else {
		props->panel_height_mm = val;
	}

	str = of_get_property(of_node, "qcom,mdss-dsi-panel-orientation", NULL);
	if (!str) {
		props->rotation = DSI_PANEL_ROTATE_NONE;
	} else if (!strcmp(str, "180")) {
		props->rotation = DSI_PANEL_ROTATE_HV_FLIP;
	} else if (!strcmp(str, "hflip")) {
		props->rotation = DSI_PANEL_ROTATE_H_FLIP;
	} else if (!strcmp(str, "vflip")) {
		props->rotation = DSI_PANEL_ROTATE_V_FLIP;
	} else {
		pr_err("[%s] Unrecognized panel rotation-%s\n", name, str);
		rc = -EINVAL;
		goto error;
	}
error:
	return rc;
}
const char *cmd_set_prop_map[DSI_CMD_SET_MAX] = {
	"qcom,mdss-dsi-pre-on-command",
	"qcom,mdss-dsi-on-command",
	"qcom,mdss-dsi-post-panel-on-command",
	"qcom,mdss-dsi-pre-off-command",
	"qcom,mdss-dsi-off-command",
	"qcom,mdss-dsi-post-off-command",
	"qcom,mdss-dsi-pre-res-switch",
	"qcom,mdss-dsi-res-switch",
	"qcom,mdss-dsi-post-res-switch",
	"qcom,cmd-to-video-mode-switch-commands",
	"qcom,cmd-to-video-mode-post-switch-commands",
	"qcom,video-to-cmd-mode-switch-commands",
	"qcom,video-to-cmd-mode-post-switch-commands",
	"qcom,mdss-dsi-panel-status-command",
	"qcom,mdss-dsi-lp1-command",
	"qcom,mdss-dsi-lp2-command",
	"qcom,mdss-dsi-nolp-command",
	"qcom,mdss-dsi-doze-hbm-command",
	"qcom,mdss-dsi-doze-lbm-command",
	"PPS not parsed from DTSI, generated dynamically",
	"ROI not parsed from DTSI, generated dynamically",
	"qcom,mdss-dsi-timing-switch-command",
	"qcom,mdss-dsi-post-mode-switch-on-command",
	"qcom,mdss-dsi-dispparam-warm-command",
	"qcom,mdss-dsi-dispparam-default-command",
	"qcom,mdss-dsi-dispparam-cold-command",
	"qcom,mdss-dsi-dispparam-papermode-command",
	"qcom,mdss-dsi-dispparam-papermode1-command",
	"qcom,mdss-dsi-dispparam-papermode2-command",
	"qcom,mdss-dsi-dispparam-papermode3-command",
	"qcom,mdss-dsi-dispparam-papermode4-command",
	"qcom,mdss-dsi-dispparam-papermode5-command",
	"qcom,mdss-dsi-dispparam-papermode6-command",
	"qcom,mdss-dsi-dispparam-papermode7-command",
	"qcom,mdss-dsi-dispparam-normal1-command",
	"qcom,mdss-dsi-dispparam-normal2-command",
	"qcom,mdss-dsi-dispparam-srgb-command",
	"qcom,mdss-dsi-dispparam-ceon-command",
	"qcom,mdss-dsi-dispparam-ceoff-command",
	"qcom,mdss-dsi-dispparam-cabcuion-command",
	"qcom,mdss-dsi-dispparam-cabcstillon-command",
	"qcom,mdss-dsi-dispparam-cabcmovieon-command",
	"qcom,mdss-dsi-dispparam-cabcoff-command",
	"qcom,mdss-dsi-dispparam-skince-cabcuion-command",
	"qcom,mdss-dsi-dispparam-skince-cabcstillon-command",
	"qcom,mdss-dsi-dispparam-skince-cabcmovieon-command",
	"qcom,mdss-dsi-dispparam-skince-cabcoff-command",
	"qcom,mdss-dsi-dispparam-dimmingon-command",
	"qcom,mdss-dsi-dispparam-dimmingoff-command",
	"qcom,mdss-dsi-dispparam-acl-off-command",
	"qcom,mdss-dsi-dispparam-acl-l1-command",
	"qcom,mdss-dsi-dispparam-acl-l2-command",
	"qcom,mdss-dsi-dispparam-acl-l3-command",
	"qcom,mdss-dsi-dispparam-hbm-on-command",
	"qcom,mdss-dsi-dispparam-hbm-off-command",
	"qcom,mdss-dsi-dispparam-hbm-fod-on-command",
	"qcom,mdss-dsi-dispparam-hbm-fod-off-command",
	"qcom,mdss-dsi-dispparam-hbm-fod2norm-command",
	"qcom,mdss-dsi-displayoff-command",
	"qcom,mdss-dsi-displayon-command",
	"qcom,mdss-dsi-dispparam-xy-coordinate-command",
	"qcom,mdss-dsi-read-brightness-command",
	"qcom,mdss-dsi-dispparam-max-luminance-command",
	"qcom,mdss-dsi-dispparam-max-luminance-valid-command",
	"qcom,mdss-dsi-dispparam-crc-srgb-on-command",
	"qcom,mdss-dsi-dispparam-crc-dcip3-on-command",
	"qcom,mdss-dsi-dispparam-crc-off-command",
	"qcom,mdss-dsi-read-panel-id-command",
	"qcom,mdss-dsi-dispparam-flash-test-on-command",
	"qcom,mdss-dsi-dispparam-flash-test-off-command",
};

const char *cmd_set_state_map[DSI_CMD_SET_MAX] = {
	"qcom,mdss-dsi-pre-on-command-state",
	"qcom,mdss-dsi-on-command-state",
	"qcom,mdss-dsi-post-on-command-state",
	"qcom,mdss-dsi-pre-off-command-state",
	"qcom,mdss-dsi-off-command-state",
	"qcom,mdss-dsi-post-off-command-state",
	"qcom,mdss-dsi-pre-res-switch-state",
	"qcom,mdss-dsi-res-switch-state",
	"qcom,mdss-dsi-post-res-switch-state",
	"qcom,cmd-to-video-mode-switch-commands-state",
	"qcom,cmd-to-video-mode-post-switch-commands-state",
	"qcom,video-to-cmd-mode-switch-commands-state",
	"qcom,video-to-cmd-mode-post-switch-commands-state",
	"qcom,mdss-dsi-panel-status-command-state",
	"qcom,mdss-dsi-lp1-command-state",
	"qcom,mdss-dsi-lp2-command-state",
	"qcom,mdss-dsi-nolp-command-state",
	"qcom,mdss-dsi-doze-hbm-command-state",
	"qcom,mdss-dsi-doze-lbm-command-state",
	"PPS not parsed from DTSI, generated dynamically",
	"ROI not parsed from DTSI, generated dynamically",
	"qcom,mdss-dsi-timing-switch-command-state",
	"qcom,mdss-dsi-post-mode-switch-on-command-state",
	"qcom,mdss-dsi-dispparam-warm-command-state",
	"qcom,mdss-dsi-dispparam-default-command-state",
	"qcom,mdss-dsi-dispparam-cold-command-state",
	"qcom,mdss-dsi-dispparam-papermode-command-state",
	"qcom,mdss-dsi-dispparam-papermode1-command-state",
	"qcom,mdss-dsi-dispparam-papermode2-command-state",
	"qcom,mdss-dsi-dispparam-papermode3-command-state",
	"qcom,mdss-dsi-dispparam-papermode4-command-state",
	"qcom,mdss-dsi-dispparam-papermode5-command-state",
	"qcom,mdss-dsi-dispparam-papermode6-command-state",
	"qcom,mdss-dsi-dispparam-papermode7-command-state",
	"qcom,mdss-dsi-dispparam-normal1-command-state",
	"qcom,mdss-dsi-dispparam-normal2-command-state",
	"qcom,mdss-dsi-dispparam-srgb-command-state",
	"qcom,mdss-dsi-dispparam-ceon-command-state",
	"qcom,mdss-dsi-dispparam-ceoff-command-state",
	"qcom,mdss-dsi-dispparam-cabcuion-command-state",
	"qcom,mdss-dsi-dispparam-cabcstillon-command-state",
	"qcom,mdss-dsi-dispparam-cabcmovieon-command-state",
	"qcom,mdss-dsi-dispparam-cabcoff-command-state",
	"qcom,mdss-dsi-dispparam-skince-cabcuion-command-state",
	"qcom,mdss-dsi-dispparam-skince-cabcstillon-command-state",
	"qcom,mdss-dsi-dispparam-skince-cabcmovieon-command-state",
	"qcom,mdss-dsi-dispparam-skince-cabcoff-command-state",
	"qcom,mdss-dsi-dispparam-dimmingon-command-state",
	"qcom,mdss-dsi-dispparam-dimmingoff-command-state",
	"qcom,mdss-dsi-dispparam-acl-off-command-state",
	"qcom,mdss-dsi-dispparam-acl-l1-command-state",
	"qcom,mdss-dsi-dispparam-acl-l2-command-state",
	"qcom,mdss-dsi-dispparam-acl-l3-command-state",
	"qcom,mdss-dsi-dispparam-hbm-on-command-state",
	"qcom,mdss-dsi-dispparam-hbm-off-command-state",
	"qcom,mdss-dsi-dispparam-hbm-fod-on-command-state",
	"qcom,mdss-dsi-dispparam-hbm-fod-off-command-state",
	"qcom,mdss-dsi-dispparam-hbm-fod2norm-command-state",
	"qcom,mdss-dsi-displayoff-command-state",
	"qcom,mdss-dsi-displayon-command-state",
	"qcom,mdss-dsi-dispparam-xy-coordinate-command-state",
	"qcom,mdss-dsi-read-brightness-command-state",
	"qcom,mdss-dsi-dispparam-max-luminance-command-state",
	"qcom,mdss-dsi-dispparam-max-luminance-valid-command-state",
	"qcom,mdss-dsi-dispparam-crc-srgb-on-command-state",
	"qcom,mdss-dsi-dispparam-crc-dcip3-on-command-state",
	"qcom,mdss-dsi-dispparam-crc-off-command-state",
	"qcom,mdss-dsi-read-panel-id-command-state",
	"qcom,mdss-dsi-dispparam-flash-test-on-command-state",
	"qcom,mdss-dsi-dispparam-flash-test-off-command-state",
};

static int dsi_panel_get_cmd_pkt_count(const char *data, u32 length, u32 *cnt)
{
	const u32 cmd_set_min_size = 7;
	u32 count = 0;
	u32 packet_length;
	u32 tmp;

	while (length >= cmd_set_min_size) {
		packet_length = cmd_set_min_size;
		tmp = ((data[5] << 8) | (data[6]));
		packet_length += tmp;
		if (packet_length > length) {
			pr_err("format error\n");
			return -EINVAL;
		}
		length -= packet_length;
		data += packet_length;
		count++;
	};

	*cnt = count;
	return 0;
}

static int dsi_panel_create_cmd_packets(const char *data,
					u32 length,
					u32 count,
					struct dsi_cmd_desc *cmd)
{
	int rc = 0;
	int i, j;
	u8 *payload;

	for (i = 0; i < count; i++) {
		u32 size;

		cmd[i].msg.type = data[0];
		cmd[i].last_command = (data[1] == 1 ? true : false);
		cmd[i].msg.channel = data[2];
		cmd[i].msg.flags |= (data[3] == 1 ? MIPI_DSI_MSG_REQ_ACK : 0);
		cmd[i].msg.ctrl = 0;
		cmd[i].post_wait_ms = cmd[i].msg.wait_ms = data[4];
		cmd[i].msg.tx_len = ((data[5] << 8) | (data[6]));

		size = cmd[i].msg.tx_len * sizeof(u8);

		payload = kzalloc(size, GFP_KERNEL);
		if (!payload) {
			rc = -ENOMEM;
			goto error_free_payloads;
		}

		for (j = 0; j < cmd[i].msg.tx_len; j++)
			payload[j] = data[7 + j];

		cmd[i].msg.tx_buf = payload;
		data += (7 + cmd[i].msg.tx_len);
	}

	return rc;
error_free_payloads:
	for (i = i - 1; i >= 0; i--) {
		kfree(cmd[i].msg.tx_buf);
		cmd[i].msg.tx_buf = NULL;
	}

	return rc;
}

static void dsi_panel_destroy_cmds_packets_buf(struct dsi_panel_cmd_set *set)
{
	u32 i = 0;
	struct dsi_cmd_desc *cmd;

	for (i = 0; i < set->count; i++) {
		cmd = &set->cmds[i];
		kfree(cmd->msg.tx_buf);
		cmd->msg.tx_buf = NULL;
	}
}

static void dsi_panel_destroy_cmd_packets(struct dsi_panel_cmd_set *set)
{
	dsi_panel_destroy_cmds_packets_buf(set);
	kfree(set->cmds);
	set->count = 0;
}

static int dsi_panel_alloc_cmd_packets(struct dsi_panel_cmd_set *cmd,
					u32 packet_count)
{
	u32 size;

	size = packet_count * sizeof(*cmd->cmds);
	cmd->cmds = kzalloc(size, GFP_KERNEL);
	if (!cmd->cmds)
		return -ENOMEM;

	cmd->count = packet_count;
	return 0;
}

static int dsi_panel_parse_cmd_sets_sub(struct dsi_panel_cmd_set *cmd,
					enum dsi_cmd_set_type type,
					struct device_node *of_node)
{
	int rc = 0;
	u32 length = 0;
	const char *data;
	const char *state;
	u32 packet_count = 0;

	data = of_get_property(of_node, cmd_set_prop_map[type], &length);
	if (!data) {
		pr_debug("%s commands not defined\n", cmd_set_prop_map[type]);
		rc = -ENOTSUPP;
		goto error;
	}

	rc = dsi_panel_get_cmd_pkt_count(data, length, &packet_count);
	if (rc) {
		pr_err("commands failed, rc=%d\n", rc);
		goto error;
	}
	pr_debug("[%s] packet-count=%d, %d\n", cmd_set_prop_map[type],
		packet_count, length);

	rc = dsi_panel_alloc_cmd_packets(cmd, packet_count);
	if (rc) {
		pr_err("failed to allocate cmd packets, rc=%d\n", rc);
		goto error;
	}

	rc = dsi_panel_create_cmd_packets(data, length, packet_count,
					  cmd->cmds);
	if (rc) {
		pr_err("failed to create cmd packets, rc=%d\n", rc);
		goto error_free_mem;
	}

	state = of_get_property(of_node, cmd_set_state_map[type], NULL);
	if (!state || !strcmp(state, "dsi_lp_mode")) {
		cmd->state = DSI_CMD_SET_STATE_LP;
	} else if (!strcmp(state, "dsi_hs_mode")) {
		cmd->state = DSI_CMD_SET_STATE_HS;
	} else {
		pr_err("[%s] command state unrecognized-%s\n",
		       cmd_set_state_map[type], state);
		goto error_free_mem;
	}

	return rc;
error_free_mem:
	kfree(cmd->cmds);
	cmd->cmds = NULL;
error:
	return rc;

}

static int dsi_panel_parse_cmd_sets(
		struct dsi_display_mode_priv_info *priv_info,
		struct device_node *of_node)
{
	int rc = 0;
	struct dsi_panel_cmd_set *set;
	u32 i;

	if (!priv_info) {
		pr_err("invalid mode priv info\n");
		return -EINVAL;
	}

	for (i = DSI_CMD_SET_PRE_ON; i < DSI_CMD_SET_MAX; i++) {
		set = &priv_info->cmd_sets[i];
		set->type = i;
		set->count = 0;

		if (i == DSI_CMD_SET_PPS) {
			rc = dsi_panel_alloc_cmd_packets(set, 1);
			if (rc)
				pr_err("failed to allocate cmd set %d, rc = %d\n",
					i, rc);
			set->state = DSI_CMD_SET_STATE_LP;
		} else {
			rc = dsi_panel_parse_cmd_sets_sub(set, i, of_node);
			if (rc)
				pr_debug("failed to parse set %d\n", i);
		}
	}

	rc = 0;
	return rc;
}

static int dsi_panel_parse_reset_sequence(struct dsi_panel *panel,
				      struct device_node *of_node)
{
	int rc = 0;
	int i;
	u32 length = 0;
	u32 count = 0;
	u32 size = 0;
	u32 *arr_32 = NULL;
	const u32 *arr;
	struct dsi_reset_seq *seq;

	arr = of_get_property(of_node, "qcom,mdss-dsi-reset-sequence", &length);
	if (!arr) {
		pr_err("[%s] dsi-reset-sequence not found\n", panel->name);
		rc = -EINVAL;
		goto error;
	}
	if (length & 0x1) {
		pr_err("[%s] syntax error for dsi-reset-sequence\n",
		       panel->name);
		rc = -EINVAL;
		goto error;
	}

	pr_err("RESET SEQ LENGTH = %d\n", length);
	length = length / sizeof(u32);

	size = length * sizeof(u32);

	arr_32 = kzalloc(size, GFP_KERNEL);
	if (!arr_32) {
		rc = -ENOMEM;
		goto error;
	}

	rc = of_property_read_u32_array(of_node, "qcom,mdss-dsi-reset-sequence",
					arr_32, length);
	if (rc) {
		pr_err("[%s] cannot read dso-reset-seqience\n", panel->name);
		goto error_free_arr_32;
	}

	count = length / 2;
	size = count * sizeof(*seq);
	seq = kzalloc(size, GFP_KERNEL);
	if (!seq) {
		rc = -ENOMEM;
		goto error_free_arr_32;
	}

	panel->reset_config.sequence = seq;
	panel->reset_config.count = count;

	for (i = 0; i < length; i += 2) {
		seq->level = arr_32[i];
		seq->sleep_ms = arr_32[i + 1];
		seq++;
	}


error_free_arr_32:
	kfree(arr_32);
error:
	return rc;
}

static int dsi_panel_parse_misc_features(struct dsi_panel *panel,
				     struct device_node *of_node)
{
	panel->ulps_enabled =
		of_property_read_bool(of_node, "qcom,ulps-enabled");

	pr_info("%s: ulps feature %s\n", __func__,
		(panel->ulps_enabled ? "enabled" : "disabled"));

	panel->ulps_suspend_enabled =
		of_property_read_bool(of_node, "qcom,suspend-ulps-enabled");

	pr_info("%s: ulps during suspend feature %s", __func__,
		(panel->ulps_suspend_enabled ? "enabled" : "disabled"));

	panel->te_using_watchdog_timer = of_property_read_bool(of_node,
					"qcom,mdss-dsi-te-using-wd");

	panel->sync_broadcast_en = of_property_read_bool(of_node,
			"qcom,cmd-sync-wait-broadcast");

	panel->lp11_init = of_property_read_bool(of_node,
			"qcom,mdss-dsi-lp11-init");
	return 0;
}

static int dsi_panel_parse_jitter_config(
				struct dsi_display_mode *mode,
				struct device_node *of_node)
{
	int rc;
	struct dsi_display_mode_priv_info *priv_info;
	u32 jitter[DEFAULT_PANEL_JITTER_ARRAY_SIZE] = {0, 0};
	u64 jitter_val = 0;

	priv_info = mode->priv_info;

	rc = of_property_read_u32_array(of_node, "qcom,mdss-dsi-panel-jitter",
				jitter, DEFAULT_PANEL_JITTER_ARRAY_SIZE);
	if (rc) {
		pr_debug("panel jitter not defined rc=%d\n", rc);
	} else {
		jitter_val = jitter[0];
		jitter_val = div_u64(jitter_val, jitter[1]);
	}

	if (rc || !jitter_val || (jitter_val > MAX_PANEL_JITTER)) {
		priv_info->panel_jitter_numer = DEFAULT_PANEL_JITTER_NUMERATOR;
		priv_info->panel_jitter_denom =
					DEFAULT_PANEL_JITTER_DENOMINATOR;
	} else {
		priv_info->panel_jitter_numer = jitter[0];
		priv_info->panel_jitter_denom = jitter[1];
	}

	rc = of_property_read_u32(of_node, "qcom,mdss-dsi-panel-prefill-lines",
				  &priv_info->panel_prefill_lines);
	if (rc) {
		pr_debug("panel prefill lines are not defined rc=%d\n", rc);
		priv_info->panel_prefill_lines = DEFAULT_PANEL_PREFILL_LINES;
	} else if (priv_info->panel_prefill_lines >=
					DSI_V_TOTAL(&mode->timing)) {
		pr_debug("invalid prefill lines config=%d setting to:%d\n",
		priv_info->panel_prefill_lines, DEFAULT_PANEL_PREFILL_LINES);

		priv_info->panel_prefill_lines = DEFAULT_PANEL_PREFILL_LINES;
	}

	return 0;
}

static int dsi_panel_parse_power_cfg(struct device *parent,
				     struct dsi_panel *panel,
				     struct device_node *of_node)
{
	int rc = 0;

	panel->lp11_init = of_property_read_bool(of_node,
				"qcom,mdss-dsi-lp11-init");
	pr_debug("%s: lp11_init = %d\n", __func__, panel->lp11_init);

	rc = dsi_pwr_of_get_vreg_data(of_node,
					  &panel->power_info,
					  "qcom,panel-supply-entries");
	if (rc) {
		pr_err("[%s] failed to parse vregs\n", panel->name);
		goto error;
	}

error:
	return rc;
}

static int dsi_panel_parse_gpios(struct dsi_panel *panel,
				 struct device_node *of_node)
{
	int rc = 0;
	const char *data;

	panel->reset_config.reset_gpio = of_get_named_gpio(of_node,
					      "qcom,platform-reset-gpio",
					      0);
	if (!gpio_is_valid(panel->reset_config.reset_gpio)) {
		pr_err("[%s] failed get reset gpio, rc=%d\n", panel->name, rc);
		rc = -EINVAL;
		goto error;
	}

	panel->reset_config.disp_en_gpio = of_get_named_gpio(of_node,
						"qcom,5v-boost-gpio",
						0);
	if (!gpio_is_valid(panel->reset_config.disp_en_gpio)) {
		pr_debug("[%s] 5v-boot-gpio is not set, rc=%d\n",
			 panel->name, rc);
		panel->reset_config.disp_en_gpio = of_get_named_gpio(of_node,
							"qcom,platform-en-gpio",
							0);
		if (!gpio_is_valid(panel->reset_config.disp_en_gpio)) {
			pr_debug("[%s] platform-en-gpio is not set, rc=%d\n",
				 panel->name, rc);
		}
	}

	panel->reset_config.lcd_mode_sel_gpio = of_get_named_gpio(of_node,
		"qcom,panel-mode-gpio", 0);
	if (!gpio_is_valid(panel->reset_config.lcd_mode_sel_gpio))
		pr_debug("%s:%d mode gpio not specified\n", __func__, __LINE__);

	data = of_get_property(of_node,
		"qcom,mdss-dsi-mode-sel-gpio-state", NULL);
	if (data) {
		if (!strcmp(data, "single_port"))
			panel->reset_config.mode_sel_state =
				MODE_SEL_SINGLE_PORT;
		else if (!strcmp(data, "dual_port"))
			panel->reset_config.mode_sel_state =
				MODE_SEL_DUAL_PORT;
		else if (!strcmp(data, "high"))
			panel->reset_config.mode_sel_state =
				MODE_GPIO_HIGH;
		else if (!strcmp(data, "low"))
			panel->reset_config.mode_sel_state =
				MODE_GPIO_LOW;
	} else {
		/* Set default mode as SPLIT mode */
		panel->reset_config.mode_sel_state = MODE_SEL_DUAL_PORT;
	}

	/* TODO:  release memory */
	rc = dsi_panel_parse_reset_sequence(panel, of_node);
	if (rc) {
		pr_err("[%s] failed to parse reset sequence, rc=%d\n",
		       panel->name, rc);
		goto error;
	}

error:
	return rc;
}

static int dsi_panel_parse_bl_pwm_config(struct dsi_backlight_config *config,
					 struct device_node *of_node)
{
	int rc = 0;
	u32 val;

	rc = of_property_read_u32(of_node, "qcom,dsi-bl-pmic-bank-select",
				  &val);
	if (rc) {
		pr_err("bl-pmic-bank-select is not defined, rc=%d\n", rc);
		goto error;
	}
	config->pwm_pmic_bank = val;

	rc = of_property_read_u32(of_node, "qcom,dsi-bl-pmic-pwm-frequency",
				  &val);
	if (rc) {
		pr_err("bl-pmic-bank-select is not defined, rc=%d\n", rc);
		goto error;
	}
	config->pwm_period_usecs = val;

	config->pwm_pmi_control = of_property_read_bool(of_node,
						"qcom,mdss-dsi-bl-pwm-pmi");

	config->pwm_gpio = of_get_named_gpio(of_node,
					     "qcom,mdss-dsi-pwm-gpio",
					     0);
	if (!gpio_is_valid(config->pwm_gpio)) {
		pr_err("pwm gpio is invalid\n");
		rc = -EINVAL;
		goto error;
	}

error:
	return rc;
}

static int dsi_panel_parse_bl_config(struct dsi_panel *panel,
				     struct device_node *of_node)
{
	int rc = 0;
	const char *bl_type;
	const char *data;
	u32 val = 0;

	bl_type = of_get_property(of_node,
				  "qcom,mdss-dsi-bl-pmic-control-type",
				  NULL);
	if (!bl_type) {
		panel->bl_config.type = DSI_BACKLIGHT_UNKNOWN;
	} else if (!strcmp(bl_type, "bl_ctrl_pwm")) {
		panel->bl_config.type = DSI_BACKLIGHT_PWM;
	} else if (!strcmp(bl_type, "bl_ctrl_wled")) {
		panel->bl_config.type = DSI_BACKLIGHT_WLED;
	} else if (!strcmp(bl_type, "bl_ctrl_dcs")) {
		panel->bl_config.type = DSI_BACKLIGHT_DCS;
	} else {
		pr_debug("[%s] bl-pmic-control-type unknown-%s\n",
			 panel->name, bl_type);
		panel->bl_config.type = DSI_BACKLIGHT_UNKNOWN;
	}

	panel->bl_config.dcs_type_ss = of_property_read_bool(of_node,
						"qcom,mdss-dsi-bl-dcs-type-ss");

	data = of_get_property(of_node, "qcom,bl-update-flag", NULL);
	if (!data) {
		panel->bl_config.bl_update = BL_UPDATE_NONE;
	} else if (!strcmp(data, "delay_until_first_frame")) {
		panel->bl_config.bl_update = BL_UPDATE_DELAY_UNTIL_FIRST_FRAME;
	} else {
		pr_debug("[%s] No valid bl-update-flag: %s\n",
						panel->name, data);
		panel->bl_config.bl_update = BL_UPDATE_NONE;
	}

	rc = of_property_read_u32(of_node, "qcom,bl-update-delay", &val);
	if (rc) {
		pr_debug("[%s] bl-update-delay unspecified, defaulting to zero\n",
			 panel->name);
		panel->bl_config.bl_update_delay = 0;
	} else {
		panel->bl_config.bl_update_delay = val;
	}

	panel->bl_config.bl_scale = MAX_BL_SCALE_LEVEL;
	panel->bl_config.bl_scale_ad = MAX_AD_BL_SCALE_LEVEL;

	panel->bl_config.dcs_type_ss = of_property_read_bool(of_node,
						"qcom,mdss-dsi-bl-dcs-type-ss");

	rc = of_property_read_u32(of_node, "qcom,bl-update-delay", &val);
	if (rc) {
		pr_debug("[%s] bl-update-delay unspecified, defaulting to zero\n",
			 panel->name);
		panel->bl_config.bl_update_delay = 0;
	} else {
		panel->bl_config.bl_update_delay = val;
	}

	rc = of_property_read_u32(of_node, "qcom,mdss-dsi-bl-min-level", &val);
	if (rc) {
		pr_debug("[%s] bl-min-level unspecified, defaulting to zero\n",
			 panel->name);
		panel->bl_config.bl_min_level = 0;
	} else {
		panel->bl_config.bl_min_level = val;
	}

	rc = of_property_read_u32(of_node, "qcom,mdss-dsi-bl-max-level", &val);
	if (rc) {
		pr_debug("[%s] bl-max-level unspecified, defaulting to max level\n",
			 panel->name);
		panel->bl_config.bl_max_level = MAX_BL_LEVEL;
	} else {
		panel->bl_config.bl_max_level = val;
	}

	rc = of_property_read_u32(of_node, "qcom,mdss-dsi-bl-typical-level", &val);
	if (rc) {
		pr_debug("[%s] bl-typical-level unspecified, defaulting to bl-min-level\n",
			 panel->name);
		panel->bl_config.bl_typical_level = panel->bl_config.bl_min_level;
	} else {
		panel->bl_config.bl_typical_level = val;
	}

	panel->bl_config.bl_remap_flag = of_property_read_bool(of_node,
						"qcom,mdss-brightness-remap");

	panel->bl_config.doze_brightness_varible_flag = of_property_read_bool(of_node,
						"qcom,mdss-doze-brightness-variable");

	rc = of_property_read_u32(of_node, "qcom,mdss-brightness-max-level",
		&val);
	if (rc) {
		pr_debug("[%s] brigheness-max-level unspecified, defaulting to 255\n",
			 panel->name);
		panel->bl_config.brightness_max_level = 255;
	} else {
		panel->bl_config.brightness_max_level = val;
	}

	if (panel->bl_config.type == DSI_BACKLIGHT_PWM) {
		rc = dsi_panel_parse_bl_pwm_config(&panel->bl_config, of_node);
		if (rc) {
			pr_err("[%s] failed to parse pwm config, rc=%d\n",
			       panel->name, rc);
			goto error;
		}
	}

	panel->bl_config.en_gpio = of_get_named_gpio(of_node,
					      "qcom,platform-bklight-en-gpio",
					      0);
	if (!gpio_is_valid(panel->bl_config.en_gpio)) {
		pr_debug("[%s] failed get bklt gpio, rc=%d\n", panel->name, rc);
		rc = 0;
		goto error;
	}

error:
	return rc;
}

void dsi_dsc_pclk_param_calc(struct msm_display_dsc_info *dsc, int intf_width)
{
	int slice_per_pkt, slice_per_intf;
	int bytes_in_slice, total_bytes_per_intf;

	if (!dsc || !dsc->slice_width || !dsc->slice_per_pkt ||
	    (intf_width < dsc->slice_width)) {
		pr_err("invalid input, intf_width=%d slice_width=%d\n",
			intf_width, dsc ? dsc->slice_width : -1);
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
}


int dsi_dsc_populate_static_param(struct msm_display_dsc_info *dsc)
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
	int ratio_index, mod_offset;

	dsc->rc_model_size = 8192;

	if (dsc->version == 0x11 && dsc->scr_rev == 0x1)
		dsc->first_line_bpg_offset = 15;
	else
		dsc->first_line_bpg_offset = 12;

	dsc->edge_factor = 6;
	dsc->tgt_offset_hi = 3;
	dsc->tgt_offset_lo = 3;
	dsc->enable_422 = 0;
	dsc->convert_rgb = 1;
	dsc->vbr_enable = 0;

	dsc->buf_thresh = dsi_dsc_rc_buf_thresh;

	bpp = dsc->bpp;
	bpc = dsc->bpc;

	if (bpc == 12)
		ratio_index = DSC_12BPC_8BPP;
	else if (bpc == 10)
		ratio_index = DSC_10BPC_8BPP;
	else
		ratio_index = DSC_8BPC_8BPP;

	if (dsc->version == 0x11 && dsc->scr_rev == 0x1) {
		dsc->range_min_qp =
			dsi_dsc_rc_range_min_qp_1_1_scr1[ratio_index];
		dsc->range_max_qp =
			dsi_dsc_rc_range_max_qp_1_1_scr1[ratio_index];
	} else {
		dsc->range_min_qp = dsi_dsc_rc_range_min_qp_1_1[ratio_index];
		dsc->range_max_qp = dsi_dsc_rc_range_max_qp_1_1[ratio_index];
	}
	dsc->range_bpg_offset = dsi_dsc_rc_range_bpg_offset;

	if (bpp == 8)
		dsc->initial_offset = 6144;
	else
		dsc->initial_offset = 2048;	/* bpp = 12 */

	if (bpc == 12)
		mux_words_size = 64;
	else
		mux_words_size = 48;		/* bpc == 8/10 */

	if (bpc == 8) {
		dsc->line_buf_depth = 9;
		dsc->input_10_bits = 0;
		dsc->min_qp_flatness = 3;
		dsc->max_qp_flatness = 12;
		dsc->quant_incr_limit0 = 11;
		dsc->quant_incr_limit1 = 11;
	} else if (bpc == 10) { /* 10bpc */
		dsc->line_buf_depth = 11;
		dsc->input_10_bits = 1;
		dsc->min_qp_flatness = 7;
		dsc->max_qp_flatness = 16;
		dsc->quant_incr_limit0 = 15;
		dsc->quant_incr_limit1 = 15;
	} else { /* 12 bpc */
		dsc->line_buf_depth = 9;
		dsc->input_10_bits = 0;
		dsc->min_qp_flatness = 11;
		dsc->max_qp_flatness = 20;
		dsc->quant_incr_limit0 = 19;
		dsc->quant_incr_limit1 = 19;
	}

	mod_offset = dsc->slice_width % 3;
	switch (mod_offset) {
	case 0:
		dsc->slice_last_group_size = 2;
		break;
	case 1:
		dsc->slice_last_group_size = 0;
		break;
	case 2:
		dsc->slice_last_group_size = 1;
		break;
	default:
		break;
	}

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

	return 0;
}


static int dsi_panel_parse_phy_timing(struct dsi_display_mode *mode,
				struct device_node *of_node)
{
	const char *data;
	u32 len, i;
	int rc = 0;
	struct dsi_display_mode_priv_info *priv_info;

	priv_info = mode->priv_info;

	data = of_get_property(of_node,
			"qcom,mdss-dsi-panel-phy-timings", &len);
	if (!data) {
		pr_debug("Unable to read Phy timing settings");
	} else {
		priv_info->phy_timing_val =
			kzalloc((sizeof(u32) * len), GFP_KERNEL);
		if (!priv_info->phy_timing_val)
			return -EINVAL;

		for (i = 0; i < len; i++)
			priv_info->phy_timing_val[i] = data[i];

		priv_info->phy_timing_len = len;
	};

	mode->pixel_clk_khz = (mode->timing.h_active *
			DSI_V_TOTAL(&mode->timing) *
			mode->timing.refresh_rate) / 1000;
	return rc;
}

static int dsi_panel_parse_dsc_params(struct dsi_display_mode *mode,
				struct device_node *of_node)
{
	u32 data;
	int rc = -EINVAL;
	int intf_width;
	const char *compression;
	struct dsi_display_mode_priv_info *priv_info;

	if (!mode || !mode->priv_info)
		return -EINVAL;

	priv_info = mode->priv_info;

	priv_info->dsc_enabled = false;
	compression = of_get_property(of_node, "qcom,compression-mode", NULL);
	if (compression && !strcmp(compression, "dsc"))
		priv_info->dsc_enabled = true;

	if (!priv_info->dsc_enabled) {
		pr_debug("dsc compression is not enabled for the mode");
		return 0;
	}

	rc = of_property_read_u32(of_node, "qcom,mdss-dsc-version", &data);
	if (rc) {
		priv_info->dsc.version = 0x11;
		rc = 0;
	} else {
		priv_info->dsc.version = data & 0xff;
		/* only support DSC 1.1 rev */
		if (priv_info->dsc.version != 0x11) {
			pr_err("%s: DSC version:%d not supported\n", __func__,
					priv_info->dsc.version);
			rc = -EINVAL;
			goto error;
		}
	}

	rc = of_property_read_u32(of_node, "qcom,mdss-dsc-scr-version", &data);
	if (rc) {
		priv_info->dsc.scr_rev = 0x0;
		rc = 0;
	} else {
		priv_info->dsc.scr_rev = data & 0xff;
		/* only one scr rev supported */
		if (priv_info->dsc.scr_rev > 0x1) {
			pr_err("%s: DSC scr version:%d not supported\n",
					__func__, priv_info->dsc.scr_rev);
			rc = -EINVAL;
			goto error;
		}
	}

	rc = of_property_read_u32(of_node, "qcom,mdss-dsc-slice-height", &data);
	if (rc) {
		pr_err("failed to parse qcom,mdss-dsc-slice-height\n");
		goto error;
	}
	priv_info->dsc.slice_height = data;

	rc = of_property_read_u32(of_node, "qcom,mdss-dsc-slice-width", &data);
	if (rc) {
		pr_err("failed to parse qcom,mdss-dsc-slice-width\n");
		goto error;
	}
	priv_info->dsc.slice_width = data;

	intf_width = mode->timing.h_active;
	if (intf_width % priv_info->dsc.slice_width) {
		pr_err("invalid slice width for the intf width:%d slice width:%d\n",
			intf_width, priv_info->dsc.slice_width);
		rc = -EINVAL;
		goto error;
	}

	priv_info->dsc.pic_width = mode->timing.h_active;
	priv_info->dsc.pic_height = mode->timing.v_active;

	rc = of_property_read_u32(of_node, "qcom,mdss-dsc-slice-per-pkt",
			&data);
	if (rc) {
		pr_err("failed to parse qcom,mdss-dsc-slice-per-pkt\n");
		goto error;
	}
	priv_info->dsc.slice_per_pkt = data;

	rc = of_property_read_u32(of_node, "qcom,mdss-dsc-bit-per-component",
		&data);
	if (rc) {
		pr_err("failed to parse qcom,mdss-dsc-bit-per-component\n");
		goto error;
	}
	priv_info->dsc.bpc = data;

	rc = of_property_read_u32(of_node, "qcom,mdss-dsc-bit-per-pixel",
			&data);
	if (rc) {
		pr_err("failed to parse qcom,mdss-dsc-bit-per-pixel\n");
		goto error;
	}
	priv_info->dsc.bpp = data;

	priv_info->dsc.block_pred_enable = of_property_read_bool(of_node,
		"qcom,mdss-dsc-block-prediction-enable");

	priv_info->dsc.full_frame_slices = DIV_ROUND_UP(intf_width,
		priv_info->dsc.slice_width);

	dsi_dsc_populate_static_param(&priv_info->dsc);
	dsi_dsc_pclk_param_calc(&priv_info->dsc, intf_width);

error:
	return rc;
}

static int dsi_panel_parse_hdr_config(struct dsi_panel *panel,
				     struct device_node *of_node)
{

	int rc = 0;
	struct drm_panel_hdr_properties *hdr_prop;

	hdr_prop = &panel->hdr_props;
	hdr_prop->hdr_enabled = of_property_read_bool(of_node,
		"qcom,mdss-dsi-panel-hdr-enabled");

	if (hdr_prop->hdr_enabled) {
		rc = of_property_read_u32_array(of_node,
				"qcom,mdss-dsi-panel-hdr-color-primaries",
				hdr_prop->display_primaries,
				DISPLAY_PRIMARIES_MAX);
		if (rc) {
			pr_err("%s:%d, Unable to read color primaries,rc:%u",
					__func__, __LINE__, rc);
			hdr_prop->hdr_enabled = false;
			return rc;
		}

		rc = of_property_read_u32(of_node,
			"qcom,mdss-dsi-panel-peak-brightness",
			&(hdr_prop->peak_brightness));
		if (rc) {
			pr_err("%s:%d, Unable to read hdr brightness, rc:%u",
				__func__, __LINE__, rc);
			hdr_prop->hdr_enabled = false;
			return rc;
		}

		rc = of_property_read_u32(of_node,
			"qcom,mdss-dsi-panel-blackness-level",
			&(hdr_prop->blackness_level));
		if (rc) {
			pr_err("%s:%d, Unable to read hdr brightness, rc:%u",
				__func__, __LINE__, rc);
			hdr_prop->hdr_enabled = false;
			return rc;
		}
	}
	return 0;
}

static int dsi_panel_parse_topology(
		struct dsi_display_mode_priv_info *priv_info,
		struct device_node *of_node, int topology_override)
{
	struct msm_display_topology *topology;
	u32 top_count, top_sel, *array = NULL;
	int i, len = 0;
	int rc = -EINVAL;

	len = of_property_count_u32_elems(of_node, "qcom,display-topology");
	if (len <= 0 || len % TOPOLOGY_SET_LEN ||
			len > (TOPOLOGY_SET_LEN * MAX_TOPOLOGY)) {
		pr_err("invalid topology list for the panel, rc = %d\n", rc);
		return rc;
	}

	top_count = len / TOPOLOGY_SET_LEN;

	array = kcalloc(len, sizeof(u32), GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node,
			"qcom,display-topology", array, len);
	if (rc) {
		pr_err("unable to read the display topologies, rc = %d\n", rc);
		goto read_fail;
	}

	topology = kcalloc(top_count, sizeof(*topology), GFP_KERNEL);
	if (!topology) {
		rc = -ENOMEM;
		goto read_fail;
	}

	for (i = 0; i < top_count; i++) {
		struct msm_display_topology *top = &topology[i];

		top->num_lm = array[i * TOPOLOGY_SET_LEN];
		top->num_enc = array[i * TOPOLOGY_SET_LEN + 1];
		top->num_intf = array[i * TOPOLOGY_SET_LEN + 2];
	};

	if (topology_override >= 0 && topology_override < top_count) {
		pr_info("override topology: cfg:%d lm:%d comp_enc:%d intf:%d\n",
			topology_override,
			topology[topology_override].num_lm,
			topology[topology_override].num_enc,
			topology[topology_override].num_intf);
		top_sel = topology_override;
		goto parse_done;
	}

	rc = of_property_read_u32(of_node,
			"qcom,default-topology-index", &top_sel);
	if (rc) {
		pr_err("no default topology selected, rc = %d\n", rc);
		goto parse_fail;
	}

	if (top_sel >= top_count) {
		rc = -EINVAL;
		pr_err("default topology is specified is not valid, rc = %d\n",
			rc);
		goto parse_fail;
	}

	pr_info("default topology: lm: %d comp_enc:%d intf: %d\n",
		topology[top_sel].num_lm,
		topology[top_sel].num_enc,
		topology[top_sel].num_intf);

parse_done:
	memcpy(&priv_info->topology, &topology[top_sel],
		sizeof(struct msm_display_topology));
parse_fail:
	kfree(topology);
read_fail:
	kfree(array);

	return rc;
}

static int dsi_panel_parse_roi_alignment(struct device_node *of_node,
					 struct msm_roi_alignment *align)
{
	int len = 0, rc = 0;
	u32 value[6];
	struct property *data;

	if (!align || !of_node)
		return -EINVAL;

	memset(align, 0, sizeof(*align));

	data = of_find_property(of_node, "qcom,panel-roi-alignment", &len);
	len /= sizeof(u32);
	if (!data) {
		pr_err("panel roi alignment not found\n");
		rc = -EINVAL;
	} else if (len != 6) {
		pr_err("incorrect roi alignment len %d\n", len);
		rc = -EINVAL;
	} else {
		rc = of_property_read_u32_array(of_node,
				"qcom,panel-roi-alignment", value, len);
		if (rc)
			pr_debug("error reading panel roi alignment values\n");
		else {
			align->xstart_pix_align = value[0];
			align->ystart_pix_align = value[1];
			align->width_pix_align = value[2];
			align->height_pix_align = value[3];
			align->min_width = value[4];
			align->min_height = value[5];
		}

		pr_info("roi alignment: [%d, %d, %d, %d, %d, %d]\n",
			align->xstart_pix_align,
			align->width_pix_align,
			align->ystart_pix_align,
			align->height_pix_align,
			align->min_width,
			align->min_height);
	}

	return rc;
}

static int dsi_panel_parse_partial_update_caps(struct dsi_display_mode *mode,
				struct device_node *of_node)
{
	struct msm_roi_caps *roi_caps = NULL;
	const char *data;
	int rc = 0;

	if (!mode || !mode->priv_info) {
		pr_err("invalid arguments\n");
		return -EINVAL;
	}

	roi_caps = &mode->priv_info->roi_caps;

	memset(roi_caps, 0, sizeof(*roi_caps));

	data = of_get_property(of_node, "qcom,partial-update-enabled", NULL);
	if (data) {
		if (!strcmp(data, "dual_roi"))
			roi_caps->num_roi = 2;
		else if (!strcmp(data, "single_roi"))
			roi_caps->num_roi = 1;
		else {
			pr_info(
			"invalid value for qcom,partial-update-enabled: %s\n",
			data);
			return 0;
		}
	} else {
		pr_info("partial update disabled as the property is not set\n");
		return 0;
	}

	roi_caps->merge_rois = of_property_read_bool(of_node,
			"qcom,partial-update-roi-merge");

	roi_caps->enabled = roi_caps->num_roi > 0;

	pr_info("partial update num_rois=%d enabled=%d\n", roi_caps->num_roi,
			roi_caps->enabled);

	if (roi_caps->enabled)
		rc = dsi_panel_parse_roi_alignment(of_node,
				&roi_caps->align);

	if (rc)
		memset(roi_caps, 0, sizeof(*roi_caps));

	return rc;
}

static int dsi_panel_parse_dms_info(struct dsi_panel *panel,
	struct device_node *of_node)
{
	int dms_enabled;
	const char *data;

	if (!of_node || !panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	panel->dms_mode = DSI_DMS_MODE_DISABLED;
	dms_enabled = of_property_read_bool(of_node,
		"qcom,dynamic-mode-switch-enabled");
	if (!dms_enabled)
		return 0;

	data = of_get_property(of_node, "qcom,dynamic-mode-switch-type", NULL);
	if (data && !strcmp(data, "dynamic-resolution-switch-immediate")) {
		panel->dms_mode = DSI_DMS_MODE_RES_SWITCH_IMMEDIATE;
	} else {
		pr_err("[%s] unsupported dynamic switch mode: %s\n",
							panel->name, data);
		return -EINVAL;
	}

	return 0;
};

/*
 * The length of all the valid values to be checked should not be greater
 * than the length of returned data from read command.
 */
static bool
dsi_panel_parse_esd_check_valid_params(struct dsi_panel *panel, u32 count)
{
	int i;
	struct drm_panel_esd_config *config = &panel->esd_config;

	for (i = 0; i < count; ++i) {
		if (config->status_valid_params[i] >
				config->status_cmds_rlen[i]) {
			pr_debug("ignore valid params\n");
			return false;
		}
	}

	return true;
}

static bool dsi_panel_parse_esd_status_len(struct device_node *np,
	char *prop_key, u32 **target, u32 cmd_cnt)
{
	int tmp;

	if (!of_find_property(np, prop_key, &tmp))
		return false;

	tmp /= sizeof(u32);
	if (tmp != cmd_cnt) {
		pr_err("request property(%d) do not match cmd count(%d)\n",
				tmp, cmd_cnt);
		return false;
	}

	*target = kcalloc(tmp, sizeof(u32), GFP_KERNEL);
	if (IS_ERR_OR_NULL(*target)) {
		pr_err("Error allocating memory for property\n");
		return false;
	}

	if (of_property_read_u32_array(np, prop_key, *target, tmp)) {
		pr_err("cannot get values from dts\n");
		kfree(*target);
		*target = NULL;
		return false;
	}

	return true;
}

static void dsi_panel_esd_config_deinit(struct drm_panel_esd_config *esd_config)
{
	kfree(esd_config->status_buf);
	kfree(esd_config->return_buf);
	kfree(esd_config->status_value);
	kfree(esd_config->status_valid_params);
	kfree(esd_config->status_cmds_rlen);
	kfree(esd_config->status_cmd.cmds);
}

int dsi_panel_parse_esd_reg_read_configs(struct dsi_panel *panel,
				struct device_node *of_node)
{
	struct drm_panel_esd_config *esd_config;
	int rc = 0;
	u32 tmp;
	u32 i, status_len, *lenp;
	struct property *data;

	if (!panel || !of_node) {
		pr_err("Invalid Params\n");
		return -EINVAL;
	}

	esd_config = &panel->esd_config;
	if (!esd_config)
		return -EINVAL;

	dsi_panel_parse_cmd_sets_sub(&esd_config->status_cmd,
				DSI_CMD_SET_PANEL_STATUS, of_node);
	if (!esd_config->status_cmd.count) {
		pr_err("panel status command parsing failed\n");
		rc = -EINVAL;
		goto error;
	}

	if (!dsi_panel_parse_esd_status_len(of_node,
		"qcom,mdss-dsi-panel-status-read-length",
			&panel->esd_config.status_cmds_rlen,
				esd_config->status_cmd.count)) {
		pr_err("Invalid status read length\n");
		rc = -EINVAL;
		goto error1;
	}

	if (dsi_panel_parse_esd_status_len(of_node,
		"qcom,mdss-dsi-panel-status-valid-params",
			&panel->esd_config.status_valid_params,
				esd_config->status_cmd.count)) {
		if (!dsi_panel_parse_esd_check_valid_params(panel,
					esd_config->status_cmd.count)) {
			rc = -EINVAL;
			goto error2;
		}
	}

	status_len = 0;
	lenp = esd_config->status_valid_params ?: esd_config->status_cmds_rlen;
	for (i = 0; i < esd_config->status_cmd.count; ++i)
		status_len += lenp[i];

	if (!status_len) {
		rc = -EINVAL;
		goto error2;
	}

	/*
	 * Some panel may need multiple read commands to properly
	 * check panel status. Do a sanity check for proper status
	 * value which will be compared with the value read by dsi
	 * controller during ESD check. Also check if multiple read
	 * commands are there then, there should be corresponding
	 * status check values for each read command.
	 */
	data = of_find_property(of_node,
			"qcom,mdss-dsi-panel-status-value", &tmp);
	tmp /= sizeof(u32);
	if (!IS_ERR_OR_NULL(data) && tmp != 0 && (tmp % status_len) == 0) {
		esd_config->groups = tmp / status_len;
	} else {
		pr_err("error parse panel-status-value\n");
		rc = -EINVAL;
		goto error2;
	}

	esd_config->status_value =
		kzalloc(sizeof(u32) * status_len * esd_config->groups,
			GFP_KERNEL);
	if (!esd_config->status_value) {
		rc = -ENOMEM;
		goto error2;
	}

	esd_config->return_buf = kcalloc(status_len * esd_config->groups,
			sizeof(unsigned char), GFP_KERNEL);
	if (!esd_config->return_buf) {
		rc = -ENOMEM;
		goto error3;
	}

	esd_config->status_buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (!esd_config->status_buf) {
		rc = -ENOMEM;
		goto error4;
	}

	rc = of_property_read_u32_array(of_node,
		"qcom,mdss-dsi-panel-status-value",
		esd_config->status_value, esd_config->groups * status_len);
	if (rc) {
		pr_debug("error reading panel status values\n");
		memset(esd_config->status_value, 0,
				esd_config->groups * status_len);
	}

	esd_config->cmd_channel = of_property_read_bool(of_node,
		"qcom,mdss-dsi-panel-cmds-only-by-right");

	return 0;

error4:
	kfree(esd_config->return_buf);
error3:
	kfree(esd_config->status_value);
error2:
	kfree(esd_config->status_valid_params);
	kfree(esd_config->status_cmds_rlen);
error1:
	kfree(esd_config->status_cmd.cmds);
error:
	return rc;
}

static int dsi_panel_parse_esd_config(struct dsi_panel *panel,
				     struct device_node *of_node)
{
	int rc = 0;
	const char *string;
	struct drm_panel_esd_config *esd_config;
	u8 *esd_mode = NULL;

	esd_config = &panel->esd_config;
	esd_config->status_mode = ESD_MODE_MAX;

	/* esd-err-flag method will be prefered */
	esd_config->esd_err_irq_gpio = of_get_named_gpio_flags(
			of_node,
			"qcom,esd-err-irq-gpio",
			0,
			(enum of_gpio_flags *)&(esd_config->esd_err_irq_flags));
	if (gpio_is_valid(esd_config->esd_err_irq_gpio)) {
		esd_config->esd_err_irq = gpio_to_irq(esd_config->esd_err_irq_gpio);
		rc = gpio_request(esd_config->esd_err_irq_gpio, "esd_err_irq_gpio");
		if (rc)
			pr_err("%s: Failed to get esd irq gpio %d (code: %d)",
				__func__, esd_config->esd_err_irq_gpio, rc);
		else
			gpio_direction_input(esd_config->esd_err_irq_gpio);

		return 0;
	}

	esd_config->esd_enabled = of_property_read_bool(of_node,
		"qcom,esd-check-enabled");

	if (!esd_config->esd_enabled)
		return 0;

	rc = of_property_read_string(of_node,
			"qcom,mdss-dsi-panel-status-check-mode", &string);
	if (!rc) {
		if (!strcmp(string, "bta_check")) {
			esd_config->status_mode = ESD_MODE_SW_BTA;
		} else if (!strcmp(string, "reg_read")) {
			esd_config->status_mode = ESD_MODE_REG_READ;
		} else if (!strcmp(string, "te_signal_check")) {
			if (panel->panel_mode == DSI_OP_CMD_MODE) {
				esd_config->status_mode = ESD_MODE_PANEL_TE;
			} else {
				pr_err("TE-ESD not valid for video mode\n");
				rc = -EINVAL;
				goto error;
			}
		} else {
			pr_err("No valid panel-status-check-mode string\n");
			rc = -EINVAL;
			goto error;
		}
	} else {
		pr_debug("status check method not defined!\n");
		rc = -EINVAL;
		goto error;
	}

	if (panel->esd_config.status_mode == ESD_MODE_REG_READ) {
		rc = dsi_panel_parse_esd_reg_read_configs(panel, of_node);
		if (rc) {
			pr_err("failed to parse esd reg read mode params, rc=%d\n",
						rc);
			goto error;
		}
		esd_mode = "register_read";
	} else if (panel->esd_config.status_mode == ESD_MODE_SW_BTA) {
		esd_mode = "bta_trigger";
	} else if (panel->esd_config.status_mode ==  ESD_MODE_PANEL_TE) {
		esd_mode = "te_check";
	}

	pr_info("ESD enabled with mode: %s\n", esd_mode);

	return 0;

error:
	panel->esd_config.esd_enabled = false;
	return rc;
}

#define PANEL_DIMMING_ON_CMD 0xF00
static void panelon_dimming_enable_delayed_work(struct work_struct *work)
{
	struct dsi_panel *panel = container_of(work,
				struct dsi_panel, cmds_work.work);
	struct dsi_display *display = NULL;
	struct mipi_dsi_host *host = panel->host;

	if (host)
		display = container_of(host, struct dsi_display, host);

	if (display) {
		mutex_lock(&display->display_lock);
		panel_disp_param_send_lock(panel, PANEL_DIMMING_ON_CMD);
		mutex_unlock(&display->display_lock);
	}
}

#define XY_COORDINATE_NUM    2
#define MAX_LUMINANCE_NUM    2
#define MAX_LUMINANCE_VALID_NUM    2
static int dsi_panel_parse_mi_config(struct dsi_panel *panel,
				     struct device_node *of_node)
{
	bool dispparam_enabled = 0;
	int rc = 0;
	int i;
	u32 xy_coordinate[XY_COORDINATE_NUM] = {0};
	u32 max_luminance[MAX_LUMINANCE_NUM] = {0};
	u32 max_luminance_valid[MAX_LUMINANCE_VALID_NUM] = {0};
	u32 panel_bl_info[PANEL_BL_INFO_NUM] = {0};  /* Bl table min value, typical bl, min bl, max bl(nit) */

	if (panel == NULL)
		return -EINVAL;

	dispparam_enabled = of_property_read_bool(of_node,
							"qcom,dispparam-enabled");
	if (dispparam_enabled){
		pr_info("[LCD]%s:%d Dispparam enabled.\n", __func__, __LINE__);
		panel->dispparam_enabled = true;
	} else {
		pr_info("[LCD]%s:%d Dispparam disabled.\n", __func__, __LINE__);
		panel->dispparam_enabled = false;
	}

	rc = of_property_read_u32_array(of_node,
		"qcom,mdss-dsi-panel-xy-coordinate",
		xy_coordinate, XY_COORDINATE_NUM);

	if (rc) {
		pr_info("%s:%d, Unable to read panel xy coordinate\n",
		       __func__, __LINE__);
		panel->xy_coordinate_cmds.enabled = false;
	} else {
		panel->xy_coordinate_cmds.cmds_rlen = xy_coordinate[0];
		panel->xy_coordinate_cmds.valid_bits = xy_coordinate[1];
		panel->xy_coordinate_cmds.enabled = true;
	}
	pr_info("0x%x 0x%x enabled:%d\n",
		xy_coordinate[0], xy_coordinate[1], panel->xy_coordinate_cmds.enabled);

	rc = of_property_read_u32_array(of_node,
		"qcom,mdss-dsi-panel-max-luminance",
		max_luminance, MAX_LUMINANCE_NUM);

	if (rc) {
		pr_info("%s:%d, Unable to read panel max luminance\n",
		       __func__, __LINE__);
		panel->max_luminance_cmds.enabled = false;
	} else {
		panel->max_luminance_cmds.cmds_rlen = max_luminance[0];
		panel->max_luminance_cmds.valid_bits = max_luminance[1];
		panel->max_luminance_cmds.enabled = true;
	}
	pr_info("0x%x 0x%x enabled:%d\n",
		max_luminance[0], max_luminance[1], panel->max_luminance_cmds.enabled);

	rc = of_property_read_u32_array(of_node,
			"qcom,mdss-dsi-panel-max-luminance-valid",
			max_luminance_valid, MAX_LUMINANCE_VALID_NUM);

	if (rc) {
		pr_info("%s:%d, Unable to read panel max luminance valid\n",
		       __func__, __LINE__);
		panel->max_luminance_valid_cmds.enabled = false;
	} else {
		panel->max_luminance_valid_cmds.cmds_rlen = max_luminance_valid[0];
		panel->max_luminance_valid_cmds.valid_bits = max_luminance_valid[1];
		panel->max_luminance_valid_cmds.enabled = true;
	}
	rc = of_property_read_u32_array(of_node,
		"qcom,mdss-dsi-panel-bl-info",
		panel_bl_info, PANEL_BL_INFO_NUM);
	if (rc) {
		pr_info("%s:%d, Unable to read panel bl info\n",
			   __func__, __LINE__);
		panel->panel_bl_info[0] = 0;
	} else {
		for (i = 0; i < PANEL_BL_INFO_NUM; i++) {
			panel->panel_bl_info[i] = panel_bl_info[i];
		}
	}

	panel->onoff_mode_enabled = of_property_read_bool(of_node,
		"qcom,disp-panel-offon-mode-enabled");

	panel->disable_cabc = of_property_read_bool(of_node,
		"qcom,disp-paneloff-disablecabc-enabled");

	panel->off_keep_reset = of_property_read_bool(of_node,
		"qcom,mdss-panel-off-keep-reset");

	rc = of_property_read_u32(of_node,
		"qcom,mdss-panel-on-dimming-delay", &panel->panel_on_dimming_delay);
	if (rc) {
		panel->panel_on_dimming_delay = 0;
		pr_info("Panel on dimming delay disabled\n");
	} else {
		pr_info("Panel on dimming delay %d ms\n", panel->panel_on_dimming_delay);
	}

	rc = of_property_read_u32(of_node,
			"qcom,disp-doze-backlight-threshold", &panel->doze_backlight_threshold);
	if (rc) {
		panel->doze_backlight_threshold = DOZE_MIN_BRIGHTNESS_LEVEL;
		pr_info("default doze backlight threshold is %d\n", DOZE_MIN_BRIGHTNESS_LEVEL);
	} else {
		pr_info("doze backlight threshold %d \n", panel->doze_backlight_threshold);
	}

	rc = of_property_read_u32(of_node,
			"qcom,mdss-dsi-panel-dc-threshold", &panel->dc_threshold);
	if (rc) {
		panel->dc_threshold = 320;
		pr_info("default dc backlight threshold is %d\n", panel->dc_threshold);
	} else {
		pr_info("dc backlight threshold %d \n", panel->dc_threshold);
	}

	INIT_DELAYED_WORK(&panel->cmds_work, panelon_dimming_enable_delayed_work);

	panel->dsi_panel_off_mode = false;
	panel->fod_hbm_enabled = false;
	panel->in_aod = false;
	panel->skip_dimmingon = STATE_NONE;

	panel->panel_active = 0;
	panel->kickoff_count = 0;
	panel->bl_duration = 0;
	panel->bl_level_integral = 0;
	panel->bl_highlevel_duration = 0;
	panel->bl_lowlevel_duration = 0;
	panel->hbm_duration = 0;
	panel->hbm_times = 0;
	panel->panel_dead = 0;
	panel->dc_enable = false;

	return rc;
}

struct dsi_panel *dsi_panel_get(struct device *parent,
				struct device_node *of_node,
				int topology_override,
				enum dsi_panel_type type)
{
	struct dsi_panel *panel;
	int rc = 0;

	panel = kzalloc(sizeof(*panel), GFP_KERNEL);
	if (!panel)
		return ERR_PTR(-ENOMEM);

	if (type == DSI_PANEL) {
		panel->name = of_get_property(of_node,
			"qcom,mdss-dsi-panel-name", NULL);
		if (!panel->name)
			panel->name = DSI_PANEL_DEFAULT_LABEL;


	panel->on_cmds_tuning = of_property_read_bool(of_node,
				"qcom,mdss-dsi-on-command-tuning");
	pr_info("[LCD]%s: on_cmds_tuning = %d.\n", __func__, panel->on_cmds_tuning);

		rc = dsi_panel_parse_host_config(panel, of_node);
		if (rc) {
			pr_err("failed to parse host configuration, rc=%d\n",
				rc);
			goto error;
		}

		rc = dsi_panel_parse_panel_mode(panel, of_node);
		if (rc) {
			pr_err("failed to parse panel mode configuration, rc=%d\n",
				rc);
			goto error;
		}

		rc = dsi_panel_parse_dfps_caps(&panel->dfps_caps,
			of_node, panel->name);
		if (rc)
			pr_err("failed to parse dfps configuration, rc=%d\n",
				rc);

		if (panel->panel_mode == DSI_OP_VIDEO_MODE) {
			rc = dsi_panel_parse_dyn_clk_caps(&panel->dyn_clk_caps,
				of_node, panel->name);
			if (rc)
				pr_err("failed to parse dynamic clk config, rc=%d\n",
				       rc);
		}

		rc = dsi_panel_parse_phy_props(&panel->phy_props,
			of_node, panel->name);
		if (rc) {
			pr_err("failed to parse panel physical dimension, rc=%d\n",
				rc);
			goto error;
		}

		rc = dsi_panel_parse_power_cfg(parent, panel, of_node);
		if (rc)
			pr_err("failed to parse power config, rc=%d\n", rc);

		rc = dsi_panel_parse_gpios(panel, of_node);
		if (rc)
			pr_err("failed to parse panel gpios, rc=%d\n", rc);

		rc = dsi_panel_parse_bl_config(panel, of_node);
		if (rc)
			pr_err("failed to parse backlight config, rc=%d\n", rc);


		rc = dsi_panel_parse_misc_features(panel, of_node);
		if (rc)
			pr_err("failed to parse misc features, rc=%d\n", rc);

		rc = dsi_panel_parse_hdr_config(panel, of_node);
		if (rc)
			pr_err("failed to parse hdr config, rc=%d\n", rc);

		rc = dsi_panel_get_mode_count(panel, of_node);
		if (rc) {
			pr_err("failed to get mode count, rc=%d\n", rc);
			goto error;
		}

		rc = dsi_panel_parse_dms_info(panel, of_node);
		if (rc)
			pr_debug("failed to get dms info, rc=%d\n", rc);

		rc = dsi_panel_parse_esd_config(panel, of_node);
		if (rc)
			pr_debug("failed to parse esd config, rc=%d\n", rc);

		panel->type = DSI_PANEL;
	} else if (type == EXT_BRIDGE) {
		panel->name = EXT_BRIDGE_DEFAULT_LABEL;
		panel->type = EXT_BRIDGE;
	} else {
		pr_err("invalid panel type\n");
		rc = -ENOTSUPP;
		goto error;
	}

	rc = dsi_panel_parse_mi_config(panel, of_node);
	if (rc)
		pr_err("failed to parse mi config, rc=%d\n", rc);

	panel->panel_of_node = of_node;
	drm_panel_init(&panel->drm_panel);
	mutex_init(&panel->panel_lock);
	panel->hist_bl_offset = 0;
	panel->parent = parent;
	g_panel = panel;
	return panel;
error:
	kfree(panel);
	return ERR_PTR(rc);
}

void dsi_panel_put(struct dsi_panel *panel)
{
	/* free resources allocated for ESD check */
	if (panel->type == DSI_PANEL)
		dsi_panel_esd_config_deinit(&panel->esd_config);

	kfree(panel);
}

static int dsi_display_write_panel(struct dsi_panel *panel,
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
		pr_debug("[%s] No commands to be sent for state\n",
			 panel->name);
		goto error;
	}

	for (i = 0; i < count; i++) {
		if (state == DSI_CMD_SET_STATE_LP)
			cmds->msg.flags |= MIPI_DSI_MSG_USE_LPM;

		if (cmds->last_command)
			cmds->msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;

		len = ops->transfer(panel->host, &cmds->msg);//dsi_host_transfer,
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

ssize_t mipi_reg_write(char *buf, size_t count)
{
	struct dsi_panel *panel = g_panel;
	struct dsi_panel_cmd_set cmd_sets = {0};
	int retval = 0, dlen = 0;
	u32 packet_count = 0;
	char *input = NULL, *data = NULL;
	char pbuf[3] = {0};
	u32 tmp_data = 0;
	int i = 0;

	mutex_lock(&panel->panel_lock);

	if (!panel || !panel->panel_initialized) {
		pr_err("[LCD] panel not ready!\n");
		retval = -EAGAIN;
		goto exit_unlock;
	}

	input = buf;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	retval = kstrtou32(pbuf, 10, &tmp_data);
	if (retval)
		goto exit_unlock;
	read_reg.enabled = !!tmp_data;
	input = input + 3;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	retval = kstrtou32(pbuf, 10, &tmp_data);
	if (retval)
		goto exit_unlock;
	if (read_reg.enabled && !tmp_data) {
		retval = -EINVAL;
		goto exit_unlock;
	}
	read_reg.cmds_rlen = tmp_data;
	input = input + 3;

	data = kzalloc(count - 6, GFP_KERNEL);
	if (!data) {
		retval = -ENOMEM;
		goto exit_unlock;
	}
	data[count-6-1] = '\0';
	dlen = string_merge_into_buf(input, count - 6, data);
	if (dlen <= 0)
		goto exit_free1;

	retval = dsi_panel_get_cmd_pkt_count(data, dlen, &packet_count);
	if (!packet_count) {
		pr_err("%s: get pkt count failed!\n", __func__);
		goto exit_free1;
	}

	retval = dsi_panel_alloc_cmd_packets(&cmd_sets, packet_count);
	if (retval) {
		pr_err("%s: failed to allocate cmd packets, ret=%d\n", __func__, retval);
		goto exit_free1;
	}

	retval = dsi_panel_create_cmd_packets(data, dlen, packet_count,
						  cmd_sets.cmds);
	if (retval) {
		pr_err("%s: failed to create cmd packets, ret=%d\n", __func__, retval);
		goto exit_free2;
	}

	if (read_reg.enabled) {
		read_reg.read_cmd = cmd_sets;
		retval = dsi_display_read_panel(panel, &read_reg);
		if (retval <= 0) {
			pr_err("%s: [%s]failed to read cmds, rc=%d\n", __func__, panel->name, retval);
			goto exit_free3;
		}
	} else {
		read_reg.read_cmd = cmd_sets;
		retval = dsi_display_write_panel(panel, &cmd_sets);
		if (retval) {
			pr_err("%s: [%s] failed to send cmds, rc=%d\n", __func__, panel->name, retval);
			goto exit_free3;
		}
	}

	pr_debug("[%s]: mipi_procfs_write done!\n", panel->name);
	retval = count;

exit_free3:
	for (i = packet_count - 1; i >= 0; i--) {
		kfree(cmd_sets.cmds[i].msg.tx_buf);
	}
exit_free2:
	kfree(cmd_sets.cmds);
	cmd_sets.cmds = NULL;
exit_free1:
	kfree(data);
exit_unlock:
	mutex_unlock(&panel->panel_lock);
	return retval;
}


ssize_t mipi_reg_read(char *buf)
{
	struct dsi_panel *panel = g_panel;
	int i = 0;
	ssize_t count = 0;

	mutex_lock(&panel->panel_lock);
	if (!panel) {
		mutex_unlock(&panel->panel_lock);
		return -EAGAIN;
	}

	if (read_reg.enabled) {
		for (i = 0; i < read_reg.cmds_rlen; i++) {
			if (i == read_reg.cmds_rlen - 1) {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02x\n",
				     read_reg.rbuf[i]);
			} else {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02x ",
				     read_reg.rbuf[i]);
			}
		}
	}
	mutex_unlock(&panel->panel_lock);

	return count;
}

#if DSI_READ_WRITE_PANEL_DEBUG
static ssize_t mipi_reg_procfs_write(struct file *file, const char __user *buf,
	size_t count, loff_t *offp)
{
	int retval = 0;
	char *input = NULL;

	input = kmalloc(count, GFP_KERNEL);
	if (!input) {
		return -ENOMEM;
	}
	if (copy_from_user(input, buf, count)) {
		pr_err("copy from user failed\n");
		retval = -EFAULT;
		goto end;
	}
	input[count-1] = '\0';
	pr_debug("copy_from_user input: %s\n", input);

	retval = mipi_reg_write(input, count);
end:
	kfree(input);
	return retval;
}
static int mipi_reg_procfs_show(struct seq_file *m, void *v)
{
	struct dsi_panel *panel = (struct dsi_panel *)m->private;
	int i = 0;

	mutex_lock(&panel->panel_lock);

	if (!panel) {
		mutex_unlock(&panel->panel_lock);
		return -EAGAIN;
	}

	if (read_reg.enabled) {
		seq_printf(m, "return value: ");
		for (i = 0; i < read_reg.cmds_rlen; i++) {
			printk("0x%02x ", read_reg.rbuf[i]);
			seq_printf(m, "0x%02x ", read_reg.rbuf[i]);
		}
	}
	seq_printf(m,"\n");
	mutex_unlock(&panel->panel_lock);

	return 0;
}

static int mipi_reg_procfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, mipi_reg_procfs_show, g_panel);
}

const struct file_operations mipi_reg_proc_fops = {
	.owner   = THIS_MODULE,
	.open    = mipi_reg_procfs_open,
	.write   = mipi_reg_procfs_write,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};
#endif

int dsi_panel_drv_init(struct dsi_panel *panel,
		       struct mipi_dsi_host *host)
{
	int rc = 0;
	struct mipi_dsi_device *dev;

	if (!panel || !host) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (panel->type == EXT_BRIDGE)
		return 0;

	mutex_lock(&panel->panel_lock);

	dev = &panel->mipi_device;

	dev->host = host;
	/*
	 * We dont have device structure since panel is not a device node.
	 * When using drm panel framework, the device is probed when the host is
	 * create.
	 */
	dev->channel = 0;
	dev->lanes = 4;

	panel->host = host;
	rc = dsi_panel_vreg_get(panel);
	if (rc) {
		pr_err("[%s] failed to get panel regulators, rc=%d\n",
		       panel->name, rc);
		goto exit;
	}

	rc = dsi_panel_pinctrl_init(panel);
	if (rc) {
		pr_err("[%s] failed to init pinctrl, rc=%d\n", panel->name, rc);
		goto error_vreg_put;
	}

	rc = dsi_panel_gpio_request(panel);
	if (rc) {
		pr_err("[%s] failed to request gpios, rc=%d\n", panel->name,
		       rc);
		goto error_pinctrl_deinit;
	}

	rc = dsi_panel_bl_register(panel);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			pr_err("[%s] failed to register backlight, rc=%d\n",
			       panel->name, rc);
		goto error_gpio_release;
	}

#if DSI_READ_WRITE_PANEL_DEBUG
	mipi_proc_entry = proc_create(MIPI_PROC_NAME, 0, NULL, &mipi_reg_proc_fops);
	if (!mipi_proc_entry)
		printk(KERN_WARNING "mipi_reg: unable to create proc entry.\n");
#endif

	goto exit;

error_gpio_release:
	(void)dsi_panel_gpio_release(panel);
error_pinctrl_deinit:
	(void)dsi_panel_pinctrl_deinit(panel);
error_vreg_put:
	(void)dsi_panel_vreg_put(panel);
exit:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_drv_deinit(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (panel->type == EXT_BRIDGE)
		return 0;

	mutex_lock(&panel->panel_lock);

	rc = dsi_panel_bl_unregister(panel);
	if (rc)
		pr_err("[%s] failed to unregister backlight, rc=%d\n",
		       panel->name, rc);

	rc = dsi_panel_gpio_release(panel);
	if (rc)
		pr_err("[%s] failed to release gpios, rc=%d\n", panel->name,
		       rc);

	rc = dsi_panel_pinctrl_deinit(panel);
	if (rc)
		pr_err("[%s] failed to deinit gpios, rc=%d\n", panel->name,
		       rc);

	rc = dsi_panel_vreg_put(panel);
	if (rc)
		pr_err("[%s] failed to put regs, rc=%d\n", panel->name, rc);

	panel->host = NULL;
	memset(&panel->mipi_device, 0x0, sizeof(panel->mipi_device));

#if DSI_READ_WRITE_PANEL_DEBUG
	if (mipi_proc_entry) {
		remove_proc_entry(MIPI_PROC_NAME, NULL);
		mipi_proc_entry = NULL;
	}
#endif

	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_validate_mode(struct dsi_panel *panel,
			    struct dsi_display_mode *mode)
{
	return 0;
}

int dsi_panel_get_mode_count(struct dsi_panel *panel,
	struct device_node *of_node)
{
	const u32 SINGLE_MODE_SUPPORT = 1;
	struct device_node *timings_np;
	int count, rc = 0;

	if (!of_node || !panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	panel->num_timing_nodes = 0;

	timings_np = of_get_child_by_name(of_node,
			"qcom,mdss-dsi-display-timings");
	if (!timings_np) {
		pr_err("no display timing nodes defined\n");
		rc = -EINVAL;
		goto error;
	}

	count = of_get_child_count(timings_np);
	if (!count || count > DSI_MODE_MAX) {
		pr_err("invalid count of timing nodes: %d\n", count);
		rc = -EINVAL;
		goto error;
	}

	/* No multiresolution support is available for video mode panels */
	if (panel->panel_mode != DSI_OP_CMD_MODE)
		count = SINGLE_MODE_SUPPORT;

	panel->num_timing_nodes = count;

error:
	return rc;
}

int dsi_panel_get_phy_props(struct dsi_panel *panel,
			    struct dsi_panel_phy_props *phy_props)
{
	int rc = 0;

	if (!panel || !phy_props) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	memcpy(phy_props, &panel->phy_props, sizeof(*phy_props));
	return rc;
}

int dsi_panel_get_dfps_caps(struct dsi_panel *panel,
			    struct dsi_dfps_capabilities *dfps_caps)
{
	int rc = 0;

	if (!panel || !dfps_caps) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	memcpy(dfps_caps, &panel->dfps_caps, sizeof(*dfps_caps));
	return rc;
}

void dsi_panel_put_mode(struct dsi_display_mode *mode)
{
	int i;

	if (!mode->priv_info)
		return;

	kfree(mode->priv_info->phy_timing_val);

	for (i = 0; i < DSI_CMD_SET_MAX; i++)
		dsi_panel_destroy_cmd_packets(&mode->priv_info->cmd_sets[i]);

	kfree(mode->priv_info);
	mode->priv_info = NULL;
}

int dsi_panel_get_mode(struct dsi_panel *panel,
			u32 index, struct dsi_display_mode *mode,
			int topology_override)
{
	struct device_node *timings_np, *child_np;
	struct dsi_display_mode_priv_info *prv_info;
	u32 child_idx = 0;
	int rc = 0, num_timings;

	if (!panel || !mode) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (panel->type == EXT_BRIDGE)
		return 0;

	mutex_lock(&panel->panel_lock);

	mode->priv_info = kzalloc(sizeof(*mode->priv_info), GFP_KERNEL);
	if (!mode->priv_info) {
		rc = -ENOMEM;
		goto done;
	}

	prv_info = mode->priv_info;

	timings_np = of_get_child_by_name(panel->panel_of_node,
		"qcom,mdss-dsi-display-timings");
	if (!timings_np) {
		pr_err("no display timing nodes defined\n");
		rc = -EINVAL;
		goto parse_fail;
	}

	num_timings = of_get_child_count(timings_np);
	if (!num_timings || num_timings > DSI_MODE_MAX) {
		pr_err("invalid count of timing nodes: %d\n", num_timings);
		rc = -EINVAL;
		goto parse_fail;
	}

	for_each_child_of_node(timings_np, child_np) {
		if (index != child_idx++)
			continue;

		rc = dsi_panel_parse_timing(panel->parent, &mode->timing,
			panel->name, child_np);
		if (rc) {
			pr_err("failed to parse panel timing, rc=%d\n", rc);
			goto parse_fail;
		}

		rc = dsi_panel_parse_dsc_params(mode, child_np);
		if (rc) {
			pr_err("failed to parse dsc params, rc=%d\n", rc);
			goto parse_fail;
		}

		rc = dsi_panel_parse_topology(prv_info, child_np,
				topology_override);
		if (rc) {
			pr_err("failed to parse panel topology, rc=%d\n", rc);
			goto parse_fail;
		}

		rc = dsi_panel_parse_cmd_sets(prv_info, child_np);
		if (rc) {
			pr_err("failed to parse command sets, rc=%d\n", rc);
			goto parse_fail;
		}

		rc = dsi_panel_parse_jitter_config(mode, child_np);
		if (rc)
			pr_err(
			"failed to parse panel jitter config, rc=%d\n", rc);

		rc = dsi_panel_parse_phy_timing(mode, child_np);
		if (rc) {
			pr_err(
			"failed to parse panel phy timings, rc=%d\n", rc);
			goto parse_fail;
		}

		rc = dsi_panel_parse_partial_update_caps(mode, child_np);
		if (rc)
			pr_err("failed to partial update caps, rc=%d\n", rc);
	}
	goto done;

parse_fail:
	kfree(mode->priv_info);
	mode->priv_info = NULL;
done:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_get_host_cfg_for_mode(struct dsi_panel *panel,
				    struct dsi_display_mode *mode,
				    struct dsi_host_config *config)
{
	int rc = 0;

	if (!panel || !mode || !config) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	config->panel_mode = panel->panel_mode;
	memcpy(&config->common_config, &panel->host_config,
	       sizeof(config->common_config));

	if (panel->panel_mode == DSI_OP_VIDEO_MODE) {
		memcpy(&config->u.video_engine, &panel->video_config,
		       sizeof(config->u.video_engine));
	} else {
		memcpy(&config->u.cmd_engine, &panel->cmd_config,
		       sizeof(config->u.cmd_engine));
	}

	memcpy(&config->video_timing, &mode->timing,
	       sizeof(config->video_timing));

	if (mode->priv_info) {
		config->video_timing.dsc_enabled = mode->priv_info->dsc_enabled;
		config->video_timing.dsc = &mode->priv_info->dsc;
		config->bit_clk_rate_hz = mode->timing.clk_rate_hz;
	}
	config->esc_clk_rate_hz = 19200000;
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_pre_prepare(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (panel->type == EXT_BRIDGE)
		return 0;

	mutex_lock(&panel->panel_lock);
#if 0
	/* If LP11_INIT is set, panel will be powered up during prepare() */
	if (panel->lp11_init)
		goto error;
#endif
	rc = dsi_panel_power_on(panel);
	if (rc) {
		pr_err("[%s] panel power on failed, rc=%d\n", panel->name, rc);
		goto error;
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_update_pps(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_panel_cmd_set *set = NULL;
	struct dsi_display_mode_priv_info *priv_info = NULL;

	if (!panel || !panel->cur_mode) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (panel->type == EXT_BRIDGE)
		return 0;

	mutex_lock(&panel->panel_lock);

	priv_info = panel->cur_mode->priv_info;

	set = &priv_info->cmd_sets[DSI_CMD_SET_PPS];

	dsi_dsc_create_pps_buf_cmd(&priv_info->dsc, panel->dsc_pps_cmd, 0);
	rc = dsi_panel_create_cmd_packets(panel->dsc_pps_cmd,
					  DSI_CMD_PPS_SIZE, 1, set->cmds);
	if (rc) {
		pr_err("failed to create cmd packets, rc=%d\n", rc);
		goto error;
	}

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_PPS);
	if (rc) {
		pr_err("[%s] failed to send DSI_CMD_SET_PPS cmds, rc=%d\n",
			panel->name, rc);
	}

	dsi_panel_destroy_cmds_packets_buf(set);
error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_set_lp1(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}
	pr_info("%s\n", __func__);

	if (panel->type == EXT_BRIDGE)
		return 0;

	mutex_lock(&panel->panel_lock);
	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_LP1);
	if (rc)
		pr_err("[%s] failed to send DSI_CMD_SET_LP1 cmd, rc=%d\n",
		       panel->name, rc);
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_set_lp2(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}
	pr_info("%s\n", __func__);

	if (panel->type == EXT_BRIDGE)
		return 0;

	mutex_lock(&panel->panel_lock);
	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_LP2);
	if (rc)
		pr_err("[%s] failed to send DSI_CMD_SET_LP2 cmd, rc=%d\n",
		       panel->name, rc);
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_set_nolp(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}
	pr_info("%s\n", __func__);

	if (panel->type == EXT_BRIDGE)
		return 0;

	mutex_lock(&panel->panel_lock);

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_NOLP);
	if (rc)
		pr_err("[%s] failed to send DSI_CMD_SET_NOLP cmd, rc=%d\n",
				panel->name, rc);

	panel->fod_hbm_enabled = false;
	panel->in_aod = false;
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_prepare(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (panel->type == EXT_BRIDGE)
		return 0;

	mutex_lock(&panel->panel_lock);

	if (panel->lp11_init) {
#if 0
		rc = dsi_panel_power_on(panel);
		if (rc) {
			pr_err("[%s] panel power on failed, rc=%d\n",
			       panel->name, rc);
			goto error;
		}
#endif
		rc = dsi_panel_reset(panel);
		if (rc) {
			pr_err("[%s] failed to reset panel, rc=%d\n", panel->name, rc);
			goto error;
		}
	}

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_PRE_ON);
	if (rc) {
		pr_err("[%s] failed to send DSI_CMD_SET_PRE_ON cmds, rc=%d\n",
		       panel->name, rc);
		goto error;
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

static int dsi_panel_roi_prepare_dcs_cmds(struct dsi_panel_cmd_set *set,
		struct dsi_rect *roi, int ctrl_idx, int unicast)
{
	static const int ROI_CMD_LEN = 5;

	int rc = 0;

	/* DTYPE_DCS_LWRITE */
	static char *caset, *paset;

	set->cmds = NULL;

	caset = kzalloc(ROI_CMD_LEN, GFP_KERNEL);
	if (!caset) {
		rc = -ENOMEM;
		goto exit;
	}
	caset[0] = 0x2a;
	caset[1] = (roi->x & 0xFF00) >> 8;
	caset[2] = roi->x & 0xFF;
	caset[3] = ((roi->x - 1 + roi->w) & 0xFF00) >> 8;
	caset[4] = (roi->x - 1 + roi->w) & 0xFF;

	paset = kzalloc(ROI_CMD_LEN, GFP_KERNEL);
	if (!paset) {
		rc = -ENOMEM;
		goto error_free_mem;
	}
	paset[0] = 0x2b;
	paset[1] = (roi->y & 0xFF00) >> 8;
	paset[2] = roi->y & 0xFF;
	paset[3] = ((roi->y - 1 + roi->h) & 0xFF00) >> 8;
	paset[4] = (roi->y - 1 + roi->h) & 0xFF;

	set->type = DSI_CMD_SET_ROI;
	set->state = DSI_CMD_SET_STATE_LP;
	set->count = 2; /* send caset + paset together */
	set->cmds = kcalloc(set->count, sizeof(*set->cmds), GFP_KERNEL);
	if (!set->cmds) {
		rc = -ENOMEM;
		goto error_free_mem;
	}
	set->cmds[0].msg.channel = 0;
	set->cmds[0].msg.type = MIPI_DSI_DCS_LONG_WRITE;
	set->cmds[0].msg.flags = unicast ? MIPI_DSI_MSG_UNICAST : 0;
	set->cmds[0].msg.ctrl = unicast ? ctrl_idx : 0;
	set->cmds[0].msg.tx_len = ROI_CMD_LEN;
	set->cmds[0].msg.tx_buf = caset;
	set->cmds[0].msg.rx_len = 0;
	set->cmds[0].msg.rx_buf = 0;
	set->cmds[0].msg.wait_ms = 0;
	set->cmds[0].last_command = 0;
	set->cmds[0].post_wait_ms = 0;

	set->cmds[1].msg.channel = 0;
	set->cmds[1].msg.type = MIPI_DSI_DCS_LONG_WRITE;
	set->cmds[1].msg.flags = unicast ? MIPI_DSI_MSG_UNICAST : 0;
	set->cmds[1].msg.ctrl = unicast ? ctrl_idx : 0;
	set->cmds[1].msg.tx_len = ROI_CMD_LEN;
	set->cmds[1].msg.tx_buf = paset;
	set->cmds[1].msg.rx_len = 0;
	set->cmds[1].msg.rx_buf = 0;
	set->cmds[1].msg.wait_ms = 0;
	set->cmds[1].last_command = 1;
	set->cmds[1].post_wait_ms = 0;

	goto exit;

error_free_mem:
	kfree(caset);
	kfree(paset);
	kfree(set->cmds);

exit:
	return rc;
}

int dsi_panel_send_roi_dcs(struct dsi_panel *panel, int ctrl_idx,
		struct dsi_rect *roi)
{
	int rc = 0;
	struct dsi_panel_cmd_set *set;
	struct dsi_display_mode_priv_info *priv_info;

	if (!panel || !panel->cur_mode) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (panel->type == EXT_BRIDGE)
		return 0;

	priv_info = panel->cur_mode->priv_info;
	set = &priv_info->cmd_sets[DSI_CMD_SET_ROI];

	rc = dsi_panel_roi_prepare_dcs_cmds(set, roi, ctrl_idx, true);
	if (rc) {
		pr_err("[%s] failed to prepare DSI_CMD_SET_ROI cmds, rc=%d\n",
				panel->name, rc);
		return rc;
	}
	pr_debug("[%s] send roi x %d y %d w %d h %d\n", panel->name,
			roi->x, roi->y, roi->w, roi->h);

	mutex_lock(&panel->panel_lock);

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ROI);
	if (rc)
		pr_err("[%s] failed to send DSI_CMD_SET_ROI cmds, rc=%d\n",
				panel->name, rc);

	mutex_unlock(&panel->panel_lock);

	dsi_panel_destroy_cmd_packets(set);

	return rc;
}

static void handle_dsi_read_data(struct dsi_panel *panel, struct dsi_read_config *read_config)
{
	int i = 0;
	int param_nb = 0, write_len = 0;
	u32 read_cnt = 0, bit_valide = 0;

	u8 *pRead_data = panel->panel_read_data;
	read_cnt = read_config->cmds_rlen;
	bit_valide = read_config->valid_bits;

	for (i = 0; i < read_cnt; i++) {
		if ((bit_valide & 0x1) && ((pRead_data + 8) < (panel->panel_read_data + BUF_LEN_MAX))) {
			write_len = scnprintf(pRead_data, 8, "p%d=%d", param_nb, read_config->rbuf[i]);
			pRead_data += write_len;
			param_nb ++;
		}
		bit_valide = bit_valide >> 1;
	}
	pr_info("read %s from panel\n", panel->panel_read_data);

	return;
}

static int panel_disp_param_send_lock(struct dsi_panel *panel, int param)
{
	int rc = 0;
	uint32_t temp = 0;
	int read_val = 0;
	u32  bl_level = 0;
	mutex_lock(&panel->panel_lock);

	if (!panel->panel_initialized) {
		pr_err("[LCD] panel not ready!\n");
		mutex_unlock(&panel->panel_lock);
		return rc;
	}

	pr_debug("[LCD] param_type=%d\n", param);

	if (param & 0x1000000) {
		panel->hist_bl_offset = (param & 0x0FF);
		if (panel->hist_bl_offset > HIST_BL_OFFSET_LIMIT)
			panel->hist_bl_offset = HIST_BL_OFFSET_LIMIT;

		if (panel->last_bl_lvl) {
			bl_level = panel->last_bl_lvl + panel->hist_bl_offset;
			dsi_panel_backlight_consistency(panel, &bl_level);
			if  (panel->bl_config.type == DSI_BACKLIGHT_WLED)
				led_trigger_event(panel->bl_config.wled, bl_level);
			else if (panel->bl_config.type == DSI_BACKLIGHT_DCS)
				dsi_panel_update_backlight(panel, bl_level);
		} else {
			/* clear bl_offset if backlight's already off */
			panel->hist_bl_offset = 0;
		}
		pr_info("[LCD] last_bl_lvl:%d,offset:%d,bl_level:%d!\n", panel->last_bl_lvl, panel->hist_bl_offset, bl_level);
		param = 0x1000000;
	}

	temp = param & 0x0000000F;
	switch (temp) {
	case 0x1:
		pr_info("warm\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_WARM);
		break;
	case 0x2:		/*normal*/
		pr_info("normal\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_DEFAULT);
		break;
	case 0x3:		/*cold*/
		pr_info("cold\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_COLD);
		break;
	case 0x5:
		pr_info("paper mode\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_COLD);
		break;
	case 0x6:
		pr_info("paper mode 1\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_PAPER1);
		break;
	case 0x7:
		pr_info("paper mode 2\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_PAPER2);
		break;
	case 0x8:
		pr_info("paper mode 3\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_PAPER3);
		break;
	case 0x9:
		pr_info("paper mode 4\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_PAPER4);
		break;
	case 0xa:
		pr_info("paper mode 5\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_PAPER5);
		break;
	case 0xb:
		pr_info("paper mode 6\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_PAPER6);
		break;
	case 0xc:
		pr_info("paper mode 7\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_PAPER7);
		break;
	default:
		break;
	}

	temp = param & 0x000000F0;
	switch (temp) {
	case 0x10:		/*ce on default*/
		pr_info("ceon\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_CEON);
		break;
	case 0xF0:		/*ce off*/
		pr_info("ceoff\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_CEOFF);
		break;
	default:
		break;
	}

	temp = param & 0x00000F00;
	switch (temp) {
	case 0x100:
		pr_info("cabcuion\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_CABCUION);
		break;
	case 0x200:
		pr_info("cabcstillon\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_CABCSTILLON);
		break;
	case 0x300:
		pr_info("cabcmovieon\n");
		dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_CABCMOVIEON);
		break;
	case 0x400:
		pr_info("cabcoff\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_CABCOFF);
		break;
	case 0x500:
		pr_info("skince cabcuion\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_SKINCE_CABCUION);
		break;
	case 0x600:
		pr_info("skince cabcstillon\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_SKINCE_CABCSTILLON);
		break;
	case 0x700:
		pr_info("skince cabcmovieon\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_SKINCE_CABCMOVIEON);
		break;
	case 0x800:
		pr_info("skince cabcoff\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_SKINCE_CABCOFF);
		break;
	case 0xE00:
		pr_info("dimming off");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_DIMMINGOFF);
		break;
	case PANEL_DIMMING_ON_CMD:
		pr_info("dimmingon\n");
		if (panel->skip_dimmingon != STATE_DIM_BLOCK && !panel->in_aod) {
			if (ktime_after(ktime_get(), panel->fod_hbm_off_time)) {
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_DIMMINGON);
			} else {
				pr_info("skip dimmingon due to hbm off\n");
			}
		} else {
			pr_info("skip dimmingon due to hbm on\n");
		}
		break;
	default:
		break;
	}

	temp = param & 0x0000F000;
	switch (temp) {
	case 0x1000:
		pr_info("acl level 1\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_ACL_L1);
		break;
	case 0x2000:
		pr_info("acl level 2\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_ACL_L2);
		break;
	case 0x3000:
		pr_info("acl level 3\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_ACL_L3);
		break;
	case 0xF000:
		pr_info("acl off\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_ACL_OFF);
		break;
	default:
		break;
	}

	temp = param & 0x000F0000;
	switch (temp) {
	case 0x10000:
		pr_info("hbm on\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_HBM_ON);
		break;
	case 0x20000:
		pr_info("hbm fod on\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_HBM_FOD_ON);
		panel->skip_dimmingon = STATE_DIM_BLOCK;
		panel->fod_hbm_enabled = true;
		break;
	case 0x30000:
		pr_info("hbm fod to normal mode\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_HBM_FOD2NORM);
		break;
	case 0x40000:
		pr_info("DC on\n");
		panel->dc_enable = true;
		break;
	case 0x50000:
		pr_info("DC off\n");
		panel->dc_enable = false;
		break;
	case 0xE0000:
		pr_info("hbm fod off\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_HBM_FOD_OFF);
		panel->skip_dimmingon = STATE_DIM_RESTORE;
		panel->fod_hbm_enabled = false;
		panel->fod_hbm_off_time = ktime_add_ms(ktime_get(), 170);
		break;
	case 0xF0000:
		pr_info("hbm off\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_HBM_OFF);
		break;
	case DISPLAY_OFF_MODE:
		pr_info("display off mode\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_OFF_MODE);
		break;
	case DISPLAY_ON_MODE:
		pr_info("display on mode\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_ON_MODE);
		break;
	default:
		break;
	}

	temp = param & 0x00F00000;
	switch (temp) {
	case 0x100000:
		pr_info("normal mode1\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_NORMAL1);
		break;
	case 0x300000:
		pr_info("sRGB\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_SRGB);
		break;
	case 0x600000:
		if (panel->in_aod) {
			pr_info("doze hbm On\n");
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DOZE_HBM);
		}
		break;
	case 0x700000:
		if (panel->in_aod) {
			pr_info("doze lbm On\n");
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DOZE_LBM);
		}
		break;
	case 0x800000:
		pr_info("doze Off\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_NOLP);
		break;
	case 0xA00000:
		{
			if (panel->last_bl_lvl >= 1022) {
				if (panel->backlight_delta == -1)
					panel->backlight_delta = -2;
				else
					panel->backlight_delta = -1;
			} else {
				if (panel->backlight_delta == 1)
					panel->backlight_delta = 2;
				else
					panel->backlight_delta = 1;
			}
			pr_info("backlight repeat:%d\n", (panel->last_bl_lvl + panel->backlight_delta));
			rc = dsi_panel_update_backlight(panel, (panel->last_bl_lvl + panel->backlight_delta));
		}
		break;
	case 0xC00000:
		pr_info("crc sRGB\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_CRC_SRGB);
		break;
	case 0xD00000:
		pr_info("crc DCI-P3\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_CRC_DCIP3);
		break;
	case 0xE00000:
		pr_info("crc off\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_CRC_OFF);
		break;
	default:
		break;
	}

	/* DSI Read Command */
	temp = param & 0xF0000000;
	switch (temp) {
	case 0x10000000:
		pr_info("read panel backlight\n");
		rc = dsi_display_read_panel(panel, &panel->brightness_cmds);
		break;
	case 0x20000000:
		pr_info("read xy coordinate\n");
		panel->xy_coordinate_cmds.read_cmd =
			panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_READ_XY_COORDINATE];
		rc = dsi_display_read_panel(panel, &panel->xy_coordinate_cmds);
		if (rc > 0)
			handle_dsi_read_data(panel, &panel->xy_coordinate_cmds);
		break;
	case 0x30000000:
		pr_info("read max luminance\n");
		panel->max_luminance_valid_cmds.read_cmd =
			panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_MAX_LUMINANCE_VALID];
		rc = dsi_display_read_panel(panel, &panel->max_luminance_valid_cmds);
		if (rc > 0)
			handle_dsi_read_data(panel, &panel->max_luminance_valid_cmds);
		sscanf(panel->panel_read_data, "p0=%d", &read_val);
		if ((read_val == 0xB1) || panel->panel_bl_info[0] == 308) {
			panel->max_luminance_cmds.read_cmd =
				panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_READ_MAX_LUMINANCE];
			rc = dsi_display_read_panel(panel, &panel->max_luminance_cmds);
			if (rc > 0) {
				handle_dsi_read_data(panel, &panel->max_luminance_cmds);
				set_panel_max_luminance(panel);
			}
		} else {
			/*panel_bl_info[1] is typical max luminance*/
			max_lum = panel->panel_bl_info[1];
			rc = -1;
		}
		break;
	case 0x40000000:
		pr_info("read ss panel backlight type\n");
		panel->panel_ddic_id_cmds.read_cmd =
			panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_READ_SS_PANEL_ID];
		if (panel->panel_ddic_id_cmds.read_cmd.count) {
			panel->panel_ddic_id_cmds.enabled = true;
			panel->panel_ddic_id_cmds.cmds_rlen = 1;
			rc = dsi_display_read_panel(panel, &panel->panel_ddic_id_cmds);
			if (rc > 0)
				panel->bl_config.ss_panel_id = panel->panel_ddic_id_cmds.rbuf[0];
		}
		break;
	case 0xe0000000:
		pr_info("Flashing Test On\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_FLASH_TEST_ON);
		break;
	case 0xf0000000:
		pr_info("Flashing Test Off\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_FLASH_TEST_OFF);
		break;
	default:
		break;
	}

	mutex_unlock(&panel->panel_lock);
	return rc;
}

static char string_to_hex(const char *str)
{
	char val_l = 0;
	char val_h = 0;

	if (str[0] >= '0' && str[0] <= '9')
		val_h = str[0] - '0';
	else if (str[0] <= 'f' && str[0] >= 'a')
		val_h = 10 + str[0] - 'a';
	else if (str[0] <= 'F' && str[0] >= 'A')
		val_h = 10 + str[0] - 'A';

	if (str[1] >= '0' && str[1] <= '9')
		val_l = str[1]-'0';
	else if (str[1] <= 'f' && str[1] >= 'a')
		val_l = 10 + str[1] - 'a';
	else if (str[1] <= 'F' && str[1] >= 'A')
		val_l = 10 + str[1] - 'A';

	return (val_h << 4) | val_l;
}

static int string_merge_into_buf(const char *str, int len, char *buf)
{
	int buf_size = 0;
	int i = 0;
	const char *p = str;

	while (i < len) {
		if (((p[0] >= '0' && p[0] <= '9') ||
			(p[0] <= 'f' && p[0] >= 'a') ||
			(p[0] <= 'F' && p[0] >= 'A'))
			&& ((i + 1) < len)) {
			buf[buf_size] = string_to_hex(p);
			pr_debug("0x%02x ", buf[buf_size]);
			buf_size++;
			i += 2;
			p += 2;
		} else {
			i++;
			p++;
		}
	}
	return buf_size;
}

static int parse_to_dcs_cmds(struct dsi_panel_cmd_set *on_cmd_sets)
{
	int ret = 0, dlen = 0;
	u32 packet_count = 0;
	char *data = NULL, *buf = NULL;
	struct file *filp = NULL;
	const char *file_name = "/data/lcd.txt";
	int file_size = 0;

	filp = filp_open(file_name, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		pr_err("[LCD]%s open failed\n", file_name);
		return -EINVAL;
	}

	file_size = filp->f_path.dentry->d_inode->i_size;
	data = (char *)kzalloc(file_size, GFP_KERNEL);
	if (!data) {
		filp_close(filp, NULL);
		return -ENOMEM;
	}

	buf = kzalloc(file_size/2, GFP_KERNEL);
	if (!buf) {
		kfree(data);
		filp_close(filp, NULL);
		return -ENOMEM;
	}

	ret = kernel_read(filp, filp->f_pos, data, file_size);
	if (ret < 0) {
		pr_err("[LCD]%s read failed, return %d\n", file_name, ret);
		goto exit_free;
	}

	dlen = string_merge_into_buf(data, file_size, buf);
	if (dlen <= 0)
		goto exit_free;

	ret = dsi_panel_get_cmd_pkt_count(buf, dlen, &packet_count);
	if (!packet_count) {
		pr_err("[LCD]%s: get pkt count failed!\n", __func__);
		goto exit_free;
	}

	if (on_cmd_sets->cmds) {
		kfree(on_cmd_sets->cmds);
		on_cmd_sets->cmds = NULL;
	}

	ret = dsi_panel_alloc_cmd_packets(on_cmd_sets, packet_count);
	if (ret) {
		pr_err("[LCD]failed to allocate cmd packets, ret=%d\n", ret);
		goto exit_free;
	}

	ret = dsi_panel_create_cmd_packets(buf, dlen, packet_count,
						  on_cmd_sets->cmds);
	if (ret) {
		pr_err("[LCD]failed to create cmd packets, ret=%d\n", ret);
		goto exit_free;
	}

	filp_close(filp, NULL);
	pr_info("[LCD]%s: parse done!\n", __func__);
	return 0;
exit_free:
	filp_close(filp, NULL);
	kfree(buf);
	kfree(data);
	return -ENOMEM;
}

int panel_disp_param_send(struct dsi_display *display, int param_type)
{
	int rc = 0;
	struct dsi_panel *panel = NULL;
	struct drm_device *drm_dev = NULL;

	if (!display || !display->panel || !display->drm_dev) {
		pr_err("invalid display/panel/drm_dev\n");
		return -EINVAL;
	}

	panel = display->panel;
	drm_dev = display->drm_dev;

	if (!panel->dispparam_enabled)
		return rc;

	rc = panel_disp_param_send_lock(panel, param_type);

	return rc;
}

int dsi_panel_switch(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (panel->type == EXT_BRIDGE)
		return 0;

	mutex_lock(&panel->panel_lock);

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_TIMING_SWITCH);
	if (rc)
		pr_err("[%s] failed to send DSI_CMD_SET_TIMING_SWITCH cmds, rc=%d\n",
		       panel->name, rc);

	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_post_switch(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (panel->type == EXT_BRIDGE)
		return 0;

	mutex_lock(&panel->panel_lock);

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_POST_TIMING_SWITCH);
	if (rc)
		pr_err("[%s] failed to send DSI_CMD_SET_POST_TIMING_SWITCH cmds, rc=%d\n",
		       panel->name, rc);

	mutex_unlock(&panel->panel_lock);
	return rc;
}

static int tf8719_on_cmd_add_60ms_delay(struct dsi_panel *panel,
			enum dsi_cmd_set_type type)
{
	static bool one_call;
	bool need_delay = 0;
	u32 version_major = 0;
	struct dsi_cmd_desc *cmds = NULL;

	if (one_call == 1)
		return 0;

	if (!panel || !panel->cur_mode)
		return -EINVAL;

	if ((get_hw_version_platform() != HARDWARE_PLATFORM_BERYLLIUM)
			|| strncmp(panel->name, "ebbg", 4)) {
		one_call = 1;
		return 0;
	}

	version_major = get_hw_version_major();

	if (version_major < 2) {
		need_delay = 1;
	} else if (version_major == 2) {
		if (get_hw_version_minor() < 1)
			need_delay = 1;
	} else {
		need_delay = 0;
	}

	if (need_delay) {
		cmds = panel->cur_mode->priv_info->cmd_sets[type].cmds;
		if (cmds && cmds[0].post_wait_ms == 0) {
			cmds[0].post_wait_ms = 0x3C;
		}
	}

	one_call = 1;
	return 0;
}

int dsi_panel_enable(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (panel->type == EXT_BRIDGE)
		return 0;

	mutex_lock(&panel->panel_lock);

	tf8719_on_cmd_add_60ms_delay(panel, DSI_CMD_SET_ON);

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ON);
	if (rc) {
		pr_err("[%s] failed to send DSI_CMD_SET_ON cmds, rc=%d\n",
		       panel->name, rc);
	}
	panel->panel_initialized = true;
	panel->fod_hbm_enabled = false;
	panel->in_aod = false;

	mutex_unlock(&panel->panel_lock);
	pr_info("[LCD] %s: DSI_CMD_SET_ON\n", __func__);

	if (panel->onoff_mode_enabled) {
		set_skip_panel_dead(false);
		pr_debug("%s: set set_skip_panel_dead = false \n", __func__);
	}

	return rc;
}

int dsi_panel_post_enable(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (panel->type == EXT_BRIDGE)
		return 0;

	mutex_lock(&panel->panel_lock);

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_POST_ON);
	if (rc) {
		pr_err("[%s] failed to send DSI_CMD_SET_POST_ON cmds, rc=%d\n",
		       panel->name, rc);
		goto error;
	}
error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_pre_disable(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (panel->type == EXT_BRIDGE)
		return 0;

	mutex_lock(&panel->panel_lock);

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_PRE_OFF);
	if (rc) {
		pr_err("[%s] failed to send DSI_CMD_SET_PRE_OFF cmds, rc=%d\n",
		       panel->name, rc);
		goto error;
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_disable(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (panel->type == EXT_BRIDGE)
		return 0;

	mutex_lock(&panel->panel_lock);

	/* Avoid sending panel off commands when ESD recovery is underway */
	if (!atomic_read(&panel->esd_recovery_pending)) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_OFF);
		if (rc) {
			pr_err("[%s] failed to send DSI_CMD_SET_OFF cmds, rc=%d\n",
					panel->name, rc);
			goto error;
		}
	}

	panel->panel_initialized = false;
	panel->skip_dimmingon = STATE_NONE;
	panel->fod_hbm_enabled = false;
	panel->in_aod = false;
	panel->fod_hbm_off_time = ktime_get();

error:
	mutex_unlock(&panel->panel_lock);
	pr_info("[LCD] %s: DSI_CMD_SET_OFF\n", __func__);
	if (panel->on_cmds_tuning)
		parse_to_dcs_cmds(&(panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_ON]));
	return rc;
}

int dsi_panel_unprepare(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (panel->type == EXT_BRIDGE)
		return 0;

	mutex_lock(&panel->panel_lock);

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_POST_OFF);
	if (rc) {
		pr_err("[%s] failed to send DSI_CMD_SET_POST_OFF cmds, rc=%d\n",
		       panel->name, rc);
		goto error;
	}

	if (!panel->lp11_init) {
		rc = dsi_panel_power_off(panel);
		if (rc) {
			pr_err("[%s] panel power_Off failed, rc=%d\n",
			       panel->name, rc);
			goto error;
		}
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_post_unprepare(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (panel->type == EXT_BRIDGE)
		return 0;

	mutex_lock(&panel->panel_lock);

	if (panel->lp11_init) {
		rc = dsi_panel_power_off(panel);
		if (rc) {
			pr_err("[%s] panel power_Off failed, rc=%d\n",
			       panel->name, rc);
			goto error;
		}
	}
error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

ssize_t dsi_panel_disp_count_get(struct dsi_display *display, char *buf)
{
	int ret = -1;
	struct timespec64 now_boot;
	u64 record_end = 0;
	/* struct timespec rtctime; */
	struct dsi_panel *panel = NULL;

	if (!display || !display->panel || !display->drm_dev) {
		pr_err("invalid display/panel/drm_dev\n");
		return -EINVAL;
	}

	if (buf == NULL) {
		pr_err("dsi_panel_disp_count_get buffer is NULL!\n");
		return -EINVAL;
	}

	panel = display->panel;
	get_monotonic_boottime64(&now_boot);
	/* getnstimeofday(&rtctime); */

	ret = scnprintf(buf, PAGE_SIZE,
		"panel_active=%llu\n"
		"panel_kickoff_count=%llu\n"
		"kernel_boottime=%llu\n"
		"kernel_rtctime=%llu\n"
		"kernel_days=%llu\n"
		"bl_duration=%llu\n"
		"bl_level_integral=%llu\n"
		"bl_highlevel_duration=%llu\n"
		"bl_lowlevel_duration=%llu\n"
		"hbm_duration=%llu\n"
		"hbm_times=%llu\n"
		"panel_dead=%llu\n"
		"record_end=%llu\n",
		panel->panel_active,
		panel->kickoff_count,
		panel->boottime + now_boot.tv_sec,
		panel->bootRTCtime,
		panel->bootdays,
		panel->bl_duration,
		panel->bl_level_integral,
		panel->bl_highlevel_duration,
		panel->bl_lowlevel_duration,
		panel->hbm_duration,
		panel->hbm_times,
		panel->panel_dead,
		record_end);

	return ret;
}

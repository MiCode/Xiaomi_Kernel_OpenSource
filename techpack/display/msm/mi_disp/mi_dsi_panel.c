/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#define pr_fmt(fmt)	"mi-dsi-panel:[%s:%d] " fmt, __func__, __LINE__
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/rtc.h>
#include <linux/pm_wakeup.h>
#include <video/mipi_display.h>

#include <drm/mi_disp.h>
#include <drm/sde_drm.h>
#include "sde_connector.h"
#include "sde_encoder.h"
#include "sde_crtc.h"
#include "sde_trace.h"

#include "dsi_panel.h"
#include "dsi_display.h"
#include "dsi_ctrl_hw.h"
#include "dsi_parser.h"
#include "../../../../kernel/irq/internals.h"
#include "mi_disp_feature.h"
#include "mi_disp_print.h"
#include "mi_dsi_display.h"
#include "mi_disp_nvt_alpha_data.h"

extern void mi_sde_connector_fod_ui_ready(struct dsi_display *display, int type, int value);

#define MAX_SLEEP_TIME 200
#define to_dsi_display(x) container_of(x, struct dsi_display, host)

extern const char *cmd_set_prop_map[DSI_CMD_SET_MAX];

struct dsi_read_config g_dsi_read_cfg;
/**
* fold_status :  0 - unfold status
*			1 - fold status
*/
int fold_status;

#define NVT_BIC_LENGTH 21060
static char read_result[21060];

static int mi_dsi_update_hbm_cmd_51reg(struct dsi_panel *panel,
			enum dsi_cmd_set_type type, int bl_lvl);

int mi_dsi_panel_init(struct dsi_panel *panel)
{
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	mi_cfg->doze_wakelock = wakeup_source_register(NULL, "doze_wakelock");
	if (!mi_cfg->doze_wakelock) {
		DISP_ERROR("doze_wakelock wake_source register failed");
		return -ENOMEM;
	}
	return 0;
}

int mi_dsi_panel_deinit(struct dsi_panel *panel)
{
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	if (mi_cfg->doze_wakelock) {
		wakeup_source_unregister(mi_cfg->doze_wakelock);
	}
	return 0;
}

int mi_dsi_acquire_wakelock(struct dsi_panel *panel)
{
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	if (mi_cfg->doze_wakelock) {
		__pm_stay_awake(mi_cfg->doze_wakelock);
	}

	return 0;
}

int mi_dsi_release_wakelock(struct dsi_panel *panel)
{
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	if (mi_cfg->doze_wakelock) {
		__pm_relax(mi_cfg->doze_wakelock);
	}

	return 0;

}

bool is_aod_and_panel_initialized(struct dsi_panel *panel)
{
	if ((panel->power_mode == SDE_MODE_DPMS_LP1 ||
		panel->power_mode == SDE_MODE_DPMS_LP2) &&
		dsi_panel_initialized(panel)){
		return true;
	} else {
		return false;
	}
}

bool is_backlight_set_skip(struct dsi_panel *panel, u32 bl_lvl)
{
	if (panel->mi_cfg.in_fod_calibration ||
		panel->mi_cfg.feature_val[DISP_FEATURE_HBM_FOD] == FEATURE_ON) {
		DSI_INFO("%s panel skip set backlight %d due to fod hbm "
				"or fod calibration\n", panel->type, bl_lvl);
		return true;
	} else if (panel->mi_cfg.feature_val[DISP_FEATURE_DC] == FEATURE_ON &&
		bl_lvl < panel->mi_cfg.dc_threshold && bl_lvl != 0 && panel->mi_cfg.dc_type) {
		if (panel->mi_cfg.hbm_brightness_flag &&
			panel->mi_cfg.brightness_clone > (int)(panel->mi_cfg.max_brightness_clone / 2)) {
			return false;
		} else {
			DSI_INFO("%s panel skip set backlight %d due to DC on\n", panel->type, bl_lvl);
			return true;
		}
	} else {
		return false;
	}
}

bool is_hbm_fod_on(struct dsi_panel *panel)
{
	if (panel->mi_cfg.feature_val[DISP_FEATURE_HBM_FOD] == FEATURE_ON ||
		panel->mi_cfg.feature_val[DISP_FEATURE_LOCAL_HBM] == LOCAL_HBM_NORMAL_WHITE_1000NIT ||
		panel->mi_cfg.feature_val[DISP_FEATURE_LOCAL_HBM] == LOCAL_HBM_NORMAL_WHITE_110NIT ||
		panel->mi_cfg.feature_val[DISP_FEATURE_LOCAL_HBM] == LOCAL_HBM_HLPM_WHITE_1000NIT ||
		panel->mi_cfg.feature_val[DISP_FEATURE_LOCAL_HBM] == LOCAL_HBM_HLPM_WHITE_110NIT) {
		return true;
	} else {
		return false;
	}
}

int mi_dsi_panel_esd_irq_ctrl(struct dsi_panel *panel,
				bool enable)
{
	struct mi_dsi_panel_cfg *mi_cfg;
	struct irq_desc *desc;

	if (!panel || !panel->panel_initialized) {
		DISP_ERROR("Panel not ready!\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	mi_cfg = &panel->mi_cfg;
	if (gpio_is_valid(mi_cfg->esd_err_irq_gpio)) {
		if (mi_cfg->esd_err_irq) {
			if (enable) {
				if (!mi_cfg->esd_err_enabled) {
					desc = irq_to_desc(mi_cfg->esd_err_irq);
					if (!irq_settings_is_level(desc))
						desc->istate &= ~IRQS_PENDING;
					enable_irq_wake(mi_cfg->esd_err_irq);
					enable_irq(mi_cfg->esd_err_irq);
					mi_cfg->esd_err_enabled = true;
					DISP_INFO("%s panel esd irq is enable\n", panel->type);
				}
			} else {
				if (mi_cfg->esd_err_enabled) {
					disable_irq_wake(mi_cfg->esd_err_irq);
					disable_irq_nosync(mi_cfg->esd_err_irq);
					mi_cfg->esd_err_enabled = false;
					DISP_INFO("%s panel esd irq is disable\n", panel->type);
				}
			}
		}
	} else {
		DISP_INFO("%s panel esd irq gpio invalid\n", panel->type);
	}

	mutex_unlock(&panel->panel_lock);
	return 0;
}

int mi_dsi_panel_write_cmd_set(struct dsi_panel *panel,
				struct dsi_panel_cmd_set *cmd_sets)
{
	int rc = 0, i = 0;
	ssize_t len;
	struct dsi_cmd_desc *cmds;
	u32 count;
	enum dsi_cmd_set_state state;
	struct dsi_display_mode *mode;
	const struct mipi_dsi_host_ops *ops = panel->host->ops;

	if (!panel || !panel->cur_mode) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mode = panel->cur_mode;

	cmds = cmd_sets->cmds;
	count = cmd_sets->count;
	state = cmd_sets->state;

	if (count == 0) {
		DISP_DEBUG("[%s] No commands to be sent for state\n", panel->type);
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
			DISP_ERROR("failed to set cmds, rc=%d\n", rc);
			goto error;
		}
		if (cmds->post_wait_ms)
			usleep_range(cmds->post_wait_ms * 1000,
					((cmds->post_wait_ms * 1000) + 10));
		cmds++;
	}
error:
	return rc;
}

int mi_dsi_panel_read_cmd_set(struct dsi_panel *panel,
				struct dsi_read_config *read_config)
{
	struct dsi_display *display;
	struct dsi_display_ctrl *ctrl;
	struct dsi_cmd_desc *cmds;
	enum dsi_cmd_set_state state;
	int i, rc = 0, count = 0;
	u32 flags = 0;

	if (!panel || !panel->host || !read_config) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	display = to_dsi_display(panel->host);

	/* Avoid sending DCS commands when ESD recovery is pending */
	if (atomic_read(&display->panel->esd_recovery_pending)) {
		DISP_ERROR("[%s] ESD recovery pending\n", panel->type);
		return 0;
	}

	if (!panel->panel_initialized) {
		DISP_INFO("[%s] Panel not initialized\n", panel->type);
		return -EINVAL;
	}

	if (!read_config->is_read) {
		DISP_INFO("[%s] read operation was not permitted\n", panel->type);
		return -EPERM;
	}

	dsi_display_clk_ctrl(display->dsi_clk_handle,
		DSI_ALL_CLKS, DSI_CLK_ON);

	ctrl = &display->ctrl[display->cmd_master_idx];

	rc = dsi_display_cmd_engine_enable(display);
	if (rc) {
		DISP_ERROR("[%s] cmd engine enable failed\n", panel->type);
		rc = -EPERM;
		goto error_disable_clks;
	}

	if (display->tx_cmd_buf == NULL) {
		rc = dsi_host_alloc_cmd_tx_buffer(display);
		if (rc) {
			DISP_ERROR("[%s] failed to allocate cmd tx buffer\n", panel->type);
			goto error_disable_cmd_engine;
		}
	}

	count = read_config->read_cmd.count;
	cmds = read_config->read_cmd.cmds;
	state = read_config->read_cmd.state;
	if (count == 0) {
		DISP_ERROR("[%s] No commands to be sent\n", panel->type);
		goto error_disable_cmd_engine;
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
		DISP_ERROR("[%s] rx cmd transfer failed rc=%d\n", panel->type, rc);
		goto error_disable_cmd_engine;
	}

	/* for debug log */
	for (i = 0; i < read_config->cmds_rlen; i++)
		DISP_DEBUG("[%d] = 0x%02X\n", i, read_config->rbuf[i]);

error_disable_cmd_engine:
	dsi_display_cmd_engine_disable(display);
error_disable_clks:
	dsi_display_clk_ctrl(display->dsi_clk_handle,
		DSI_ALL_CLKS, DSI_CLK_OFF);

	return rc;
}

int mi_dsi_panel_write_mipi_reg(struct dsi_panel *panel,
				char *buf)
{
	struct dsi_panel_cmd_set cmd_sets = {0};
	int rc = 0, dlen = 0;
	u32 packet_count = 0;
	char *token, *input_copy, *input_dup = NULL;
	const char *delim = " ";
	char *buffer = NULL;
	u32 buf_size = 0;
	u32 tmp_data = 0;

	mutex_lock(&panel->panel_lock);

	if (!panel || !panel->panel_initialized) {
		DISP_ERROR("Panel not initialized!\n");
		rc = -EAGAIN;
		goto exit_unlock;
	}

	DISP_DEBUG("[%s] input buffer:{%s}\n", panel->type, buf);

	input_copy = kstrdup(buf, GFP_KERNEL);
	if (!input_copy) {
		rc = -ENOMEM;
		goto exit_unlock;
	}

	input_dup = input_copy;
	/* removes leading and trailing whitespace from input_copy */
	input_copy = strim(input_copy);

	/* Split a string into token */
	token = strsep(&input_copy, delim);
	if (token) {
		rc = kstrtoint(token, 10, &tmp_data);
		if (rc) {
			DISP_ERROR("input buffer conversion failed\n");
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
		rc = kstrtoint(token, 10, &tmp_data);
		if (rc) {
			DISP_ERROR("input buffer conversion failed\n");
			goto exit_free0;
		}
		if (tmp_data > sizeof(g_dsi_read_cfg.rbuf)) {
			DISP_ERROR("read size exceeding the limit %d\n",
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
		rc = -ENOMEM;
		goto exit_free0;
	}

	token = strsep(&input_copy, delim);
	while (token) {
		rc = kstrtoint(token, 16, &tmp_data);
		if (rc) {
			DISP_ERROR("input buffer conversion failed\n");
			goto exit_free1;
		}
		DISP_DEBUG("buffer[%d] = 0x%02x\n", buf_size, tmp_data);
		buffer[buf_size++] = (tmp_data & 0xff);
		/* Removes leading whitespace from input_copy */
		if (input_copy) {
			input_copy = skip_spaces(input_copy);
			token = strsep(&input_copy, delim);
		} else {
			token = NULL;
		}
	}

	rc = dsi_panel_get_cmd_pkt_count(buffer, buf_size, &packet_count);
	if (!packet_count) {
		DISP_ERROR("get pkt count failed!\n");
		goto exit_free1;
	}

	rc = dsi_panel_alloc_cmd_packets(&cmd_sets, packet_count);
	if (rc) {
		DISP_ERROR("failed to allocate cmd packets, ret=%d\n", rc);
		goto exit_free1;
	}

	rc = dsi_panel_create_cmd_packets(buffer, dlen, packet_count,
						  cmd_sets.cmds);
	if (rc) {
		DISP_ERROR("failed to create cmd packets, ret=%d\n", rc);
		goto exit_free2;
	}

	if (g_dsi_read_cfg.is_read) {
		g_dsi_read_cfg.read_cmd = cmd_sets;
		rc = mi_dsi_panel_read_cmd_set(panel, &g_dsi_read_cfg);
		if (rc <= 0) {
			DISP_ERROR("[%s]failed to read cmds, rc=%d\n", panel->name, rc);
			goto exit_free3;
		}
	} else {
		g_dsi_read_cfg.read_cmd = cmd_sets;
		rc = mi_dsi_panel_write_cmd_set(panel, &cmd_sets);
		if (rc) {
			DISP_ERROR("[%s] failed to send cmds, rc=%d\n", panel->name, rc);
			goto exit_free3;
		}
	}

	DISP_DEBUG("[%s]: done!\n", panel->name);
	rc = 0;

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
	return rc;
}

ssize_t mi_dsi_panel_read_mipi_reg(struct dsi_panel *panel,
			char *buf, size_t size)
{
	int i = 0;
	ssize_t count = 0;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	if (g_dsi_read_cfg.is_read) {
		for (i = 0; i < g_dsi_read_cfg.cmds_rlen; i++) {
			if (i == g_dsi_read_cfg.cmds_rlen - 1) {
				count += snprintf(buf + count, size - count, "0x%02X\n",
				     g_dsi_read_cfg.rbuf[i]);
			} else {
				count += snprintf(buf + count, size - count, "0x%02X,",
				     g_dsi_read_cfg.rbuf[i]);
			}
		}
	}

	mutex_unlock(&panel->panel_lock);

	return count;
}

int mi_dsi_panel_write_dsi_cmd_set(struct dsi_panel *panel,
			int type)
{
	int rc = 0;
	int i = 0, j = 0;
	u8 *tx_buf = NULL;
	u8 *buffer = NULL;
	int buf_size = 1024;
	u32 cmd_count = 0;
	int buf_count = 1024;
	struct dsi_cmd_desc *cmds;
	enum dsi_cmd_set_state state;
	struct dsi_display_mode *mode;

	if (!panel || !panel->cur_mode || type < 0 || type >= DSI_CMD_SET_MAX) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	buffer = kzalloc(buf_size, GFP_KERNEL);
	if (!buffer) {
		return -ENOMEM;
	}

	mutex_lock(&panel->panel_lock);

	mode = panel->cur_mode;
	cmds = mode->priv_info->cmd_sets[type].cmds;
	cmd_count = mode->priv_info->cmd_sets[type].count;
	state = mode->priv_info->cmd_sets[type].state;

	if (cmd_count == 0) {
		DISP_ERROR("[%s] No commands to be sent\n", cmd_set_prop_map[type]);
		rc = -EAGAIN;
		goto error;
	}

	DISP_INFO("set cmds [%s], count (%d), state(%s)\n",
		cmd_set_prop_map[type], cmd_count,
		(state == DSI_CMD_SET_STATE_LP) ? "dsi_lp_mode" : "dsi_hs_mode");

	for (i = 0; i < cmd_count; i++) {
		memset(buffer, 0, buf_size);
		buf_count = snprintf(buffer, buf_size, "%02X", cmds->msg.tx_len);
		tx_buf = (u8 *)cmds->msg.tx_buf;
		for (j = 0; j < cmds->msg.tx_len ; j++) {
			buf_count += snprintf(buffer + buf_count, buf_size - buf_count, " %02X", tx_buf[j]);
		}
		DISP_DEBUG("[%d] %s\n", i, buffer);
		cmds++;
	}

	rc = dsi_panel_tx_cmd_set(panel, type);

error:
	mutex_unlock(&panel->panel_lock);
	kfree(buffer);
	return rc;
}

ssize_t mi_dsi_panel_show_dsi_cmd_set_type(struct dsi_panel *panel,
			char *buf, size_t size)
{
	ssize_t count = 0;
	int type = 0;

	if (!panel || !buf) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	count = snprintf(buf, size, "%s: dsi cmd_set name\n", "id");

	for (type = DSI_CMD_SET_PRE_ON; type < DSI_CMD_SET_MAX; type++) {
		count += snprintf(buf + count, size - count, "%02d: %s\n",
				     type, cmd_set_prop_map[type]);
	}

	return count;
}

static int mi_dsi_panel_read_gamma_otp_and_flash(struct dsi_panel *panel,
				struct dsi_display_ctrl *ctrl)
{
	int rc = 0;
	int retval = 0;
	int i = 0;
	int retry_cnt = 0;
	u32 flags = 0;
	struct dsi_display_mode *mode;
	struct gamma_cfg *gamma_cfg;
	struct dsi_cmd_desc *cmds;
	enum dsi_cmd_set_state state;
	u32 param_index = 0;
	u8 read_param_buf[256] = {0};
	bool checksum_pass = 0;

	if (!panel || !ctrl || !panel->cur_mode || !panel->cur_mode->priv_info) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mode = panel->cur_mode;
	gamma_cfg = &panel->mi_cfg.gamma_cfg;

	/* OTP Read 144hz gamma parameter */
	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_GAMMA_OTP_READ_PRE);
	if (rc) {
		DISP_ERROR("Failed to send DSI_CMD_SET_MI_GAMMA_OTP_READ_PRE command\n");
		retval = -EAGAIN;
		goto error;
	}

	DISP_INFO("[%s] Gamma 0xB8 OPT Read 44 Parameter (144Hz)\n", panel->type);
	flags = 0;
	memset(read_param_buf, 0, sizeof(read_param_buf));
	cmds = mode->priv_info->cmd_sets[DSI_CMD_SET_MI_GAMMA_OTP_READ_B8].cmds;
	state = mode->priv_info->cmd_sets[DSI_CMD_SET_MI_GAMMA_OTP_READ_B8].state;
	if (state == DSI_CMD_SET_STATE_LP)
		cmds->msg.flags |= MIPI_DSI_MSG_USE_LPM;
	if (cmds->last_command) {
		cmds->msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;
		flags |= DSI_CTRL_CMD_LAST_COMMAND;
	}
	flags |= (DSI_CTRL_CMD_FETCH_MEMORY | DSI_CTRL_CMD_READ | DSI_CTRL_CMD_CUSTOM_DMA_SCHED);
	cmds->msg.rx_buf = read_param_buf;
	cmds->msg.rx_len = sizeof(gamma_cfg->otp_read_b8);
	rc = dsi_ctrl_cmd_transfer(ctrl->ctrl, &cmds->msg, &flags);
	if (rc <= 0) {
		DISP_ERROR("Failed to read DSI_CMD_SET_MI_GAMMA_OTP_READ_B8\n");
		retval = -EAGAIN;
		goto error;
	}
	memcpy(gamma_cfg->otp_read_b8, cmds->msg.rx_buf, sizeof(gamma_cfg->otp_read_b8));

	DISP_INFO("[%s] Gamma 0xB9 OPT Read 237 Parameter (144Hz)\n", panel->type);
	flags = 0;
	memset(read_param_buf, 0, sizeof(read_param_buf));
	cmds = mode->priv_info->cmd_sets[DSI_CMD_SET_MI_GAMMA_OTP_READ_B9].cmds;
	state = mode->priv_info->cmd_sets[DSI_CMD_SET_MI_GAMMA_OTP_READ_B9].state;
	if (state == DSI_CMD_SET_STATE_LP)
		cmds->msg.flags |= MIPI_DSI_MSG_USE_LPM;
	if (cmds->last_command) {
		cmds->msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;
		flags |= DSI_CTRL_CMD_LAST_COMMAND;
	}
	flags |= (DSI_CTRL_CMD_FETCH_MEMORY | DSI_CTRL_CMD_READ | DSI_CTRL_CMD_CUSTOM_DMA_SCHED);
	cmds->msg.rx_buf = read_param_buf;
	cmds->msg.rx_len = sizeof(gamma_cfg->otp_read_b9);
	rc = dsi_ctrl_cmd_transfer(ctrl->ctrl, &cmds->msg, &flags);
	if (rc <= 0) {
		DISP_ERROR("Failed to read DSI_CMD_SET_MI_GAMMA_OTP_READ_B9\n");
		retval = -EAGAIN;
		goto error;
	}
	memcpy(gamma_cfg->otp_read_b9, cmds->msg.rx_buf, sizeof(gamma_cfg->otp_read_b9));

	DISP_INFO("[%s] Gamma 0xBA OTP Read 63 Parameter (144Hz)\n", panel->type);
	flags = 0;
	memset(read_param_buf, 0, sizeof(read_param_buf));
	cmds = mode->priv_info->cmd_sets[DSI_CMD_SET_MI_GAMMA_OTP_READ_BA].cmds;
	state = mode->priv_info->cmd_sets[DSI_CMD_SET_MI_GAMMA_OTP_READ_BA].state;
	if (state == DSI_CMD_SET_STATE_LP)
		cmds->msg.flags |= MIPI_DSI_MSG_USE_LPM;
	if (cmds->last_command) {
		cmds->msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;
		flags |= DSI_CTRL_CMD_LAST_COMMAND;
	}
	flags |= (DSI_CTRL_CMD_FETCH_MEMORY | DSI_CTRL_CMD_READ | DSI_CTRL_CMD_CUSTOM_DMA_SCHED);
	cmds->msg.rx_buf = read_param_buf;
	cmds->msg.rx_len = sizeof(gamma_cfg->otp_read_ba);
	rc = dsi_ctrl_cmd_transfer(ctrl->ctrl, &cmds->msg, &flags);
	if (rc <= 0) {
		DISP_ERROR("Failed to read DSI_CMD_SET_MI_GAMMA_OTP_READ_BA\n");
		retval = -EAGAIN;
		goto error;
	}
	memcpy(gamma_cfg->otp_read_ba, cmds->msg.rx_buf, sizeof(gamma_cfg->otp_read_ba));

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_GAMMA_OTP_READ_POST);
	if (rc) {
		pr_err("Failed to send DSI_CMD_SET_MI_GAMMA_OTP_READ_POST command\n");
		retval = -EAGAIN;
		goto error;
	}
	DISP_INFO("[%s] OTP Read 144hz gamma done\n", panel->type);

	/* Flash Read 90hz gamma parameter */
	do {
		gamma_cfg->gamma_checksum = 0;

		if (retry_cnt > 0) {
			DISP_ERROR("Failed to flash read 90hz gamma parameters, retry_cnt = %d\n",
					retry_cnt);
			mdelay(80);
		}

		DISP_INFO("Gamma Flash Read1 200 Parameter (90Hz)\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_GAMMA_FLASH_READ1_PRE);
		if (rc) {
			DISP_ERROR("Failed to send DSI_CMD_SET_MI_GAMMA_FLASH_READ1_PRE command\n");
			retval = -EAGAIN;
			goto error;
		}

		flags = 0;
		memset(read_param_buf, 0, sizeof(read_param_buf));
		cmds = mode->priv_info->cmd_sets[DSI_CMD_SET_MI_GAMMA_FLASH_READ_F6].cmds;
		state = mode->priv_info->cmd_sets[DSI_CMD_SET_MI_GAMMA_FLASH_READ_F6].state;
		if (state == DSI_CMD_SET_STATE_LP)
			cmds->msg.flags |= MIPI_DSI_MSG_USE_LPM;
		if (cmds->last_command) {
			cmds->msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;
			flags |= DSI_CTRL_CMD_LAST_COMMAND;
		}
		flags |= (DSI_CTRL_CMD_FETCH_MEMORY | DSI_CTRL_CMD_READ | DSI_CTRL_CMD_CUSTOM_DMA_SCHED);
		cmds->msg.rx_buf = read_param_buf;
		cmds->msg.rx_len = 200;
		rc = dsi_ctrl_cmd_transfer(ctrl->ctrl, &cmds->msg, &flags);
		if (rc <= 0) {
			DISP_ERROR("Failed to read DSI_CMD_SET_MI_GAMMA_FLASH_READ_F6\n");
			retval = -EAGAIN;
			goto error;
		}
		memcpy(gamma_cfg->flash_gamma_read, cmds->msg.rx_buf, 200);

		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_GAMMA_FLASH_READ_POST);
		if (rc) {
			DISP_ERROR("Failed to send DSI_CMD_SET_MI_GAMMA_FLASH_READ_POST command\n");
			retval = -EAGAIN;
			goto error;
		}

		DISP_INFO("Gamma Flash Read2 146 Parameter (90Hz)\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_GAMMA_FLASH_READ2_PRE);
		if (rc) {
			DISP_ERROR("Failed to send DSI_CMD_SET_MI_GAMMA_FLASH_READ2_PRE command\n");
			retval = -EAGAIN;
			goto error;
		}

		flags = 0;
		memset(read_param_buf, 0, sizeof(read_param_buf));
		cmds = mode->priv_info->cmd_sets[DSI_CMD_SET_MI_GAMMA_FLASH_READ_F6].cmds;
		state = mode->priv_info->cmd_sets[DSI_CMD_SET_MI_GAMMA_FLASH_READ_F6].state;
		if (state == DSI_CMD_SET_STATE_LP)
			cmds->msg.flags |= MIPI_DSI_MSG_USE_LPM;
		if (cmds->last_command) {
			cmds->msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;
			flags |= DSI_CTRL_CMD_LAST_COMMAND;
		}
		flags |= (DSI_CTRL_CMD_FETCH_MEMORY | DSI_CTRL_CMD_READ | DSI_CTRL_CMD_CUSTOM_DMA_SCHED);
		cmds->msg.rx_buf = read_param_buf;
		cmds->msg.rx_len = 146;
		rc = dsi_ctrl_cmd_transfer(ctrl->ctrl, &cmds->msg, &flags);
		if (rc <= 0) {
			DISP_ERROR("Failed to read DSI_CMD_SET_MI_GAMMA_FLASH_READ_F6\n");
			retval = -EAGAIN;
			goto error;
		}
		memcpy(&gamma_cfg->flash_gamma_read[200], cmds->msg.rx_buf, 146);

		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_GAMMA_FLASH_READ_POST);
		if (rc) {
			DISP_ERROR("Failed to send DSI_CMD_SET_MI_GAMMA_FLASH_READ_POST command\n");
			retval = -EAGAIN;
			goto error;
		}

		for(i = 0; i < gamma_cfg->flash_read_total_param; i++)
		{
			if (i < sizeof(gamma_cfg->flash_read_b8)) {
				gamma_cfg->flash_read_b8[i] = gamma_cfg->flash_gamma_read[i];
			}
			else if (i < (sizeof(gamma_cfg->flash_read_b8) +
						sizeof(gamma_cfg->flash_read_b9))) {
				param_index = i - sizeof(gamma_cfg->flash_read_b8);
				gamma_cfg->flash_read_b9[param_index] = gamma_cfg->flash_gamma_read[i];
			}
			else if (i < (sizeof(gamma_cfg->flash_read_b8) +
					sizeof(gamma_cfg->flash_read_b9) +
					sizeof(gamma_cfg->flash_read_ba))) {
				param_index = i - (sizeof(gamma_cfg->flash_read_b8) +
								sizeof(gamma_cfg->flash_read_b9));
				gamma_cfg->flash_read_ba[param_index] = gamma_cfg->flash_gamma_read[i];
			}

			if (i < (gamma_cfg->flash_read_total_param - 2)) {
				gamma_cfg->gamma_checksum = gamma_cfg->flash_gamma_read[i] + gamma_cfg->gamma_checksum;
			} else {
				if (i == (gamma_cfg->flash_read_total_param - 2))
					gamma_cfg->flash_read_checksum[0] = gamma_cfg->flash_gamma_read[i];
				if (i == (gamma_cfg->flash_read_total_param - 1))
					gamma_cfg->flash_read_checksum[1] = gamma_cfg->flash_gamma_read[i];
			}
		}
		if (gamma_cfg->gamma_checksum == ((gamma_cfg->flash_read_checksum[0] << 8)
				+ gamma_cfg->flash_read_checksum[1])) {
			checksum_pass = 1;
			DISP_INFO("[%s] Flash Read 90hz gamma done\n", panel->type);
		} else {
			checksum_pass = 0;
		}

		retry_cnt++;
	}
	while (!checksum_pass && (retry_cnt < 5));

	if (checksum_pass) {
		gamma_cfg->read_done = 1;
		DISP_INFO("[%s] Gamma read done\n", panel->type);
		retval = 0;
	} else {
		DISP_ERROR("[%s] Failed to flash read 90hz gamma\n", panel->type);
		retval = -EAGAIN;
	}

error:
	return retval;

}

int mi_dsi_panel_read_gamma_param(struct dsi_panel *panel)
{
	int rc = 0, ret = 0;
	struct dsi_display *display;
	struct dsi_display_ctrl *ctrl;

	if (!panel || !panel->host) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (!panel->mi_cfg.gamma_update_flag) {
		DISP_DEBUG("[%s] Gamma_update_flag is not configed\n", panel->type);
		return 0;
	}

	display = to_dsi_display(panel->host);
	if (display == NULL)
		return -EINVAL;

	if (!panel->panel_initialized) {
		DISP_ERROR("[%s] Panel not initialized\n", panel->type);
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	if (panel->mi_cfg.gamma_cfg.read_done) {
		DISP_INFO("[%s] Gamma parameter have read and stored at"
			" POWER ON sequence\n", panel->type);
		goto unlock;
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_ON);
	if (rc) {
		DISP_ERROR("[%s] failed to enable DSI clocks, rc=%d\n", display->name, rc);
		goto unlock;
	}

	ctrl = &display->ctrl[display->cmd_master_idx];

	rc = dsi_display_cmd_engine_enable(display);
	if (rc) {
		DISP_ERROR("[%s] failed to enable cmd engine, rc=%d\n",
		       display->name, rc);
		goto error_disable_clks;
	}

	if (display->tx_cmd_buf == NULL) {
		rc = dsi_host_alloc_cmd_tx_buffer(display);
		if (rc) {
			DISP_ERROR("failed to allocate cmd tx buffer memory\n");
			goto error_disable_cmd_engine;
		}
	}

	rc = mi_dsi_panel_read_gamma_otp_and_flash(panel, ctrl);
	if (rc) {
		DISP_ERROR("[%s]failed to get gamma parameter, rc=%d\n",
		       display->name, rc);
		goto error_disable_cmd_engine;
	}

error_disable_cmd_engine:
	ret = dsi_display_cmd_engine_disable(display);
	if (ret) {
		DISP_ERROR("[%s]failed to disable DSI cmd engine, rc=%d\n",
				display->name, ret);
	}
error_disable_clks:
	ret = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);
	if (ret) {
		DISP_ERROR("[%s] failed to disable all DSI clocks, rc=%d\n",
		       display->name, ret);
	}
unlock:
	mutex_unlock(&panel->panel_lock);

	return rc;
}


int mi_dsi_panel_update_gamma_param(struct dsi_panel *panel)
{
	struct dsi_display *display;
	struct dsi_display_mode *mode;
	struct gamma_cfg *gamma_cfg;
	struct dsi_cmd_desc *cmds;
	int total_modes;
	u32 i, count;
	u8 *tx_buf;
	size_t tx_len;
	u32 param_len;
	int rc;

	if (!panel || !panel->host){
		DISP_ERROR("Invalid params\n");
		return -EINVAL;
	}

	display = to_dsi_display(panel->host);
	if (!display)
		return -EINVAL;

	if (!panel->mi_cfg.gamma_update_flag) {
		DISP_DEBUG("[%s] Gamma_update_flag is not configed\n", panel->type);
		return 0;
	}

	gamma_cfg = &panel->mi_cfg.gamma_cfg;
	if (!gamma_cfg->read_done) {
		DISP_ERROR("[%s] Gamma parameter not ready, gamma parameter should be"
			" read and stored at POWER ON sequence\n", panel->type);
		return -EAGAIN;
	}

	if (!display->modes) {
		rc = dsi_display_get_modes(display, &mode);
		if (rc) {
			DISP_ERROR("Failed to get display mode for update gamma parameter\n");
			return rc;
		}
	}

	mutex_lock(&panel->panel_lock);
	total_modes = panel->num_display_modes;
	for (i = 0; i < total_modes; i++) {
		mode = &display->modes[i];
		if (mode && mode->priv_info) {
			if (144 == mode->timing.refresh_rate && !gamma_cfg->update_done_144hz) {
				DISP_INFO("[%s] Update GAMMA Parameter (144Hz)\n", panel->type);
				cmds = mode->priv_info->cmd_sets[DSI_CMD_SET_MI_WRITE_GAMMA].cmds;
				count = mode->priv_info->cmd_sets[DSI_CMD_SET_MI_WRITE_GAMMA].count;
				if (cmds && count >= gamma_cfg->update_b8_index &&
					count >= gamma_cfg->update_b9_index &&
					count >= gamma_cfg->update_ba_index) {
					tx_buf = (u8 *)cmds[gamma_cfg->update_b8_index].msg.tx_buf;
					tx_len = cmds[gamma_cfg->update_b8_index].msg.tx_len;
					param_len = min(sizeof(gamma_cfg->otp_read_b8), tx_len - 1);
					if (tx_buf && tx_buf[0] == 0xB8)
						memcpy(&tx_buf[1], gamma_cfg->otp_read_b8, param_len);
					else
						DISP_ERROR("failed to update gamma 0xB8 parameter\n");

					tx_buf = (u8 *)cmds[gamma_cfg->update_b9_index].msg.tx_buf;
					tx_len = cmds[gamma_cfg->update_b9_index].msg.tx_len;
					param_len = min(sizeof(gamma_cfg->otp_read_b9), tx_len - 1);
					if (tx_buf && tx_buf[0] == 0xB9)
						memcpy(&tx_buf[1], gamma_cfg->otp_read_b9, param_len);
					else
						DISP_ERROR("failed to update gamma 0xB9 parameter\n");

					tx_buf = (u8 *)cmds[gamma_cfg->update_ba_index].msg.tx_buf;
					tx_len = cmds[gamma_cfg->update_ba_index].msg.tx_len;
					param_len = min(sizeof(gamma_cfg->otp_read_ba), tx_len - 1);
					if (tx_buf && tx_buf[0] == 0xBA)
						memcpy(&tx_buf[1], gamma_cfg->otp_read_ba, param_len);
					else
						DISP_ERROR("failed to update gamma 0xBA parameter\n");

					gamma_cfg->update_done_144hz = true;
				} else {
					DISP_ERROR("please check gamma update parameter index configuration\n");
				}
			}
			if (90 == mode->timing.refresh_rate && !gamma_cfg->update_done_90hz) {
				DISP_INFO("[%s] Update GAMMA Parameter (90Hz)\n", panel->type);
				cmds = mode->priv_info->cmd_sets[DSI_CMD_SET_MI_WRITE_GAMMA].cmds;
				count = mode->priv_info->cmd_sets[DSI_CMD_SET_MI_WRITE_GAMMA].count;
				if (cmds && count >= gamma_cfg->update_b8_index &&
					count >= gamma_cfg->update_b9_index &&
					count >= gamma_cfg->update_ba_index) {
					tx_buf = (u8 *)cmds[gamma_cfg->update_b8_index].msg.tx_buf;
					tx_len = cmds[gamma_cfg->update_b8_index].msg.tx_len;
					param_len = min(sizeof(gamma_cfg->flash_read_b8), tx_len - 1);
					if (tx_buf && tx_buf[0] == 0xB8)
						memcpy(&tx_buf[1], gamma_cfg->flash_read_b8, param_len);
					else
						DISP_ERROR("failed to update gamma 0xB8 parameter\n");

					tx_buf = (u8 *)cmds[gamma_cfg->update_b9_index].msg.tx_buf;
					tx_len = cmds[gamma_cfg->update_b9_index].msg.tx_len;
					param_len = min(sizeof(gamma_cfg->flash_read_b9), tx_len - 1);
					if (tx_buf && tx_buf[0] == 0xB9)
						memcpy(&tx_buf[1], gamma_cfg->flash_read_b9, param_len);
					else
						DISP_ERROR("failed to update gamma 0xB9 parameter\n");

					tx_buf = (u8 *)cmds[gamma_cfg->update_ba_index].msg.tx_buf;
					tx_len = cmds[gamma_cfg->update_ba_index].msg.tx_len;
					param_len = min(sizeof(gamma_cfg->flash_read_ba), tx_len - 1);
					if (tx_buf && tx_buf[0] == 0xBA)
						memcpy(&tx_buf[1], gamma_cfg->flash_read_ba, param_len);
					else
						DISP_ERROR("failed to update gamma 0xBA parameter\n");

					gamma_cfg->update_done_90hz = true;
				} else {
					DISP_ERROR("please check gamma update parameter index configuration\n");
				}
			}
		}
	}
	mutex_unlock(&panel->panel_lock);

	return 0;
}

ssize_t mi_dsi_panel_print_gamma_param(struct dsi_panel *panel,
				char *buf, size_t size)
{
	int i = 0;
	ssize_t count = 0;
	struct gamma_cfg *gamma_cfg;
	u8 *buffer = NULL;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (!panel->mi_cfg.gamma_update_flag) {
		DISP_ERROR("gamma_update_flag is not configed\n");
		return -EINVAL;
	}

	gamma_cfg = &panel->mi_cfg.gamma_cfg;

	if (!gamma_cfg->read_done) {
		DISP_INFO("Gamma parameter not read at POWER ON sequence\n");
		return -EAGAIN;
	}

	mutex_lock(&panel->panel_lock);

	count += snprintf(buf + count, size - count,
				"Gamma 0xB8 OPT Read %d Parameter (144Hz)\n",
				sizeof(gamma_cfg->otp_read_b8));
	buffer = gamma_cfg->otp_read_b8;
	for (i = 1; i <= sizeof(gamma_cfg->otp_read_b8); i++) {
		if (i%8 && (i != sizeof(gamma_cfg->otp_read_b8))) {
			count += snprintf(buf + count, size - count, "%02X ",
				gamma_cfg->otp_read_b8[i - 1]);
		} else {
			count += snprintf(buf + count, size - count, "%02X\n",
				gamma_cfg->otp_read_b8[i - 1]);
		}
	}

	count += snprintf(buf + count, size - count,
				"Gamma 0xB9 OPT Read %d Parameter (144Hz)\n",
				sizeof(gamma_cfg->otp_read_b9));
	buffer = gamma_cfg->otp_read_b9;
	for (i = 1; i <= sizeof(gamma_cfg->otp_read_b9); i++) {
		if (i%8 && (i != sizeof(gamma_cfg->otp_read_b9))) {
			count += snprintf(buf + count, size - count, "%02X ",
				gamma_cfg->otp_read_b9[i - 1]);
		} else {
			count += snprintf(buf + count, size - count, "%02X\n",
				gamma_cfg->otp_read_b9[i - 1]);
		}
	}

	count += snprintf(buf + count, size - count,
				"Gamma 0xBA OPT Read %d Parameter (144Hz)\n",
				sizeof(gamma_cfg->otp_read_ba));
	buffer = gamma_cfg->otp_read_ba;
	for (i = 1; i <= sizeof(gamma_cfg->otp_read_ba); i++) {
		if (i%8 && (i != sizeof(gamma_cfg->otp_read_ba))) {
			count += snprintf(buf + count, size - count, "%02X ",
				gamma_cfg->otp_read_ba[i - 1]);
		} else {
			count += snprintf(buf + count, size - count, "%02X\n",
				gamma_cfg->otp_read_ba[i - 1]);
		}
	}

	count += snprintf(buf + count, size - count,
				"Gamma Flash 0xB8 Read %d Parameter (90Hz)\n",
				sizeof(gamma_cfg->flash_read_b8));
	buffer = gamma_cfg->flash_read_b8;
	for (i = 1; i <= sizeof(gamma_cfg->flash_read_b8); i++) {
		if (i%8 && (i != sizeof(gamma_cfg->flash_read_b8))) {
			count += snprintf(buf + count, size - count, "%02X ",
				gamma_cfg->flash_read_b8[i - 1]);
		} else {
			count += snprintf(buf + count, size - count, "%02X\n",
				gamma_cfg->flash_read_b8[i - 1]);
		}
	}

	count += snprintf(buf + count, size - count,
				"Gamma Flash 0xB9 Read %d Parameter (90Hz)\n",
				sizeof(gamma_cfg->flash_read_b9));
	buffer = gamma_cfg->flash_read_b9;
	for (i = 1; i <= sizeof(gamma_cfg->flash_read_b9); i++) {
		if (i%8 && (i != sizeof(gamma_cfg->flash_read_b9))) {
			count += snprintf(buf + count, size - count, "%02X ",
				gamma_cfg->flash_read_b9[i - 1]);
		} else {
			count += snprintf(buf + count, size - count, "%02X\n",
				gamma_cfg->flash_read_b9[i - 1]);
		}
	}

	count += snprintf(buf + count, size - count,
				"Gamma Flash 0xBA Read %d Parameter (90Hz)\n",
				sizeof(gamma_cfg->flash_read_ba));
	buffer = gamma_cfg->flash_read_ba;
	for (i = 1; i <= sizeof(gamma_cfg->flash_read_ba); i++) {
		if (i%8 && (i != sizeof(gamma_cfg->flash_read_ba))) {
			count += snprintf(buf + count, size - count, "%02X ",
				gamma_cfg->flash_read_ba[i - 1]);
		} else {
			count += snprintf(buf + count, size - count, "%02X\n",
				gamma_cfg->flash_read_ba[i - 1]);
		}
	}

	count += snprintf(buf + count, size - count,
				"Gamma Flash Read Checksum Decimal(0x%x,0x%x) (90Hz)\n",
				gamma_cfg->flash_read_checksum[0],
				gamma_cfg->flash_read_checksum[1]);
	count += snprintf(buf + count, size - count,
				"Gamma Flash Read 344 Parameter SUM(0x%x) (90Hz)\n",
				gamma_cfg->gamma_checksum);

	mutex_unlock(&panel->panel_lock);

	return count;
}

int mi_dsi_panel_read_flatmode_param(struct dsi_panel *panel)
{
	int rc = 0;
	struct mi_dsi_panel_cfg *mi_cfg;
	unsigned long mode_flags_backup = 0;
	ssize_t read_len = 0;
	uint request_rx_len = 0;
	u8 read_buf[16] = {0};
	int i = 0;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);
	mi_cfg = &panel->mi_cfg;

	if (!mi_cfg->flatmode_update_flag) {
		DISP_DEBUG("[%s] flatmode_update_flag is not configed\n", panel->type);
		rc = 0;
		goto exit;
	}

	if (!panel->panel_initialized) {
		DISP_ERROR("[%s] Panel not initialized\n", panel->type);
		rc = -EINVAL;
		goto exit;
	}

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_READ_PRE);
	if (rc)
		DSI_ERR("[%s] failed to send DSI_CMD_SET_MI_FLAT_MODE_READ_PRE cmds, rc=%d\n",
				  panel->name, rc);

	mode_flags_backup = panel->mipi_device.mode_flags;
	panel->mipi_device.mode_flags |= MIPI_DSI_MODE_LPM;
	request_rx_len = min(sizeof(mi_cfg->flatmode_cfg.flatmode_param), sizeof(read_buf));
	DISP_INFO("[%s][%s]read flatmode paramter length is %d\n",
		panel->type, panel->name, request_rx_len);
	read_len = mipi_dsi_dcs_read(&panel->mipi_device, 0xB8, read_buf,
		request_rx_len);
	panel->mipi_device.mode_flags = mode_flags_backup;
	if (read_len == request_rx_len) {
		for (i = 0; i < read_len; i++) {
			DISP_INFO("read flatmode_param[%d] = 0x%02x\n",
				i, mi_cfg->flatmode_cfg.flatmode_param[i]);
		}
		memcpy(mi_cfg->flatmode_cfg.flatmode_param, read_buf, read_len);
		mi_cfg->flatmode_cfg.read_done = true;
	} else {
		DISP_INFO("read failed (%d)\n", read_len);
		rc = -EINVAL;
	}
exit:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int mi_dsi_panel_set_doze_brightness(struct dsi_panel *panel,
			u32 doze_brightness)
{
	int rc = 0;
	u32 doze_bl = 0;
	unsigned long mode_flags = 0;
	struct mi_dsi_panel_cfg *mi_cfg;
	struct mipi_dsi_device *dsi = NULL;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	mi_cfg = &panel->mi_cfg;

	if (doze_brightness != DOZE_TO_NORMAL && !is_aod_and_panel_initialized(panel)) {
		DISP_ERROR("Skip! %s panel set doze brightness %d, power mode(%s) initialized(%d)\n",
			panel->type, doze_brightness,
			get_display_power_mode_name(panel->power_mode), panel->panel_initialized);
		mi_cfg->doze_brightness = DOZE_TO_NORMAL;
		goto exit;
	}

	if (is_hbm_fod_on(panel) || mi_cfg->local_hbm_to_normal) {
		DISP_INFO("Skip! %s panel set doze brightness  %d due to fod hbm on\n",
			panel->type, doze_brightness);
		mi_cfg->doze_brightness = doze_brightness;
		if (doze_brightness != DOZE_TO_NORMAL)
			mi_cfg->doze_brightness_backup = doze_brightness;
		goto exit;
	}

	if (mi_cfg->doze_brightness != doze_brightness) {
		if (mi_cfg->aod_bl_51ctl) {
			if (panel->power_mode != SDE_MODE_DPMS_LP1) {
				mi_cfg->doze_brightness = doze_brightness;
				goto exit;
			}

			if (panel->bl_config.bl_inverted_dbv)
				doze_bl = (((doze_brightness & 0xff) << 8) | (doze_brightness >> 8));

			dsi = &panel->mipi_device;

			if (unlikely(panel->bl_config.lp_mode)) {
				mode_flags = dsi->mode_flags;
				dsi->mode_flags |= MIPI_DSI_MODE_LPM;
			}

			rc = mipi_dsi_dcs_set_display_brightness(dsi, (u16)doze_bl);
			if (rc < 0)
				DSI_ERR("%s panel failed to update aod backlight:%d\n",
					panel->type, doze_brightness);

			if (unlikely(panel->bl_config.lp_mode))
				dsi->mode_flags = mode_flags;
		} else {
			if (doze_brightness == DOZE_BRIGHTNESS_HBM) {
				mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_DOZE_HBM, mi_cfg->last_no_zero_bl_level);
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM);
				if (rc) {
					DISP_ERROR("[%s] failed to send DSI_CMD_SET_MI_DOZE_HBM cmd, rc=%d\n",
						panel->name, rc);
				}
				mi_dsi_update_micfg_flags(panel, PANEL_DOZE_HIGH);
				mi_cfg->doze_brightness_backup = DOZE_BRIGHTNESS_HBM;
			} else if (doze_brightness == DOZE_BRIGHTNESS_LBM) {
				mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_DOZE_LBM, mi_cfg->last_no_zero_bl_level);
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM);
				if (rc) {
					DISP_ERROR("[%s] failed to send DSI_CMD_SET_MI_DOZE_LBM cmd, rc=%d\n",
						panel->name, rc);
				}
				mi_dsi_update_micfg_flags(panel, PANEL_DOZE_LOW);
				mi_cfg->doze_brightness_backup = DOZE_BRIGHTNESS_LBM;
			}
		}

		mi_cfg->doze_brightness = doze_brightness;
		mi_cfg->is_doze_to_off = !doze_brightness;

		DISP_UTC_INFO("%s panel set doze brightness to %s\n",
			panel->type, get_doze_brightness_name(doze_brightness));
	} else {
		DISP_INFO("%s panel %s has been set, skip\n", panel->type,
			get_doze_brightness_name(doze_brightness));
	}

exit:
	mutex_unlock(&panel->panel_lock);

	return rc;
}

static int mi_dsi_panel_restore_doze_brightness(struct dsi_panel *panel)
{
	int rc = 0;
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	if (mi_cfg->doze_brightness == DOZE_BRIGHTNESS_HBM) {
		mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_DOZE_HBM, mi_cfg->last_no_zero_bl_level);
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM);
		if (rc) {
			DISP_ERROR("[%s] failed to send DSI_CMD_SET_MI_DOZE_HBM cmd, rc=%d\n",
				panel->name, rc);
		}
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
	} else if (mi_cfg->doze_brightness == DOZE_BRIGHTNESS_LBM) {
		mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_DOZE_LBM, mi_cfg->last_no_zero_bl_level);
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM);
		if (rc) {
			DISP_ERROR("[%s] failed to send DSI_CMD_SET_MI_DOZE_LBM cmd, rc=%d\n",
				panel->name, rc);
		}
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
	}

	DISP_UTC_INFO("%s panel restore doze brightness to %s\n",
		panel->type, get_doze_brightness_name(mi_cfg->doze_brightness));

	return rc;
}

int mi_dsi_panel_get_doze_brightness(struct dsi_panel *panel,
			u32 *doze_brightness)
{
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	mi_cfg = &panel->mi_cfg;
	*doze_brightness =  mi_cfg->doze_brightness;

	mutex_unlock(&panel->panel_lock);

	return 0;
}

int mi_dsi_panel_get_brightness(struct dsi_panel *panel,
			u32 *brightness)
{
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	mi_cfg = &panel->mi_cfg;
	*brightness =  mi_cfg->last_bl_level;

	mutex_unlock(&panel->panel_lock);

	return 0;
}

int mi_dsi_panel_write_dsi_cmd(struct dsi_panel *panel,
			struct dsi_cmd_rw_ctl *ctl)
{
	struct dsi_panel_cmd_set cmd_sets = {0};
	u32 packet_count = 0;
	u32 dlen = 0;
	int rc = 0;

	mutex_lock(&panel->panel_lock);

	if (!panel || !panel->panel_initialized) {
		DISP_ERROR("Panel not initialized!\n");
		rc = -EAGAIN;
		goto exit_unlock;
	}

	if (!ctl->tx_len || !ctl->tx_ptr) {
		DISP_ERROR("%s panel invalid params\n", panel->type);
		rc = -EINVAL;
		goto exit_unlock;
	}

	rc = dsi_panel_get_cmd_pkt_count(ctl->tx_ptr, ctl->tx_len, &packet_count);
	if (rc) {
		DISP_ERROR("%s panel write dsi commands failed, rc=%d\n",
			panel->type, rc);
		goto exit_unlock;
	}

	DISP_DEBUG("%s panel packet-count=%d\n", panel->type, packet_count);

	rc = dsi_panel_alloc_cmd_packets(&cmd_sets, packet_count);
	if (rc) {
		DISP_ERROR("%s panel failed to allocate cmd packets, rc=%d\n",
			panel->type, rc);
		goto exit_unlock;
	}

	rc = dsi_panel_create_cmd_packets(ctl->tx_ptr, dlen, packet_count,
						cmd_sets.cmds);
	if (rc) {
		DISP_ERROR("%s panel failed to create cmd packets, rc=%d\n",
			panel->type, rc);
		goto exit_free1;
	}

	if (ctl->tx_state == MI_DSI_CMD_LP_STATE) {
		cmd_sets.state = DSI_CMD_SET_STATE_LP;
	} else if (ctl->tx_state == MI_DSI_CMD_HS_STATE) {
		cmd_sets.state = DSI_CMD_SET_STATE_HS;
	} else {
		DISP_ERROR("%s panel command state unrecognized-%s\n",
			panel->type, cmd_sets.state);
		goto exit_free1;
	}

	rc = mi_dsi_panel_write_cmd_set(panel, &cmd_sets);
	if (rc) {
		DISP_ERROR("%s panel [%s] failed to send cmds, rc=%d\n",
			panel->type, panel->name, rc);
		goto exit_free2;
	}

exit_free2:
	if (ctl->tx_len && ctl->tx_ptr)
		dsi_panel_destroy_cmd_packets(&cmd_sets);
exit_free1:
	if (ctl->tx_len && ctl->tx_ptr)
		dsi_panel_dealloc_cmd_packets(&cmd_sets);
exit_unlock:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int mi_dsi_panel_set_brightness_clone(struct dsi_panel *panel,
			u32 brightness_clone)
{
	int rc = 0;
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	mi_cfg = &panel->mi_cfg;
	mi_cfg->brightness_clone = brightness_clone;
	DISP_UTC_INFO("%s panel set brightness clone to %d\n",
			panel->type, brightness_clone);

	mutex_unlock(&panel->panel_lock);

	return rc;
}

int mi_dsi_panel_get_brightness_clone(struct dsi_panel *panel,
			u32 *brightness_clone)
{
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	mi_cfg = &panel->mi_cfg;
	*brightness_clone =  mi_cfg->brightness_clone;

	mutex_unlock(&panel->panel_lock);

	return 0;
}

int mi_dsi_panel_nolp(struct dsi_panel *panel)
{
	struct mi_dsi_panel_cfg *mi_cfg;
	int rc = 0;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	if (mi_cfg->dfps_bl_ctrl) {
		if (mi_cfg->last_bl_level < mi_cfg->dfps_bl_threshold) {
			if (mi_cfg->feature_val[DISP_FEATURE_DC] == FEATURE_ON)
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_NOLP_DC_LBM);
			else
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_NOLP);
		} else {
			if (mi_cfg->feature_val[DISP_FEATURE_DC] == FEATURE_ON)
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_NOLP_DC_HBM);
			else
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_NOLP);
		}

		if (rc)
			DISP_INFO("[%s] failed to send DSI_CMD_SET_NOLP cmd, rc=%d\n",
				panel->name, rc);

		return rc;
	}

	switch (mi_cfg->doze_brightness_backup) {
	case DOZE_BRIGHTNESS_HBM:
		DISP_INFO("DOZE HBM NOLP\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM_NOLP);
		if (rc) {
			DISP_ERROR("[%s] failed to send DSI_CMD_SET_MI_DOZE_HBM_NOLP cmd, rc=%d\n",
				panel->name, rc);
		}
		mi_cfg->doze_brightness_backup = DOZE_TO_NORMAL;
		break;
	case DOZE_BRIGHTNESS_LBM:
		DISP_INFO("DOZE LBM NOLP\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM_NOLP);
		if (rc) {
			DISP_ERROR("[%s] failed to send DSI_CMD_SET_MI_DOZE_LBM_NOLP cmd, rc=%d\n",
				panel->name, rc);
		}
		mi_cfg->doze_brightness_backup = DOZE_TO_NORMAL;
		break;
	default:
		break;
	}

	return rc;
}

static void mi_disp_set_dimming_delayed_work_handler(struct kthread_work *work)
{
	struct disp_work *cur_work = container_of(work,
					struct disp_work, delayed_work.work);
	struct dsi_panel *panel = (struct dsi_panel *)(cur_work->data);
	struct disp_feature_ctl ctl;

	ctl.feature_id = DISP_FEATURE_DIMMING;
	ctl.feature_val = FEATURE_ON;

	mi_dsi_panel_set_disp_param(panel, &ctl);

	kfree(cur_work);
}

static int mi_disp_set_dimming_queue_delayed_work(struct disp_display *dd_ptr,
			struct dsi_panel *panel)
{
	struct disp_work *cur_work;

	cur_work = kzalloc(sizeof(*cur_work), GFP_ATOMIC);
	if (!cur_work)
		return -ENOMEM;

	kthread_init_delayed_work(&cur_work->delayed_work, mi_disp_set_dimming_delayed_work_handler);
	cur_work->dd_ptr = dd_ptr;
	cur_work->wq = &dd_ptr->pending_wq;
	cur_work->data = panel;

	kthread_queue_delayed_work(&dd_ptr->d_thread.worker, &cur_work->delayed_work,
			msecs_to_jiffies(panel->mi_cfg.panel_on_dimming_delay));

	return 0;
}

int mi_dsi_panel_update_dc_status(struct dsi_panel *panel, int brightness)
{
	struct mi_dsi_panel_cfg *mi_cfg;
	int rc = 0;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -1;
	}

	mi_cfg = &panel->mi_cfg;

	if (mi_cfg->fod_type == 0) {
		return rc;
	}
	if (brightness > 0 && mi_cfg->last_bl_level == 0 && mi_cfg->dc_type) {
		DISP_INFO("%s panel CRC off\n", panel->type);
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CRC_OFF);
	}
	if (brightness == 0 && mi_cfg->dc_type) {
		DISP_INFO("%s panel DC off\n", panel->type);
		mi_cfg->feature_val[DISP_FEATURE_DC] = FEATURE_OFF;
	}
	return rc;
}

void mi_dsi_panel_update_last_bl_level(struct dsi_panel *panel, int brightness)
{
	struct mi_dsi_panel_cfg *mi_cfg;
	struct disp_feature *df = mi_get_disp_feature();
	struct dsi_display *display;
	int disp_id = 0;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return;
	}

	display = to_dsi_display(panel->host);
	mi_cfg = &panel->mi_cfg;

	if ((mi_cfg->last_bl_level == 0 || mi_cfg->dimming_state == STATE_DIM_RESTORE) &&
		brightness > 0) {
		disp_id = mi_get_disp_id(display);
		mi_disp_set_dimming_queue_delayed_work(&df->d_display[disp_id], panel);

		if (mi_cfg->dimming_state == STATE_DIM_RESTORE)
			mi_cfg->dimming_state = STATE_NONE;
	}

	mi_cfg->last_bl_level = brightness;
	if (brightness != 0) {
		mi_cfg->last_no_zero_bl_level = brightness;
		DISP_INFO("mi_cfg->last_no_zero_bl_level = %d\n", brightness);
	}

	return;
}

void mi_dsi_update_micfg_flags(struct dsi_panel *panel, int power_mode)
{
	struct mi_dsi_panel_cfg *mi_cfg  = NULL;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return;
	}

	mi_cfg = &panel->mi_cfg;

	switch (power_mode) {
	case PANEL_OFF:
		mi_cfg->in_fod_calibration = false;
		mi_cfg->fod_hbm_layer_enabled = false;
		mi_cfg->fod_anim_layer_enabled = false;
		mi_cfg->dimming_state = STATE_NONE;
		mi_cfg->panel_state = PANEL_STATE_OFF;
		mi_cfg->local_hbm_to_normal = false;
		mi_cfg->feature_val[DISP_FEATURE_COLOR_INVERT] = FEATURE_OFF;
		mi_cfg->feature_val[DISP_FEATURE_HBM] = FEATURE_OFF;
		break;
	case PANEL_ON:
		mi_cfg->in_fod_calibration = false;
		mi_cfg->fod_hbm_layer_enabled = false;
		mi_cfg->fod_anim_layer_enabled = false;
		mi_cfg->dimming_state = STATE_NONE;
		mi_cfg->panel_state = PANEL_STATE_ON;
		mi_cfg->local_hbm_to_normal = false;
		break;
	case PANEL_NOLP:
		mi_cfg->dimming_state = STATE_DIM_RESTORE;
		mi_cfg->panel_state = PANEL_STATE_ON;
		break;
	case PANEL_DOZE_HIGH:
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		mi_cfg->panel_state = PANEL_STATE_DOZE_HIGH;
		break;
	case PANEL_DOZE_LOW:
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		mi_cfg->panel_state = PANEL_STATE_DOZE_LOW;
		break;
	case PANEL_LP1:
	case PANEL_LP2:
	default:
		break;
	}

	return;
}

int mi_dsi_panel_demura_set(struct dsi_panel *panel)
{
	int rc = 0;
	struct mi_dsi_panel_cfg *mi_cfg  = NULL;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	if (mi_cfg->panel_id == 0x4B3800420200 && mi_cfg->feature_val[DISP_FEATURE_DC] == FEATURE_ON) {
		DISP_INFO("DC is on, current refresh_rate is %d, DSI_CMD_SET_MI_DEMURA_WHEN_DC_ON\n",
			panel->cur_mode->timing.refresh_rate);
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DEMURA_WHEN_DC_ON);
	} else if (mi_cfg->panel_id == 0x4B3800420200 && mi_cfg->feature_val[DISP_FEATURE_DC] == FEATURE_OFF) {
		DISP_INFO("DC is off, current refresh_rate is %d, DSI_CMD_SET_MI_DEMURA_WHEN_DC_OFF\n",
			panel->cur_mode->timing.refresh_rate);
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DEMURA_WHEN_DC_OFF);
	}

	return rc;
}

void mi_dsi_dc_mode_enable(struct dsi_panel *panel,
			bool enable)
{
	struct mi_dsi_panel_cfg *mi_cfg  = NULL;

	if (!panel && !panel->cur_mode) {
		DISP_ERROR("invalid params\n");
		return;
	}

	mi_cfg = &panel->mi_cfg;

	if (panel->mi_cfg.dfps_bl_ctrl) {
		if (mi_cfg->dc_type == 0 && enable) {
			if (mi_cfg->dc_threshold > 0 && mi_cfg->last_bl_level > mi_cfg->dc_threshold)
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DC_HBM_ON);
			else
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DC_ON);
		} else if (mi_cfg->dc_type == 0) {
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DC_OFF);
			mi_cfg->demura_mask = 0;
			mi_dsi_panel_demura_comp(panel, mi_cfg->last_bl_level);
		}
	} else {
		if (mi_cfg->dc_type == 0 && enable) {
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DC_ON);
		} else if (mi_cfg->dc_type == 0) {
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DC_OFF);
		}
		mi_dsi_panel_demura_set(panel);
	}
}

int mi_dsi_fps_switch(struct dsi_panel *panel)
{
	int rc = 0;
	struct mi_dsi_panel_cfg *mi_cfg  = NULL;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	mi_dsi_panel_demura_set(panel);
	if (!panel->mi_cfg.dfps_bl_ctrl)
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_TIMING_SWITCH);

	if (panel->mi_cfg.dfps_bl_ctrl) {
		if (mi_cfg->feature_val[DISP_FEATURE_DC] == FEATURE_ON
			&& mi_cfg->last_bl_level <= mi_cfg->dc_threshold)
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_TIMING_SWITCH_DC_LBM);
		else if (mi_cfg->feature_val[DISP_FEATURE_DC] == FEATURE_ON
			&& mi_cfg->last_bl_level > mi_cfg->dc_threshold)
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_TIMING_SWITCH_DC_HBM);
		else if (mi_cfg->last_bl_level <mi_cfg->dfps_bl_threshold) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_TIMING_SWITCH);
			mi_cfg->demura_mask = 0;
			mi_dsi_panel_demura_comp(panel, mi_cfg->last_bl_level);
		} else {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_TIMING_SWITCH);
			mi_cfg->demura_mask = 0;
			mi_dsi_panel_demura_comp(panel, mi_cfg->last_bl_level);
		}
	}
	return rc;
}

int mi_dsi_set_bic_reg(struct dsi_panel *panel)
{
	struct dsi_cmd_desc *cmds = NULL;
	u32 count;
	u8 *tx_buf;
	int i = 0;
	int rc = 0;

	if (panel->mi_cfg.nvt_bic_enabled && panel->mi_cfg.nvt_bic_reg_transfer_finshed) {
		cmds = panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_POST_ON].cmds;
		count = panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_POST_ON].count;
		if (cmds && count >= panel->mi_cfg.nvt_bic_post_on_d0_index
			&& panel->mi_cfg.nvt_bic_post_on_d0_index >= 0) {
			tx_buf = (u8 *)cmds[panel->mi_cfg.nvt_bic_post_on_d0_index].msg.tx_buf;
			if (tx_buf && tx_buf[0] == 0xD0) {
				for (i =1; i <= sizeof(panel->mi_cfg.bic_reg_data); i++)
					tx_buf[i] = panel->mi_cfg.bic_reg_data[i-1];
				DISP_INFO("enter DSI_CMD_SET_POST_ON\n");
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_POST_ON);
			}
		}
	}

	return rc;
}

static int mi_dsi_update_hbm_cmd_87reg(struct dsi_panel *panel,
			enum dsi_cmd_set_type type, int bl_lvl)
{
	struct dsi_display_mode_priv_info *priv_info;
	struct dsi_cmd_desc *cmds = NULL;
	struct mi_dsi_panel_cfg *mi_cfg  = NULL;
	u32 count;
	int index;
	u8 *tx_buf;
	int rc = 0;

	if (!panel || !panel->cur_mode || !panel->cur_mode->priv_info) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;
	priv_info = panel->cur_mode->priv_info;

	switch (type) {
		case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT:
		case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_750NIT:
		case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_500NIT:
		case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT:
		case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_GREEN_500NIT:
			index = mi_cfg->local_hbm_on_87_index;
			break;
		case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT:
		case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT:
			index = mi_cfg->local_hbm_hlpm_on_87_index;
			break;
		default:
			DISP_ERROR("%s panel wrong cmd type!\n", panel->type);
			return -EINVAL;
	}

	if (index < 0) {
		DISP_DEBUG("%s panel cmd[%s] update not supported\n",
			panel->type, cmd_set_prop_map[type]);
			return 0;
	}

	DISP_INFO("cmd[%s], bl_lvl=%d\n", cmd_set_prop_map[type], bl_lvl);

	/* update lhbm aa area alpha */
	cmds = priv_info->cmd_sets[type].cmds;
	count = priv_info->cmd_sets[type].count;
	if (cmds && count >= index) {
		tx_buf = (u8 *)cmds[index].msg.tx_buf;
		if (tx_buf && tx_buf[0] == 0x87) {
			tx_buf[1] = (aa_alpha_set[bl_lvl] >> 8) & 0x0f;
			tx_buf[2] = aa_alpha_set[bl_lvl] & 0xff;
			DISP_INFO("panel aa cmd[0x%02x] = 0x%02x 0x%02x\n", tx_buf[0], tx_buf[1], tx_buf[2]);
		} else {
			if (tx_buf) {
				DISP_ERROR("%s panel aa index = %d, tx_buf[0] = 0x%02X, check cmd[%s] index\n",
					panel->type, index, tx_buf[0], cmd_set_prop_map[type]);
			} else {
				DISP_ERROR("%s panel tx_buf is NULL pointer\n", panel->type);
			}
			rc = -EINVAL;
		}
	} else {
		DISP_ERROR("%s panel aa cmd[%s] 0x87 index(%d) error\n",
			panel->type, cmd_set_prop_map[type], index);
		rc = -EINVAL;
	}

	/* update lhbm cup area alpha */
	cmds = priv_info->cmd_sets[type].cmds;
	count = priv_info->cmd_sets[type].count;
	if (cmds && count >= (index + 2)) {
		tx_buf = (u8 *)cmds[index + 2].msg.tx_buf;
		if (tx_buf && tx_buf[0] == 0x87) {
			tx_buf[1] = (cup_alpha_set[bl_lvl] >> 8) & 0x0f;
			tx_buf[2] = cup_alpha_set[bl_lvl] & 0xff;
			DISP_INFO("panel cup cmd[0x%02x] = 0x%02x 0x%02x\n", tx_buf[0], tx_buf[1], tx_buf[2]);
		} else {
			if (tx_buf) {
				DISP_ERROR("%s panel cup index = %d, tx_buf[0] = 0x%02X, check cmd[%s] index\n",
					panel->type, index, tx_buf[0], cmd_set_prop_map[type]);
			} else {
				DISP_ERROR("%s panel tx_buf is NULL pointer\n", panel->type);
			}
			rc = -EINVAL;
		}
	} else {
		DISP_ERROR("%s panel cup cmd[%s] 0x87 index(%d) error\n",
			panel->type, cmd_set_prop_map[type], index);
		rc = -EINVAL;
	}

	return rc;
}

static int mi_dsi_update_hbm_cmd_51reg(struct dsi_panel *panel,
			enum dsi_cmd_set_type type, int bl_lvl)
{
	struct dsi_display_mode_priv_info *priv_info;
	struct dsi_cmd_desc *cmds = NULL;
	struct mi_dsi_panel_cfg *mi_cfg  = NULL;
	u32 count;
	int index;
	u8 *tx_buf;
	int rc = 0;

	if (!panel || !panel->cur_mode || !panel->cur_mode->priv_info) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;
	priv_info = panel->cur_mode->priv_info;

	switch (type) {
	case DSI_CMD_SET_MI_HBM_ON:
		index = mi_cfg->hbm_on_51_index;
		break;
	case DSI_CMD_SET_MI_HBM_OFF:
		index = mi_cfg->hbm_off_51_index;
		break;
	case DSI_CMD_SET_MI_HBM_FOD_ON:
		index = mi_cfg->hbm_fod_on_51_index;
		break;
	case DSI_CMD_SET_MI_HBM_FOD_OFF:
		index = mi_cfg->hbm_fod_off_51_index;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT:
		index = mi_cfg->local_hbm_on_1000nit_51_index;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_HBM:
		index = mi_cfg->local_hbm_off_to_hbm_51_index;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL:
		index = mi_cfg->local_hbm_off_to_normal_51_index;
		break;
	case DSI_CMD_SET_MI_DOZE_HBM:
		index = mi_cfg->aod_hbm_51_index;
		break;
	case DSI_CMD_SET_MI_DOZE_LBM:
		index = mi_cfg->aod_lbm_51_index;
		break;
	default:
		DISP_ERROR("%s panel wrong cmd type!\n", panel->type);
		return -EINVAL;
	}

	if (index < 0) {
		DISP_DEBUG("%s panel cmd[%s] 0x51 update not supported\n",
				panel->type, cmd_set_prop_map[type]);
		return 0;
	}

	DISP_INFO("cmd[%s], bl_lvl=%d\n", cmd_set_prop_map[type], bl_lvl);

	/* restore last backlight value when hbm off */
	cmds = priv_info->cmd_sets[type].cmds;
	count = priv_info->cmd_sets[type].count;
	if (cmds && count >= index) {
		tx_buf = (u8 *)cmds[index].msg.tx_buf;
		if (tx_buf && tx_buf[0] == 0x51) {
			tx_buf[1] = (bl_lvl >> 8) & 0x0f;
			tx_buf[2] = bl_lvl & 0xff;
		} else {
			if (tx_buf) {
				DISP_ERROR("%s panel tx_buf[0] = 0x%02X, check cmd[%s] 0x51 index\n",
					panel->type, tx_buf[0], cmd_set_prop_map[type]);
			} else {
				DISP_ERROR("%s panel tx_buf is NULL pointer\n", panel->type);
			}
			rc = -EINVAL;
		}
	} else {
		DISP_ERROR("%s panel cmd[%s] 0x51 index(%d) error\n",
			panel->type, cmd_set_prop_map[type], index);
		rc = -EINVAL;
	}

	return rc;
}

bool mi_dsi_panel_is_need_tx_cmd(u32 feature_id)
{
	switch (feature_id) {
	case DISP_FEATURE_SENSOR_LUX:
	case DISP_FEATURE_LOW_BRIGHTNESS_FOD:
	case DISP_FEATURE_FP_STATUS:
	case DISP_FEATURE_FOLD_STATUS:
		return false;
	default:
		return true;
	}
}

static int mi_dsi_update_flatmode_cmd(struct dsi_panel *panel,
		enum dsi_cmd_set_type type)
{
	int rc = 0;
	struct dsi_display_mode_priv_info *priv_info;
	struct dsi_cmd_desc *cmds = NULL;
	struct flatmode_cfg *flatmode_cfg  = NULL;
	u32 count;
	int index;
	u8 *tx_buf;
	size_t tx_len;
	u32 param_len;

	if (!panel->mi_cfg.flatmode_update_flag || !panel->mi_cfg.flatmode_cfg.read_done) {
		DISP_ERROR("please check flatmode params update\n");
		return -EINVAL;
	}

	if (!panel || !panel->cur_mode || !panel->cur_mode->priv_info) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;

	}

	flatmode_cfg = &panel->mi_cfg.flatmode_cfg;
	priv_info = panel->cur_mode->priv_info;
	index = flatmode_cfg->update_index;
	if (index < 0) {
		DISP_INFO("%s panel cmd[%s] update not supported\n",
				panel->type, cmd_set_prop_map[type]);
		return 0;
	}

	cmds = priv_info->cmd_sets[type].cmds;
	count = priv_info->cmd_sets[type].count;
	if (cmds && count >= index) {
		tx_buf = (u8 *)cmds[index].msg.tx_buf;
		if (tx_buf && tx_buf[0] == 0xB9) {
			tx_len = cmds[index].msg.tx_len;
			param_len = min(sizeof(flatmode_cfg->flatmode_param), tx_len - 1);
			memcpy(&tx_buf[1], flatmode_cfg->flatmode_param, param_len);
		} else {
			if (tx_buf) {
				DISP_ERROR("%s panel tx_buf[0] = 0x%02X, check cmd[%s]\n",
					panel->type, tx_buf[0], cmd_set_prop_map[type]);
			} else {
				DISP_ERROR("%s panel tx_buf is NULL pointer\n", panel->type);
			}
			rc = -EINVAL;
		}
	} else {
		DISP_ERROR("%s panel cmd[%s] index(%d) error\n",
			panel->type, cmd_set_prop_map[type], index);
		rc = -EINVAL;
	}

	return rc;
}


int mi_dsi_panel_set_disp_param(struct dsi_panel *panel, struct disp_feature_ctl *ctl)
{
	int rc = 0;
	struct mi_dsi_panel_cfg *mi_cfg  = NULL;

	if (!panel || !ctl) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	DISP_UTC_INFO("%s panel feature: %s, value: %d\n", panel->type,
		get_disp_feature_id_name(ctl->feature_id), ctl->feature_val);

	if (!panel->panel_initialized &&
			ctl->feature_id != DISP_FEATURE_SENSOR_LUX &&
			ctl->feature_id != DISP_FEATURE_LOW_BRIGHTNESS_FOD &&
			ctl->feature_id != DISP_FEATURE_FOLD_STATUS &&
			ctl->feature_id != DISP_FEATURE_FP_STATUS) {
		DISP_ERROR("Panel not initialized!\n");
		rc = -ENODEV;
		goto exit;
	}

	mi_cfg = &panel->mi_cfg;

	switch (ctl->feature_id) {
	case DISP_FEATURE_DIMMING:
		if (mi_cfg->dimming_state != STATE_DIM_BLOCK) {
			if (ctl->feature_val == FEATURE_ON)
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGON);
			else
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGOFF);
			mi_cfg->feature_val[DISP_FEATURE_DIMMING] = ctl->feature_val;
		} else
			DISP_INFO("skip dimming %s\n", ctl->feature_val ? "on" : "off");
		break;
	case DISP_FEATURE_HBM:
		if (ctl->feature_val == FEATURE_ON) {
			if (mi_cfg->feature_val[DISP_FEATURE_HBM_FOD] != FEATURE_ON) {
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_ON);
				mi_cfg->dimming_state = STATE_DIM_BLOCK;
			}
		} else {
			if (mi_cfg->feature_val[DISP_FEATURE_HBM_FOD] != FEATURE_ON) {
				if (mi_cfg->hbm_51_ctl_flag && mi_cfg->hbm_off_51_index >= 0)
					mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_HBM_OFF, mi_cfg->last_bl_level);
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_OFF);
				mi_cfg->dimming_state = STATE_DIM_RESTORE;
			}
		}
		mi_cfg->feature_val[DISP_FEATURE_HBM] = ctl->feature_val;
		break;
	case DISP_FEATURE_HBM_FOD:
		if (ctl->feature_val == FEATURE_ON) {
			if (mi_cfg->hbm_51_ctl_flag && mi_cfg->hbm_fod_on_51_index >= 0)
				mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_HBM_FOD_ON, mi_cfg->hbm_fod_bl_lvl);
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_FOD_ON);
			if (mi_cfg->dc_type && mi_cfg->feature_val[DISP_FEATURE_DC] == FEATURE_ON) {
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CRC_OFF);
			}
			mi_cfg->dimming_state = STATE_DIM_BLOCK;
		} else {
			if (mi_cfg->feature_val[DISP_FEATURE_HBM] != FEATURE_ON) {
				if (mi_cfg->hbm_51_ctl_flag && mi_cfg->hbm_fod_off_51_index >= 0)
					mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_HBM_FOD_OFF, mi_cfg->last_bl_level);
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_FOD_OFF);
				if (mi_cfg->doze_brightness) {
					mi_dsi_panel_restore_doze_brightness(panel);
				} else
					mi_cfg->dimming_state = STATE_DIM_RESTORE;
			} else {
				/* restore outdoor hbm after fod hbm off */
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_ON);
			}
		}
		mi_cfg->feature_val[DISP_FEATURE_HBM_FOD] = ctl->feature_val;
		break;
	case DISP_FEATURE_DOZE_BRIGHTNESS:
#ifdef CONFIG_FACTORY_BUILD
		if (dsi_panel_initialized(panel) &&
			is_aod_brightness(ctl->feature_val)) {
			if (ctl->feature_val == DOZE_BRIGHTNESS_HBM) {
				mi_cfg->doze_brightness = DOZE_BRIGHTNESS_HBM;
				mi_cfg->doze_brightness_backup = DOZE_BRIGHTNESS_HBM;
				mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_DOZE_HBM, mi_cfg->last_no_zero_bl_level);
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM);
			} else {
				mi_cfg->doze_brightness = DOZE_BRIGHTNESS_LBM;
				mi_cfg->doze_brightness_backup = DOZE_BRIGHTNESS_LBM;
				mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_DOZE_LBM, mi_cfg->last_no_zero_bl_level);
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM);
			}
			mi_cfg->dimming_state = STATE_DIM_BLOCK;
		} else {
			if (mi_cfg->aod_nolp_command_enabled)
				mi_dsi_panel_nolp(panel);
			else
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_NOLP);
			mi_cfg->doze_brightness = DOZE_TO_NORMAL;
			mi_cfg->doze_brightness_backup = DOZE_TO_NORMAL;
			mi_cfg->dimming_state = STATE_DIM_RESTORE;
		}
#else
		if (is_aod_and_panel_initialized(panel) &&
			is_aod_brightness(ctl->feature_val)) {
			if (ctl->feature_val == DOZE_BRIGHTNESS_HBM) {
				mi_cfg->doze_brightness = DOZE_BRIGHTNESS_HBM;
				mi_cfg->doze_brightness_backup = DOZE_BRIGHTNESS_HBM;
				mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_DOZE_HBM, mi_cfg->doze_hbm_dbv_level);
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM);
			} else {
				mi_cfg->doze_brightness = DOZE_BRIGHTNESS_LBM;
				mi_cfg->doze_brightness_backup = DOZE_BRIGHTNESS_LBM;
				mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_DOZE_LBM, mi_cfg->doze_lbm_dbv_level);
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM);
			}
			mi_cfg->dimming_state = STATE_DIM_BLOCK;
		} else {
			if (mi_cfg->aod_nolp_command_enabled)
				mi_dsi_panel_nolp(panel);
			else
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_NOLP);
			mi_cfg->doze_brightness = DOZE_TO_NORMAL;
			mi_cfg->doze_brightness_backup = DOZE_TO_NORMAL;
			mi_cfg->dimming_state = STATE_DIM_RESTORE;
		}
#endif
		mi_cfg->feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] = ctl->feature_val;
		break;
	case DISP_FEATURE_FOD_CALIBRATION_BRIGHTNESS:
		if (ctl->feature_val == -1) {
			DISP_INFO("FOD calibration brightness restore last_bl_level=%d\n",
				mi_cfg->last_bl_level);
			dsi_panel_update_backlight(panel, mi_cfg->last_bl_level);
			mi_cfg->in_fod_calibration = false;
		} else {
			if (ctl->feature_val >= 0 && ctl->feature_val <= panel->bl_config.bl_max_level) {
				mi_cfg->in_fod_calibration = true;
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGOFF);
				dsi_panel_update_backlight(panel, ctl->feature_val);
				mi_cfg->dimming_state = STATE_NONE;
			} else {
				mi_cfg->in_fod_calibration = false;
				DISP_ERROR("FOD calibration invalid brightness level:%d\n", ctl->feature_val);
			}
		}
		mi_cfg->feature_val[DISP_FEATURE_FOD_CALIBRATION_BRIGHTNESS] = ctl->feature_val;
		break;
	case DISP_FEATURE_FOD_CALIBRATION_HBM:
		if (ctl->feature_val == -1) {
			DISP_INFO("FOD calibration HBM restore last_bl_level=%d\n",
				mi_cfg->last_bl_level);
			if (mi_cfg->hbm_51_ctl_flag && mi_cfg->hbm_fod_off_51_index >= 0)
				mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_HBM_FOD_OFF, mi_cfg->last_bl_level);
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_FOD_OFF);
			mi_cfg->dimming_state = STATE_DIM_RESTORE;
			mi_cfg->in_fod_calibration = false;
		} else {
			mi_cfg->in_fod_calibration = true;
			if (mi_cfg->hbm_51_ctl_flag && mi_cfg->hbm_fod_on_51_index >= 0) {
				if (ctl->feature_val >= mi_cfg->hbm_bl_min_lvl
					&& ctl->feature_val <= mi_cfg->hbm_bl_max_lvl) {
					mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_HBM_FOD_ON, ctl->feature_val);
				} else {
					mi_cfg->in_fod_calibration = false;
					DISP_ERROR("FOD calibration HBM invalid brightness level:%d\n", ctl->feature_val);
				}
			}
			if (mi_cfg->in_fod_calibration) {
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_FOD_ON);
				mi_cfg->dimming_state = STATE_DIM_BLOCK;
			}
		}
		mi_cfg->feature_val[DISP_FEATURE_FOD_CALIBRATION_HBM] = ctl->feature_val;
		break;
	case DISP_FEATURE_FLAT_MODE:
		if (ctl->feature_val == FEATURE_ON) {
			if (mi_cfg->flatmode_update_flag)
				mi_dsi_update_flatmode_cmd(panel, DSI_CMD_SET_MI_FLAT_MODE_ON);
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_ON);
		} else {
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_OFF);
		}
		mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE] = ctl->feature_val;
		break;
	case DISP_FEATURE_NATURE_FLAT_MODE:
		if (ctl->feature_val == FEATURE_ON)
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_NATURE_FLAT_MODE_ON);
		else
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_NATURE_FLAT_MODE_OFF);
		mi_cfg->feature_val[DISP_FEATURE_NATURE_FLAT_MODE] = ctl->feature_val;
		break;
	case DISP_FEATURE_DC:
		DISP_INFO("DC mode state:%d\n", ctl->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_DC] = ctl->feature_val;
		mi_dsi_dc_mode_enable(panel, ctl->feature_val == FEATURE_ON);
		break;
	case DISP_FEATURE_CRC:
		if (ctl->feature_val == FEATURE_OFF)
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CRC_OFF);
		break;
	case DISP_FEATURE_LOCAL_HBM:
		switch (ctl->feature_val) {
		case LOCAL_HBM_OFF_TO_NORMAL:
			if (mi_cfg->feature_val[DISP_FEATURE_HBM] == FEATURE_ON) {
				DISP_INFO("LOCAL_HBM_OFF_TO_HBM\n");
				mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_HBM, panel->mi_cfg.last_bl_level);
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_HBM);
				mi_dsi_update_micfg_flags(panel, PANEL_NOLP);
				mi_cfg->doze_brightness_backup = DOZE_TO_NORMAL;
				mi_cfg->local_hbm_to_normal = true;
				mi_cfg->dimming_state = STATE_DIM_BLOCK;
			} else {
				DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL\n");
				mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL, panel->mi_cfg.last_bl_level);
				dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL);
				mi_dsi_update_micfg_flags(panel, PANEL_NOLP);
				mi_cfg->doze_brightness_backup = DOZE_TO_NORMAL;
				mi_cfg->local_hbm_to_normal = true;
			}
			break;
		case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT:
			DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT\n");
			mi_dsi_update_backlight_in_aod(panel, false);
			if (is_aod_and_panel_initialized(panel)) {
				switch (mi_cfg->doze_brightness) {
					case DOZE_BRIGHTNESS_HBM:
						mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL, mi_cfg->doze_hbm_dbv_level);
						DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT in doze_hbm_dbv_level\n");
						break;
					case DOZE_BRIGHTNESS_LBM:
						mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL, mi_cfg->doze_lbm_dbv_level);
						DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT in doze_lbm_dbv_level\n");
						break;
					default:
						DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT defaults\n");
						break;
				}
			}
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL);
			mi_dsi_update_micfg_flags(panel, PANEL_NOLP);
			mi_cfg->local_hbm_to_normal = true;
			break;
		case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE:
			DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE\n");
			if (mi_cfg->panel_id == 0x4B3800420200) {
				mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL, 0);
			} else {
				mi_dsi_update_backlight_in_aod(panel, true);
			}
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL);
			mi_dsi_update_micfg_flags(panel, PANEL_NOLP);
			mi_cfg->local_hbm_to_normal = true;
			break;
		case LOCAL_HBM_NORMAL_WHITE_1000NIT:
			if (mi_cfg->feature_val[DISP_FEATURE_HBM] == FEATURE_ON) {
				DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT in HBM\n");
				mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT, panel->bl_config.bl_max_level);
			} else if (mi_cfg->feature_val[DISP_FEATURE_DC] == FEATURE_ON
						&& panel->mi_cfg.last_bl_level < panel->mi_cfg.dc_threshold
						&& panel->mi_cfg.dc_type) {

				DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT in DC range\n");
				mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT, panel->mi_cfg.dc_threshold);

				if (is_aod_and_panel_initialized(panel)) {
					switch (mi_cfg->doze_brightness) {
						case DOZE_BRIGHTNESS_HBM:
							mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT, mi_cfg->doze_hbm_dbv_level);
							DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT in doze_hbm_dbv_level\n");
							break;
						case DOZE_BRIGHTNESS_LBM:
							mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT, mi_cfg->doze_lbm_dbv_level);
							DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT in doze_lbm_dbv_level\n");
							break;
						default:
							DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT defaults\n");
							break;
					}
				}
			} else {
				DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT\n");
				mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT, panel->mi_cfg.last_bl_level);
			}

			mi_dsi_update_hbm_cmd_87reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT, panel->mi_cfg.last_no_zero_bl_level);
			if (is_aod_and_panel_initialized(panel)) {
				switch (mi_cfg->doze_brightness) {
					case DOZE_BRIGHTNESS_HBM:
						mi_dsi_update_hbm_cmd_87reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT, panel->mi_cfg.doze_hbm_dbv_level);
						DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT in doze_hbm_dbv_level\n");
						break;
					case DOZE_BRIGHTNESS_LBM:
						mi_dsi_update_hbm_cmd_87reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT, panel->mi_cfg.doze_lbm_dbv_level);
						DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT in doze_lbm_dbv_level\n");
						break;
					default:
						DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT defaults\n");
						break;
				}
			}

			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT);
			mi_cfg->dimming_state = STATE_DIM_BLOCK;
			break;
		case LOCAL_HBM_NORMAL_WHITE_750NIT:
			DISP_INFO("LOCAL_HBM_NORMAL_WHITE_750NIT\n");
			mi_dsi_update_hbm_cmd_87reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_750NIT, panel->mi_cfg.last_no_zero_bl_level);
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_750NIT);
			mi_cfg->dimming_state = STATE_DIM_BLOCK;
			break;
		case LOCAL_HBM_NORMAL_WHITE_500NIT:
			DISP_INFO("LOCAL_HBM_NORMAL_WHITE_500NIT\n");
			mi_dsi_update_hbm_cmd_87reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_500NIT, panel->mi_cfg.last_no_zero_bl_level);
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_500NIT);
			mi_cfg->dimming_state = STATE_DIM_BLOCK;
			break;
		case LOCAL_HBM_NORMAL_WHITE_110NIT:
			DISP_INFO("LOCAL_HBM_NORMAL_WHITE_110NIT\n");
			mi_dsi_update_hbm_cmd_87reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT, panel->mi_cfg.last_no_zero_bl_level);
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT);
			mi_cfg->dimming_state = STATE_DIM_BLOCK;
			break;
		case LOCAL_HBM_NORMAL_GREEN_500NIT:
			DISP_INFO("LOCAL_HBM_NORMAL_GREEN_500NIT\n");
			mi_dsi_update_hbm_cmd_87reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_GREEN_500NIT, panel->mi_cfg.last_no_zero_bl_level);
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_GREEN_500NIT);
			mi_cfg->dimming_state = STATE_DIM_BLOCK;
			break;
		case LOCAL_HBM_HLPM_WHITE_1000NIT:
			DISP_INFO("LOCAL_HBM_HLPM_WHITE_1000NIT\n");
			mi_dsi_update_hbm_cmd_87reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT, panel->mi_cfg.last_no_zero_bl_level);
			if (is_aod_and_panel_initialized(panel)) {
				switch (mi_cfg->doze_brightness) {
					case DOZE_BRIGHTNESS_HBM:
						mi_dsi_update_hbm_cmd_87reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT, mi_cfg->doze_hbm_dbv_level);
						DISP_INFO("LOCAL_HBM_HLPM_WHITE_1000NIT in doze_hbm_dbv_level\n");
						break;
					case DOZE_BRIGHTNESS_LBM:
						mi_dsi_update_hbm_cmd_87reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT, mi_cfg->doze_lbm_dbv_level);
						DISP_INFO("LOCAL_HBM_HLPM_WHITE_1000NIT in doze_lbm_dbv_level\n");
						break;
					default:
						DISP_INFO("LOCAL_HBM_HLPM_WHITE_1000NIT defaults\n");
						break;
				}
			}
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT);
			break;
		case LOCAL_HBM_HLPM_WHITE_110NIT:
			DISP_INFO("LOCAL_HBM_HLPM_WHITE_110NIT\n");
			mi_dsi_update_hbm_cmd_87reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT, panel->mi_cfg.last_no_zero_bl_level);
			if (is_aod_and_panel_initialized(panel)) {
				switch (mi_cfg->doze_brightness) {
					case DOZE_BRIGHTNESS_HBM:
						mi_dsi_update_hbm_cmd_87reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT, mi_cfg->doze_hbm_dbv_level);
						DISP_INFO("LOCAL_HBM_HLPM_WHITE_110NIT in doze_hbm_dbv_level\n");
						break;
					case DOZE_BRIGHTNESS_LBM:
						mi_dsi_update_hbm_cmd_87reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT, mi_cfg->doze_lbm_dbv_level);
						DISP_INFO("LOCAL_HBM_HLPM_WHITE_110NIT in doze_lbm_dbv_level\n");
						break;
					default:
						DISP_INFO("LOCAL_HBM_HLPM_WHITE_110NIT defaults\n");
						break;
				}
			}
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT);
			break;
		case LOCAL_HBM_OFF_TO_HLPM:
			DISP_INFO("LOCAL_HBM_OFF_TO_HLPM\n");
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_HLPM);
			break;
		case LOCAL_HBM_OFF_TO_LLPM:
			DISP_INFO("LOCAL_HBM_OFF_TO_LLPM\n");
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_LLPM);
			break;
		default:
			DISP_ERROR("invalid local hbm value\n");
			break;
		}
		mi_cfg->feature_val[DISP_FEATURE_LOCAL_HBM] = ctl->feature_val;
		break;
	case DISP_FEATURE_SENSOR_LUX:
		DISP_DEBUG("DISP_FEATURE_SENSOR_LUX=%d\n", ctl->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_SENSOR_LUX] = ctl->feature_val;
		break;
	case DISP_FEATURE_LOW_BRIGHTNESS_FOD:
		DISP_INFO("DISP_FEATURE_LOW_BRIGHTNESS_FOD=%d\n", ctl->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_LOW_BRIGHTNESS_FOD] = ctl->feature_val;
		break;
	case DISP_FEATURE_FP_STATUS:
		DISP_INFO("DISP_FEATURE_FP_STATUS=%d\n", ctl->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_FP_STATUS] = ctl->feature_val;
		if  (ctl->feature_val == ENROLL_STOP ||
				ctl->feature_val == AUTH_STOP ||
				ctl->feature_val == HEART_RATE_STOP) {
			mi_disp_set_fod_queue_work(0, false);
		}
		break;
	case DISP_FEATURE_FOLD_STATUS:
		DISP_INFO("DISP_FEATURE_FOLD_STATUS=%d\n", ctl->feature_val);
		fold_status = ctl->feature_val;
		break;
	case DISP_FEATURE_SPR_RENDER:
		DISP_INFO("DISP_FEATURE_SPR_RENDER=%d\n", ctl->feature_val);
		switch (ctl->feature_val) {
		case SPR_1D_RENDERING:
			DISP_INFO("SPR_1D_RENDERING\n");
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_SPR_1D_RENDERING);
			break;
		case SPR_2D_RENDERING:
			DISP_INFO("SPR_2D_RENDERING\n");
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_SPR_2D_RENDERING);
			break;
		default:
			DISP_ERROR("invalid spr render value\n");
			break;
		}
		mi_cfg->feature_val[DISP_FEATURE_SPR_RENDER] = ctl->feature_val;
		break;
	case DISP_FEATURE_AOD_TO_NORMAL:
		if (is_hbm_fod_on(panel)) {
			DSI_INFO("fod hbm on, skip MI_FOD_AOD_TO_NORMAL %d\n", ctl->feature_val);
			break;
		}

		if (ctl->feature_val == FEATURE_ON) {
			switch (mi_cfg->doze_brightness_backup) {
			case DOZE_BRIGHTNESS_HBM:
				DISP_INFO("enter DOZE HBM NOLP\n");
				mi_dsi_update_backlight_in_aod(panel, false);
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM_NOLP);
				if (rc) {
					DISP_ERROR("[%s] failed to send DSI_CMD_SET_MI_DOZE_HBM_NOLP cmd, rc=%d\n",
						panel->name, rc);
				}
				break;
			case DOZE_BRIGHTNESS_LBM:
				DISP_INFO("enter DOZE LBM NOLP\n");
				mi_dsi_update_backlight_in_aod(panel, false);
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM_NOLP);
				if (rc) {
					DISP_ERROR("[%s] failed to send DSI_CMD_SET_MI_DOZE_LBM_NOLP cmd, rc=%d\n",
						panel->name, rc);
				}
				break;
			default:
				break;
			}
			mi_dsi_update_micfg_flags(panel, PANEL_NOLP);
			panel->mi_cfg.is_doze_to_off = false;
			panel->mi_cfg.local_hbm_to_normal = true;
		} else if (ctl->feature_val == FEATURE_OFF) {
			switch (mi_cfg->doze_brightness_backup) {
			case DOZE_BRIGHTNESS_HBM:
				DISP_INFO("enter DOZE HBM\n");
				mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_DOZE_HBM, mi_cfg->doze_hbm_dbv_level);
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM);
				if (rc) {
					DISP_ERROR("[%s] failed to send DSI_CMD_SET_MI_DOZE_HBM_NOLP cmd, rc=%d\n",
						panel->name, rc);
				}
				mi_dsi_update_micfg_flags(panel, PANEL_DOZE_HIGH);
				panel->mi_cfg.is_doze_to_off = true;
				break;
			case DOZE_BRIGHTNESS_LBM:
				DISP_INFO("enter DOZE LBM\n");
				mi_dsi_update_hbm_cmd_51reg(panel, DSI_CMD_SET_MI_DOZE_LBM, mi_cfg->doze_lbm_dbv_level);
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM);
				if (rc) {
					DISP_ERROR("[%s] failed to send DSI_CMD_SET_MI_DOZE_LBM_NOLP cmd, rc=%d\n",
						panel->name, rc);
				}
				mi_dsi_update_micfg_flags(panel, PANEL_DOZE_LOW);
				panel->mi_cfg.is_doze_to_off = true;
				break;
			default:
				break;
			}
			panel->mi_cfg.local_hbm_to_normal = false;
		}
		break;
	case DISP_FEATURE_COLOR_INVERT:
		DISP_INFO("DISP_FEATURE_COLOR_INVERT=%d\n", ctl->feature_val);
		if (ctl->feature_val == FEATURE_ON)
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_COLOR_INVERT_ON);
		else
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_COLOR_INVERT_OFF);
		mi_cfg->feature_val[DISP_FEATURE_COLOR_INVERT] = ctl->feature_val;
	case DISP_FEATURE_DC_BACKLIGHT:
		DISP_INFO("DC backlight:%d\n", ctl->feature_val);
		mi_dsi_update_dc_backlight(panel, ctl->feature_val);
		break;
	case DISP_FEATURE_BIC:
		DISP_INFO("BIC read state:%d\n", ctl->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_BIC] = ctl->feature_val;
		if (panel->mi_cfg.feature_val[DISP_FEATURE_BIC] == BIC_READ_NEED_RIGHT_NOW)
			mi_dsi_panel_read_nvt_bic(panel);
		else if (panel->mi_cfg.feature_val[DISP_FEATURE_BIC] == BIC_UPDAT_REG_RIGHT_NOW) {
			mi_dsi_set_bic_reg(panel);
		}
		break;
	case DISP_FEATURE_GIR:
		DISP_INFO("DISP_FEATURE_GIR ON:%d\n", ctl->feature_val);
		if (ctl->feature_val == FEATURE_ON) {
			if (mi_cfg->flatmode_update_flag)
				DISP_INFO("disp feature GIR ON:%d update flatmode\n", ctl->feature_val);
				mi_dsi_update_flatmode_cmd(panel, DSI_CMD_SET_MI_FLAT_MODE_ON);
		}
		mi_cfg->feature_val[DISP_FEATURE_GIR] = ctl->feature_val;
        break;
	default:
		DISP_ERROR("invalid feature id\n");
		break;
	}

exit:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

ssize_t mi_dsi_panel_get_disp_param(struct dsi_panel *panel,
			char *buf, size_t size)
{
	struct mi_dsi_panel_cfg *mi_cfg;
	ssize_t count = 0;
	int i = 0;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EAGAIN;
	}

	mi_cfg = &panel->mi_cfg;

	count = snprintf(buf, size, "%040s: feature vaule\n", "feature name[feature id]");

	mutex_lock(&panel->panel_lock);
	for (i = DISP_FEATURE_DIMMING; i < DISP_FEATURE_MAX; i++) {
		count += snprintf(buf + count, size - count, "%036s[%02d]: %d\n",
				     get_disp_feature_id_name(i), i, mi_cfg->feature_val[i]);
	}
	mutex_unlock(&panel->panel_lock);

	return count;
}

void mi_dsi_panel_demura_comp(struct dsi_panel *panel, u32 bl_lvl)
{
	int i = 0;
	int rc = 0;
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!panel || !panel->mi_cfg.demura_comp
		|| (panel->mi_cfg.feature_val[DISP_FEATURE_DC] == FEATURE_ON))
		return;

	mi_cfg = &panel->mi_cfg;

	if (bl_lvl == 0) {
		mi_cfg->demura_mask = 0;
		return;
	}

	for (i = 0; i < mi_cfg->demura_bl_num; i++) {
		if (bl_lvl <= mi_cfg->demura_bl[i])
			break;
	}

	if (i == mi_cfg->demura_bl_num || mi_cfg->demura_mask & (1 << i))
		return;

	DISP_INFO("mi_dsi_panel_demura_comp, set demura level:%d\n", i);
	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DEMURA_L1+i);
	if (rc < 0)
			DSI_ERR("failed to send DSI_CMD_SET_MI_BL_DEMURA, bl:%d\n", bl_lvl);

	mi_cfg->demura_mask = (1 << i);
}

void mi_dsi_panel_dc_vi_setting(struct dsi_panel *panel, u32 bl_lvl)
{
	struct mi_dsi_panel_cfg *mi_cfg  = NULL;

	if (!panel && !bl_lvl) {
		DISP_ERROR("invalid params\n");
		return;
	}

	mi_cfg = &panel->mi_cfg;

	if (mi_cfg->dc_type == 0 && mi_cfg->feature_val[DISP_FEATURE_DC] == FEATURE_ON
		&& mi_cfg->dc_threshold > 0) {
		if (mi_cfg->last_bl_level <= mi_cfg->dc_threshold && bl_lvl > mi_cfg->dc_threshold)
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DC_VI_HBM);
		else if ((mi_cfg->last_bl_level == 0 || mi_cfg->last_bl_level > mi_cfg->dc_threshold)
			&& bl_lvl <= mi_cfg->dc_threshold)
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DC_VI_LBM);
	}
}


struct fod_work_data {
	struct dsi_display *display;
	bool from_touch;
	int fod_btn;
};

static void mi_sde_connector_fod_lhbm_notify(struct drm_connector *conn, int fod_btn)
{
	struct sde_connector *c_conn;
	bool icon;
	static bool last_icon = false;
	struct dsi_display *display;

	if (!conn) {
		DISP_ERROR("invalid params\n");
		return;
	}

	c_conn = to_sde_connector(conn);
	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI) {
		DISP_ERROR("not DRM_MODE_CONNECTOR_DSIl\n");
		return;
	}

	display = (struct dsi_display *) c_conn->display;
	if (!display || !display->panel) {
		DISP_ERROR("invalid display/panel ptr\n");
		return;
	}

	if (mi_get_disp_id(display) != MI_DISP_PRIMARY)
		return;

	icon = (fod_btn == 1);
	if (last_icon != icon) {
		if (icon) {
			/* With local hbm, it will enable after first frame tx complete.
			 * So skip fod notify before local fod hbm enabled.
			 */
			if (display->panel->mi_cfg.local_hbm_enabled
				&& !display->panel->mi_cfg.fod_hbm_layer_enabled) {
				return;
			}
			/* Make sure icon was displayed on panel before notifying
			 * fingerprint to capture image */
			if (display->panel->mi_cfg.fod_hbm_layer_enabled) {
#if 0
				sde_encoder_wait_for_event(c_conn->encoder,
						MSM_ENC_TX_COMPLETE);
				sde_encoder_wait_for_event(c_conn->encoder,
						MSM_ENC_VBLANK);
#endif
			}
			SDE_ATRACE_BEGIN("mi_sde_connector_fod_ui_ready");
			mi_sde_connector_fod_ui_ready(display, 2, 1);
			SDE_ATRACE_END("mi_sde_connector_fod_ui_ready");
		} else {
			mi_sde_connector_fod_ui_ready(display, 2, 0);
		}
	}
	last_icon = icon;
}

static int mi_sde_connector_fod_lhbm(struct drm_connector *connector, bool from_touch, int fod_btn)
{
	int rc = 0;
	struct sde_connector *c_conn;
	struct dsi_display *display;
	bool btn_down;
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!connector) {
		DISP_ERROR("invalid connector ptr\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI) {
		DISP_ERROR("not DRM_MODE_CONNECTOR_DSI\n");
		return -EINVAL;
	}

	display = (struct dsi_display *) c_conn->display;
	if (!display || !display->panel) {
		DISP_ERROR("invalid display/panel ptr\n");
		return -EINVAL;
	}

	if (mi_get_disp_id(display) != MI_DISP_PRIMARY) {
		DISP_ERROR("not MI_DISP_PRIMARY\n");
		return -EINVAL;
	}

	mi_cfg = &display->panel->mi_cfg;
	btn_down = (fod_btn == 1);

	DISP_INFO("dsi_mi_sde_connector_fod_lhbm=%d\n", btn_down);
	if (btn_down) {
		if (mi_cfg->fod_hbm_layer_enabled == false) {
			if (!c_conn->allow_bl_update) { /* Wait first frame tx done */
				DISP_INFO("LHBM on !allow_bl_update\n");
				mi_cfg->pending_lhbm_state = 1;
				rc = -ENODEV;
			} else {
				if (!mi_cfg->pending_lhbm_state && !from_touch) {
					rc = -EINVAL;
					DISP_INFO("LHBM on from display skip\n");
				} else {
					rc = mi_sde_connector_panel_ctl(connector, MI_FOD_HBM_ON);
					if (!rc) {
						mi_cfg->fod_hbm_layer_enabled = true;
						mi_cfg->pending_lhbm_state = 0;
					} else if (rc == -ENODEV) {
						DISP_INFO("LHBM on !panel_initialized rc=%d\n", rc);
						mi_cfg->pending_lhbm_state = 1;
					} else {
						DISP_INFO("LHBM on failed rc=%d\n", rc);
					}
				}
			}
		}
	} else {
		mi_cfg->pending_lhbm_state = 0;
		if (mi_cfg->fod_hbm_layer_enabled == true) {
			rc = mi_sde_connector_panel_ctl(connector, MI_FOD_HBM_OFF);
			mi_cfg->fod_hbm_layer_enabled = false;
		}
	}

	return rc;
}

enum {
	FOD_WORK_INIT = 0,
	FOD_WORK_DOING = 1,
	FOD_WORK_DONE = 2,
};

static atomic_t fod_work_status = ATOMIC_INIT(FOD_WORK_INIT);
static atomic_t touch_current_status = ATOMIC_INIT(-1);
static atomic_t touch_last_status = ATOMIC_INIT(-1);

static void mi_disp_set_fod_work_handler(struct kthread_work *work)
{
	int rc = 0;
	struct fod_work_data *fod_data = NULL;
	struct disp_work *cur_work = container_of(work,
					struct disp_work, work);

	fod_data = (struct fod_work_data *)(cur_work->data);

	if (!fod_data || !fod_data->display) {
		DISP_ERROR("invalid params\n");
		return;
	}

	if (fod_data->from_touch) {
		atomic_set(&fod_work_status, FOD_WORK_DOING);
		do {
			DISP_DEBUG("from touch, current(%d),last(%d)\n",
				atomic_read(&touch_current_status), atomic_read(&touch_last_status));
			atomic_set(&touch_current_status, atomic_read(&touch_last_status));
			if (atomic_read(&touch_current_status) == 0) {
				mi_sde_connector_fod_lhbm_notify(fod_data->display->drm_conn,
					atomic_read(&touch_current_status));
			}

			rc = mi_sde_connector_fod_lhbm(fod_data->display->drm_conn,
				true, atomic_read(&touch_current_status));

			if (atomic_read(&touch_current_status) == 1) {
				if (rc) {
					DISP_ERROR("LHBM on failed rc=%d, not notify\n", rc);
				} else {
					mi_sde_connector_fod_lhbm_notify(fod_data->display->drm_conn,
						atomic_read(&touch_current_status));
				}
			}
		} while (atomic_read(&touch_current_status) != atomic_read(&touch_last_status));
		atomic_set(&fod_work_status, FOD_WORK_DONE);
	} else {
		DISP_DEBUG("not from touch, fod_btn(%d)\n", fod_data->fod_btn);
		if (fod_data->fod_btn == 0) {
			mi_sde_connector_fod_lhbm_notify(fod_data->display->drm_conn,
				fod_data->fod_btn);
		}

		rc = mi_sde_connector_fod_lhbm(fod_data->display->drm_conn,
			fod_data->from_touch, fod_data->fod_btn);

		if (fod_data->fod_btn == 1) {
			if (rc) {
				DISP_ERROR("LHBM on failed rc=%d, not notify\n", rc);
			} else {
				mi_sde_connector_fod_lhbm_notify(fod_data->display->drm_conn,
					fod_data->fod_btn);
			}
		}
	}

	kfree(cur_work);
	DISP_DEBUG("fod work handler done\n");
}

int mi_disp_set_fod_queue_work(u32 fod_btn, bool from_touch)
{
	struct disp_work *cur_work;
	struct dsi_display *display = mi_get_primary_dsi_display();
	struct disp_feature *df = mi_get_disp_feature();
	struct fod_work_data *fod_data;
	struct mi_dsi_panel_cfg *mi_cfg;
	int fp_state = FINGERPRINT_NONE;

#ifdef CONFIG_FACTORY_BUILD
	return 0;
#endif

	if (!display || !display->panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (from_touch) {
		if (atomic_read(&fod_work_status) == FOD_WORK_DOING) {
			DISP_DEBUG("work doing: from touch current(%d), new fod_btn(%d), skip\n",
				atomic_read(&touch_current_status), fod_btn);
			if (atomic_read(&touch_current_status) != fod_btn) {
				atomic_set(&touch_last_status, fod_btn);
			}
			return 0;
		} else {
			if (atomic_read(&touch_current_status) == fod_btn) {
				DISP_DEBUG("from touch fod_btn(%d), skip\n", fod_btn);
				return 0;
			} else {
				atomic_set(&touch_last_status, fod_btn);
				DISP_DEBUG("from touch fod_btn=%d\n", fod_btn);
			}
		}
	}

	mi_cfg = &display->panel->mi_cfg;
	fp_state = mi_cfg->feature_val[DISP_FEATURE_FP_STATUS];
	if (fp_state == ENROLL_STOP || fp_state == AUTH_STOP || fp_state == HEART_RATE_STOP) {
		if (fod_btn == 1) {
			DISP_INFO("fp_state=%d, skip\n", fp_state);
			return 0;
		}
	}

	cur_work = kzalloc(sizeof(*cur_work) + sizeof(*fod_data), GFP_ATOMIC);
	if (!cur_work)
		return -ENOMEM;

	fod_data = (struct fod_work_data *)((u8 *)cur_work + sizeof(struct disp_work));
	fod_data->display = display;
	fod_data->from_touch = from_touch;
	fod_data->fod_btn = fod_btn;

	kthread_init_work(&cur_work->work, mi_disp_set_fod_work_handler);
	cur_work->dd_ptr = &df->d_display[MI_DISP_PRIMARY];
	cur_work->wq = &df->d_display[MI_DISP_PRIMARY].fod_pending_wq;
	cur_work->data = fod_data;

	DISP_INFO("fod_queue_work: fod_btn(%d), from_touch(%d)\n", fod_btn, from_touch);
	kthread_queue_work(&cur_work->dd_ptr->fod_thread.worker, &cur_work->work);

	return 0;
}

void mi_dsi_update_backlight_in_aod(struct dsi_panel *panel, bool restore_backlight)
{
	int bl_lvl = 0;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;
	struct mipi_dsi_device *dsi = &panel->mipi_device;

	if (restore_backlight) {
		bl_lvl = mi_cfg->last_bl_level;
	} else {
		switch (mi_cfg->doze_brightness_backup) {
			case DOZE_BRIGHTNESS_HBM:
				bl_lvl = mi_cfg->doze_hbm_dbv_level;
				break;
			case DOZE_BRIGHTNESS_LBM:
				bl_lvl = mi_cfg->doze_lbm_dbv_level;
				break;
			default:
				return;
		}
	}
	DISP_INFO("mi_dsi_update_backlight_in_aod %d\n", bl_lvl);
	if (panel->bl_config.bl_inverted_dbv)
		bl_lvl = (((bl_lvl & 0xff) << 8) | (bl_lvl >> 8));
	mipi_dsi_dcs_set_display_brightness(dsi, bl_lvl);

	return;
}

void mi_dsi_update_dc_backlight(struct dsi_panel *panel, u32 bl_lvl)
{
	struct mipi_dsi_device *dsi = &panel->mipi_device;

	DISP_INFO("mi_dsi_update_dc_backlight bl_lvl=%d\n", bl_lvl);
	if (panel->bl_config.bl_inverted_dbv)
		bl_lvl = (((bl_lvl & 0xff) << 8) | (bl_lvl >> 8));
	mipi_dsi_dcs_set_display_brightness(dsi, bl_lvl);

	return;
}

void mi_dsi_backlight_logging(struct dsi_panel *panel, u32 bl_lvl)
{
	if (!panel) {
		DISP_INFO("invalid params\n");
		return;
	}

	if (!panel->mi_cfg.last_bl_level && bl_lvl)
		panel->mi_cfg.bl_statistic_cnt = BL_STATISTIC_CNT_MAX;

	if ((panel->mi_cfg.bl_statistic_cnt <= 0 && bl_lvl)
		|| (panel->mi_cfg.bl_statistic_cnt > 0
		&& panel->mi_cfg.last_bl_level == bl_lvl))
		return;

	if (panel->mi_cfg.bl_statistic_cnt > 0)
		panel->mi_cfg.bl_statistic_cnt--;

	DSI_INFO("%s set backlight from %d to %d\n",
		panel->type, panel->mi_cfg.last_bl_level, bl_lvl);
}

void mi_disp_handle_lp_event(struct dsi_panel *panel, int power_mode)
{
	u64 jiffies_time = 0;
	u64 diff_time = 0;

	if (!panel) {
		DISP_INFO("invalid params\n");
		return;
	}

	if (panel->mi_cfg.aod_exit_delay_time) {
		switch (power_mode) {
		case SDE_MODE_DPMS_LP1:
			if ((panel->power_mode != SDE_MODE_DPMS_LP1) &&
				(panel->power_mode != SDE_MODE_DPMS_LP2))
				panel->mi_cfg.aod_enter_time = get_jiffies_64();
			break;
		case SDE_MODE_DPMS_ON:
			jiffies_time = get_jiffies_64();
			if (time_after64(jiffies_time, panel->mi_cfg.aod_enter_time))
				diff_time = jiffies_time - panel->mi_cfg.aod_enter_time;
			diff_time = (diff_time * 1000)/ HZ;
			if (diff_time < panel->mi_cfg.aod_exit_delay_time
				&& panel->mi_cfg.aod_exit_delay_time < MAX_SLEEP_TIME)
				msleep(panel->mi_cfg.aod_exit_delay_time);
			break;
		case SDE_MODE_DPMS_OFF:
			panel->mi_cfg.aod_enter_time = 0;
			break;
		default:
			break;
		}
	}
}

EXPORT_SYMBOL_GPL(mi_disp_set_fod_queue_work);

int mi_dsi_panel_lhbm_set(struct dsi_panel *panel)
{
	struct dsi_display *display;
	struct dsi_display_ctrl *ctrl;
	char bufferR[50];
	char bufferG[50];
	char bufferB[50];
	char read_before[100] ="00 00 39 01 00 00 00 00 06 F0 55 AA 52 08 02 39 01 00 00 00 00 02 BF 19";
	int gray_node[25] = {0, 1, 3, 5, 7, 11, 15, 19, 23, 31, 39, 47, 55, 63, 79, 95, 111, 127, 159, 191, 223, 239, 247, 255,255};
	int R_1 = 0, R_2 = 0, G_1 = 0, G_2 = 0, B_1 = 0, B_2 = 0;
	struct dsi_cmd_desc *cmds = NULL;
	struct dsi_display_mode_priv_info *priv_info;
	u8 *tx_buf;
	u32 count;
	int type = 0;
	int ret = 0;
	int i = 0;

	if (!panel || !panel->host) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	display = to_dsi_display(panel->host);
	if (display == NULL)
		return -EINVAL;

	if (!panel->panel_initialized) {
		DISP_ERROR("[%s] Panel not initialized\n", panel->type);
		return -EINVAL;
	}

	ctrl = &display->ctrl[display->cmd_master_idx];

	ret = dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_ON);
	if (ret) {
		DISP_ERROR("[%s] failed to enable DSI clocks, ret=%d\n", display->name, ret);
		goto unlock;
	}

	ret = dsi_display_cmd_engine_enable(display);
	if (ret) {
		DISP_ERROR("[%s] failed to enable cmd engine, ret=%d\n",
		       display->name, ret);
		goto error_disable_clks;
	}

	if (display->tx_cmd_buf == NULL) {
		ret = dsi_host_alloc_cmd_tx_buffer(display);
		if (ret) {
			DISP_ERROR("failed to allocate cmd tx buffer memory\n");
			goto error_disable_cmd_engine;
		}
	}

	/* Step1: change page2 before read */
	ret = mi_dsi_panel_write_mipi_reg(panel, read_before);
	if (ret) {
		DISP_ERROR("change page failed!\n");
		ret = -EAGAIN;
		goto error_disable_cmd_engine;
	}

	/* Step2: read */
	memset(bufferR, 0, 50);
	ret = mipi_dsi_dcs_read(&panel->mipi_device, 0xB0, bufferR, 18);
	ret = mipi_dsi_dcs_read(&panel->mipi_device, 0xB1, bufferR+18, 18);
	ret = mipi_dsi_dcs_read(&panel->mipi_device, 0xB2, bufferR+36, 14);
	if (ret == 0) {
		DISP_INFO("read failed ret = %d\n", ret);
		goto error_disable_cmd_engine;
	}
	memset(bufferG, 0, 50);
	ret = mipi_dsi_dcs_read(&panel->mipi_device, 0xB3, bufferG, 18);
	ret = mipi_dsi_dcs_read(&panel->mipi_device, 0xB4, bufferG+18, 18);
	ret = mipi_dsi_dcs_read(&panel->mipi_device, 0xB5, bufferG+36, 14);
	if (ret == 0) {
		DISP_INFO("read failed ret = %d\n", ret);
		goto error_disable_cmd_engine;
	}
	memset(bufferB, 0, 50);
	ret = mipi_dsi_dcs_read(&panel->mipi_device, 0xB6, bufferB, 18);
	ret = mipi_dsi_dcs_read(&panel->mipi_device, 0xB7, bufferB+18, 18);
	ret = mipi_dsi_dcs_read(&panel->mipi_device, 0xB8, bufferB+36, 14);
	if (ret == 0) {
		DISP_INFO("read failed ret = %d\n", ret);
		goto error_disable_cmd_engine;
	}

	/* Step3: caculate what we need rgb config */
	for (i = 15; i < (sizeof(gray_node)/sizeof(int) -2); i++) {
		R_1 = ((bufferR[2*i] << 8 & 0xFF00) |bufferR[2*i+1]);
		R_2 = ((bufferR[2*i+2]<<8) |bufferR[2*i+3]);
		G_1 = ((bufferG[2*i]<<8) |bufferG[2*i+1]);
		G_2 = ((bufferG[2*i+2]<<8) |bufferG[2*i+3]);
		B_1 = ((bufferB[2*i]<<8) |bufferB[2*i+1]);
		B_2 = ((bufferB[2*i+2]<<8) |bufferB[2*i+3]);
		if (255 >= gray_node[i] && 255 <= gray_node[i+1]) {
			panel->mi_cfg.lhbm_rgb_cfg.lhbm_1000nit_rgb[0] = (((R_2-R_1)*(255-gray_node[i]))/(gray_node[i+1] - gray_node[i])) + R_1;
			panel->mi_cfg.lhbm_rgb_cfg.lhbm_1000nit_rgb[1] = (((G_2-G_1)*(255-gray_node[i]))/(gray_node[i+1] - gray_node[i])) + G_1;
			panel->mi_cfg.lhbm_rgb_cfg.lhbm_1000nit_rgb[2] = (((B_2-B_1)*(255-gray_node[i]))/(gray_node[i+1] - gray_node[i])) + B_1;
		}
		if (223 >= gray_node[i] && 223 <= gray_node[i+1]) {
			panel->mi_cfg.lhbm_rgb_cfg.lhbm_750nit_rgb[0] = (((R_2-R_1)*(223-gray_node[i]))/(gray_node[i+1] - gray_node[i])) + R_1;
			panel->mi_cfg.lhbm_rgb_cfg.lhbm_750nit_rgb[1] = (((G_2-G_1)*(223-gray_node[i]))/(gray_node[i+1] - gray_node[i])) + G_1;
			panel->mi_cfg.lhbm_rgb_cfg.lhbm_750nit_rgb[2] = (((B_2-B_1)*(223-gray_node[i]))/(gray_node[i+1] - gray_node[i])) + B_1;
		}
		if (186 >= gray_node[i] && 186 <= gray_node[i+1]) {
			panel->mi_cfg.lhbm_rgb_cfg.lhbm_500nit_rgb[0] = (((R_2-R_1)*(186-gray_node[i]))/(gray_node[i+1] - gray_node[i])) + R_1;
			panel->mi_cfg.lhbm_rgb_cfg.lhbm_500nit_rgb[1] = (((G_2-G_1)*(186-gray_node[i]))/(gray_node[i+1] - gray_node[i])) + G_1;
			panel->mi_cfg.lhbm_rgb_cfg.lhbm_500nit_rgb[2] = (((B_2-B_1)*(186-gray_node[i]))/(gray_node[i+1] - gray_node[i])) + B_1;
		}
		if (107 >= gray_node[i] && 107 <= gray_node[i+1]) {
			panel->mi_cfg.lhbm_rgb_cfg.lhbm_110nit_rgb[0] = (((R_2-R_1)*(107-gray_node[i]))/(gray_node[i+1] - gray_node[i])) + R_1;
			panel->mi_cfg.lhbm_rgb_cfg.lhbm_110nit_rgb[1] = (((G_2-G_1)*(107-gray_node[i]))/(gray_node[i+1] - gray_node[i])) + G_1;
			panel->mi_cfg.lhbm_rgb_cfg.lhbm_110nit_rgb[2] = (((B_2-B_1)*(107-gray_node[i]))/(gray_node[i+1] - gray_node[i])) + B_1;
		}
	}

	/* Step4: replace lhbm rgb setting */
	DISP_INFO("display->panel->num_display_modes = %d\n", display->panel->num_display_modes);
	for (i = 0; i < display->panel->num_display_modes; i ++) {
		if (display->modes[i].timing.refresh_rate == 120) {
			priv_info = display->modes[i].priv_info;
			for (type = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT; type < DSI_CMD_SET_MAX; type ++) {
				switch (type) {
				case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT:
					cmds = priv_info->cmd_sets[type].cmds;
					count = priv_info->cmd_sets[type].count;
					if (cmds && count >= 2) {
						tx_buf = (u8 *)cmds[2].msg.tx_buf;
						if (tx_buf && tx_buf[0] == 0xD0) {
							tx_buf[1] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_1000nit_rgb[0] >> 8) & 0x0f;
							tx_buf[2] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_1000nit_rgb[0] & 0xff;
							tx_buf[3] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_1000nit_rgb[1] >> 8) & 0x0f;
							tx_buf[4] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_1000nit_rgb[1] & 0xff;
							tx_buf[5] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_1000nit_rgb[2] >> 8) & 0x0f;
							tx_buf[6] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_1000nit_rgb[2] & 0xff;
							DISP_INFO("%s, panel cmd[0x%02x] = 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
								cmd_set_prop_map[type], tx_buf[0], tx_buf[1], tx_buf[2],
								tx_buf[3], tx_buf[4], tx_buf[5], tx_buf[6]);
						}
					}
					continue;
				case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT:
					cmds = priv_info->cmd_sets[type].cmds;
					count = priv_info->cmd_sets[type].count;
					if (cmds && count >= 14) {
						tx_buf = (u8 *)cmds[14].msg.tx_buf;
						if (tx_buf && tx_buf[0] == 0xD0) {
							tx_buf[1] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_1000nit_rgb[0] >> 8) & 0x0f;
							tx_buf[2] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_1000nit_rgb[0] & 0xff;
							tx_buf[3] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_1000nit_rgb[1] >> 8) & 0x0f;
							tx_buf[4] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_1000nit_rgb[1] & 0xff;
							tx_buf[5] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_1000nit_rgb[2] >> 8) & 0x0f;
							tx_buf[6] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_1000nit_rgb[2] & 0xff;
							DISP_INFO("%s, panel cmd[0x%02x] = 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
								cmd_set_prop_map[type], tx_buf[0], tx_buf[1], tx_buf[2],
								tx_buf[3], tx_buf[4], tx_buf[5], tx_buf[6]);
						}
					}
					continue;
				case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_750NIT:
					cmds = priv_info->cmd_sets[type].cmds;
					count = priv_info->cmd_sets[type].count;
					if (cmds && count >= 2) {
						tx_buf = (u8 *)cmds[2].msg.tx_buf;
						if (tx_buf && tx_buf[0] == 0xD0) {
							tx_buf[1] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_750nit_rgb[0] >> 8) & 0x0f;
							tx_buf[2] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_750nit_rgb[0] & 0xff;
							tx_buf[3] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_750nit_rgb[1] >> 8) & 0x0f;
							tx_buf[4] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_750nit_rgb[1] & 0xff;
							tx_buf[5] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_750nit_rgb[2] >> 8) & 0x0f;
							tx_buf[6] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_750nit_rgb[2] & 0xff;
							DISP_INFO("%s, panel cmd[0x%02x] = 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
								cmd_set_prop_map[type] , tx_buf[0], tx_buf[1], tx_buf[2],
								tx_buf[3], tx_buf[4], tx_buf[5], tx_buf[6]);
						}
					}
					continue;
				case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_500NIT:
					cmds = priv_info->cmd_sets[type].cmds;
					count = priv_info->cmd_sets[type].count;
					if (cmds && count >= 2) {
						tx_buf = (u8 *)cmds[2].msg.tx_buf;
						if (tx_buf && tx_buf[0] == 0xD0) {
							tx_buf[1] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_500nit_rgb[0] >> 8) & 0x0f;
							tx_buf[2] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_500nit_rgb[0] & 0xff;
							tx_buf[3] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_500nit_rgb[1] >> 8) & 0x0f;
							tx_buf[4] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_500nit_rgb[1] & 0xff;
							tx_buf[5] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_500nit_rgb[2] >> 8) & 0x0f;
							tx_buf[6] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_500nit_rgb[2] & 0xff;
							DISP_INFO("%s, panel cmd[0x%02x] = 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
								cmd_set_prop_map[type], tx_buf[0], tx_buf[1], tx_buf[2],
								tx_buf[3], tx_buf[4], tx_buf[5], tx_buf[6]);
						}
					}
					continue;
				case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT:
					cmds = priv_info->cmd_sets[type].cmds;
					count = priv_info->cmd_sets[type].count;
					if (cmds && count >= 2) {
						tx_buf = (u8 *)cmds[2].msg.tx_buf;
						if (tx_buf && tx_buf[0] == 0xD0) {
							tx_buf[1] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_110nit_rgb[0] >> 8) & 0x0f;
							tx_buf[2] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_110nit_rgb[0] & 0xff;
							tx_buf[3] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_110nit_rgb[1] >> 8) & 0x0f;
							tx_buf[4] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_110nit_rgb[1] & 0xff;
							tx_buf[5] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_110nit_rgb[2] >> 8) & 0x0f;
							tx_buf[6] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_110nit_rgb[2] & 0xff;
							DISP_INFO("%s, panel cmd[0x%02x] = 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
								cmd_set_prop_map[type], tx_buf[0], tx_buf[1], tx_buf[2],
								tx_buf[3], tx_buf[4], tx_buf[5], tx_buf[6]);
						}
					}
					continue;
				case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT:
					cmds = priv_info->cmd_sets[type].cmds;
					count = priv_info->cmd_sets[type].count;
					if (cmds && count >= 14) {
						tx_buf = (u8 *)cmds[14].msg.tx_buf;
						if (tx_buf && tx_buf[0] == 0xD0) {
							tx_buf[1] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_110nit_rgb[0] >> 8) & 0x0f;
							tx_buf[2] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_110nit_rgb[0] & 0xff;
							tx_buf[3] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_110nit_rgb[1] >> 8) & 0x0f;
							tx_buf[4] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_110nit_rgb[1] & 0xff;
							tx_buf[5] = (panel->mi_cfg.lhbm_rgb_cfg.lhbm_110nit_rgb[2] >> 8) & 0x0f;
							tx_buf[6] = panel->mi_cfg.lhbm_rgb_cfg.lhbm_110nit_rgb[2] & 0xff;
							DISP_INFO("%s, panel cmd[0x%02x] = 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
								cmd_set_prop_map[type] , tx_buf[0], tx_buf[1], tx_buf[2],
								tx_buf[3], tx_buf[4], tx_buf[5], tx_buf[6]);
						}
					}
					continue;
				default:
					continue;
				}
			}
		}
	}

error_disable_cmd_engine:
	ret = dsi_display_cmd_engine_disable(display);
	if (ret) {
		DISP_ERROR("[%s]failed to disable DSI cmd engine, rc=%d\n",
				display->name, ret);
	}
error_disable_clks:
	ret = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);
	if (ret) {
		DISP_ERROR("[%s] failed to disable all DSI clocks, rc=%d\n",
			   display->name, ret);
	}
unlock:
	return ret;
}

int mi_dsi_panel_read_nvt_bic(struct dsi_panel *panel)
{
	struct disp_event event;
	struct dsi_display *display;
	struct dsi_display_ctrl *ctrl;
	struct dsi_display_mode *mode;
	int time = 0;
	int read_length = NVT_BIC_LENGTH;
	int read_count = 0;
	char buffer[10];
	int ret = 0;

	if (!panel || !panel->host) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	display = to_dsi_display(panel->host);
	if (display == NULL)
		return -EINVAL;

	if (!panel->panel_initialized) {
		DISP_ERROR("[%s] Panel not initialized\n", panel->type);
		return -EINVAL;
	}

	mode = panel->cur_mode;
	ctrl = &display->ctrl[display->cmd_master_idx];

	ret = dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_ON);
	if (ret) {
		DISP_ERROR("[%s] failed to enable DSI clocks, ret=%d\n", display->name, ret);
		goto unlock;
	}

	ret = dsi_display_cmd_engine_enable(display);
	if (ret) {
		DISP_ERROR("[%s] failed to enable cmd engine, ret=%d\n",
		       display->name, ret);
		goto error_disable_clks;
	}

	if (display->tx_cmd_buf == NULL) {
		ret = dsi_host_alloc_cmd_tx_buffer(display);
		if (ret) {
			DISP_ERROR("failed to allocate cmd tx buffer memory\n");
			goto error_disable_cmd_engine;
		}
	}

	/* Step1: write 0x28 and settings before read bic */
	ret = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_BIC_READ_PRE);
	if (ret) {
		DISP_ERROR("Failed to send DSI_CMD_SET_MI_BIC_READ_PRE command\n");
		ret = -EAGAIN;
		goto error_disable_cmd_engine;
	}

	/* Step2: read */
	DISP_INFO("read bic start...\n");
	while (read_length > 0) {
		memset(buffer, 0, 10);
		if (read_count == 0)
			ret = mipi_dsi_dcs_read(&panel->mipi_device, 0x4E, buffer, 10);
		else
			ret = mipi_dsi_dcs_read(&panel->mipi_device, 0x5E, buffer, 10);
		if (ret == 0) {
			DISP_ERROR("%s error\n");
			memset(read_result, 0, NVT_BIC_LENGTH);
			break;
		}
		memcpy(read_result + read_count * 10, buffer, 10);
		read_count++;
		read_length -= 10;
	}
	DISP_INFO("read bic end...\n");

	panel->mi_cfg.bic_data = read_result;
	panel->mi_cfg.bic_data_size = sizeof(read_result);

	/* Step3: After read finished send 0x10 */
	panel->mi_cfg.feature_val[DISP_FEATURE_BIC] = BIC_READ_FINISHED;
	ret = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_BIC_READ_FINISH);
	if (ret) {
		DISP_ERROR("Failed to send DSI_CMD_SET_MI_BIC_READ_FINISH command\n");
		ret = -EAGAIN;
		goto error_disable_cmd_engine;
	}

	/* Step4: notify to df */
	time++;
	event.disp_id = 0;
	event.type = MI_DISP_EVENT_BIC;
	event.length = sizeof(time);
	mi_disp_feature_event_notify(&event, (u8 *)&time);

error_disable_cmd_engine:
	ret = dsi_display_cmd_engine_disable(display);
	if (ret) {
		DISP_ERROR("[%s]failed to disable DSI cmd engine, rc=%d\n",
				display->name, ret);
	}
error_disable_clks:
	ret = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);
	if (ret) {
		DISP_ERROR("[%s] failed to disable all DSI clocks, rc=%d\n",
			   display->name, ret);
	}
unlock:

	return ret;
}

char *mi_dsi_panel_get_bic_data_info(int * bic_len)
{
	*bic_len = sizeof(read_result);
	DISP_INFO("sizeof(read_result):%d\n", sizeof(read_result));

	return read_result;
}

char *mi_dsi_panel_get_bic_reg_data_array(struct dsi_panel *panel)
{
	return panel->mi_cfg.bic_reg_data;
}


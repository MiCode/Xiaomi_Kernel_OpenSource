/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"msm-dsi-display:[%s] " fmt, __func__

#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/err.h>

#include "msm_drv.h"
#include "sde_connector.h"
#include "msm_mmu.h"
#include "dsi_display.h"
#include "dsi_panel.h"
#include "dsi_ctrl.h"
#include "dsi_ctrl_hw.h"
#include "dsi_drm.h"
#include "dsi_clk.h"
#include "dsi_pwr.h"
#include "sde_dbg.h"
#include "dsi_parser.h"

#define to_dsi_display(x) container_of(x, struct dsi_display, host)
#define INT_BASE_10 10
#define NO_OVERRIDE -1

#define MISR_BUFF_SIZE	256
#define ESD_MODE_STRING_MAX_LEN 256

#define MAX_NAME_SIZE	64

#define DSI_CLOCK_BITRATE_RADIX 10

static char dsi_display_primary[MAX_CMDLINE_PARAM_LEN];
static char dsi_display_secondary[MAX_CMDLINE_PARAM_LEN];
static struct dsi_display_boot_param boot_displays[MAX_DSI_ACTIVE_DISPLAY] = {
	{.boot_param = dsi_display_primary},
	{.boot_param = dsi_display_secondary},
};

static const struct of_device_id dsi_display_dt_match[] = {
	{.compatible = "qcom,dsi-display"},
	{}
};

static void dsi_display_mask_ctrl_error_interrupts(struct dsi_display *display,
			u32 mask, bool enable)
{
	int i;
	struct dsi_display_ctrl *ctrl;

	if (!display)
		return;

	for (i = 0; (i < display->ctrl_count) &&
			(i < MAX_DSI_CTRLS_PER_DISPLAY); i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl)
			continue;
		dsi_ctrl_mask_error_status_interrupts(ctrl->ctrl, mask, enable);
	}
}

static void dsi_display_set_ctrl_esd_check_flag(struct dsi_display *display,
			bool enable)
{
	int i;
	struct dsi_display_ctrl *ctrl;

	if (!display)
		return;

	for (i = 0; (i < display->ctrl_count) &&
			(i < MAX_DSI_CTRLS_PER_DISPLAY); i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl)
			continue;
		ctrl->ctrl->esd_check_underway = enable;
	}
}

static void dsi_display_ctrl_irq_update(struct dsi_display *display, bool en)
{
	int i;
	struct dsi_display_ctrl *ctrl;

	if (!display)
		return;

	for (i = 0; (i < display->ctrl_count) &&
			(i < MAX_DSI_CTRLS_PER_DISPLAY); i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl)
			continue;
		dsi_ctrl_irq_update(ctrl->ctrl, en);
	}
}

void dsi_rect_intersect(const struct dsi_rect *r1,
		const struct dsi_rect *r2,
		struct dsi_rect *result)
{
	int l, t, r, b;

	if (!r1 || !r2 || !result)
		return;

	l = max(r1->x, r2->x);
	t = max(r1->y, r2->y);
	r = min((r1->x + r1->w), (r2->x + r2->w));
	b = min((r1->y + r1->h), (r2->y + r2->h));

	if (r <= l || b <= t) {
		memset(result, 0, sizeof(*result));
	} else {
		result->x = l;
		result->y = t;
		result->w = r - l;
		result->h = b - t;
	}
}

int dsi_display_set_backlight(struct drm_connector *connector,
		void *display, u32 bl_lvl)
{
	struct dsi_display *dsi_display = display;
	struct dsi_panel *panel;
	u32 bl_scale, bl_scale_ad;
	u64 bl_temp;
	int rc = 0;

	if (dsi_display == NULL || dsi_display->panel == NULL)
		return -EINVAL;

	panel = dsi_display->panel;

	mutex_lock(&panel->panel_lock);
	if (!dsi_panel_initialized(panel)) {
		rc = -EINVAL;
		goto error;
	}

	panel->bl_config.bl_level = bl_lvl;

	/* scale backlight */
	bl_scale = panel->bl_config.bl_scale;
	bl_temp = bl_lvl * bl_scale / MAX_BL_SCALE_LEVEL;

	bl_scale_ad = panel->bl_config.bl_scale_ad;
	bl_temp = (u32)bl_temp * bl_scale_ad / MAX_AD_BL_SCALE_LEVEL;

	pr_debug("bl_scale = %u, bl_scale_ad = %u, bl_lvl = %u\n",
		bl_scale, bl_scale_ad, (u32)bl_temp);

	rc = dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI core clocks, rc=%d\n",
		       dsi_display->name, rc);
		goto error;
	}

	rc = dsi_panel_set_backlight(panel, (u32)bl_temp);
	if (rc)
		pr_err("unable to set backlight\n");

	rc = dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);
	if (rc) {
		pr_err("[%s] failed to disable DSI core clocks, rc=%d\n",
		       dsi_display->name, rc);
		goto error;
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

static int dsi_display_cmd_engine_enable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	mutex_lock(&m_ctrl->ctrl->ctrl_lock);

	if (display->cmd_engine_refcount > 0) {
		display->cmd_engine_refcount++;
		goto done;
	}

	rc = dsi_ctrl_set_cmd_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_ON);
	if (rc) {
		pr_err("[%s] failed to enable cmd engine, rc=%d\n",
		       display->name, rc);
		goto done;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_cmd_engine_state(ctrl->ctrl,
						   DSI_CTRL_ENGINE_ON);
		if (rc) {
			pr_err("[%s] failed to enable cmd engine, rc=%d\n",
			       display->name, rc);
			goto error_disable_master;
		}
	}

	display->cmd_engine_refcount++;
	goto done;
error_disable_master:
	(void)dsi_ctrl_set_cmd_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_OFF);
done:
	mutex_unlock(&m_ctrl->ctrl->ctrl_lock);
	return rc;
}

static int dsi_display_cmd_engine_disable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	mutex_lock(&m_ctrl->ctrl->ctrl_lock);

	if (display->cmd_engine_refcount == 0) {
		pr_err("[%s] Invalid refcount\n", display->name);
		goto done;
	} else if (display->cmd_engine_refcount > 1) {
		display->cmd_engine_refcount--;
		goto done;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_cmd_engine_state(ctrl->ctrl,
						   DSI_CTRL_ENGINE_OFF);
		if (rc)
			pr_err("[%s] failed to enable cmd engine, rc=%d\n",
			       display->name, rc);
	}

	rc = dsi_ctrl_set_cmd_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_OFF);
	if (rc) {
		pr_err("[%s] failed to enable cmd engine, rc=%d\n",
		       display->name, rc);
		goto error;
	}

error:
	display->cmd_engine_refcount = 0;
done:
	mutex_unlock(&m_ctrl->ctrl->ctrl_lock);
	return rc;
}

static void dsi_display_aspace_cb_locked(void *cb_data, bool is_detach)
{
	struct dsi_display *display;
	struct dsi_display_ctrl *display_ctrl;
	int rc, cnt;

	if (!cb_data) {
		pr_err("aspace cb called with invalid cb_data\n");
		return;
	}
	display = (struct dsi_display *)cb_data;

	/*
	 * acquire panel_lock to make sure no commands are in-progress
	 * while detaching the non-secure context banks
	 */
	dsi_panel_acquire_panel_lock(display->panel);

	if (is_detach) {
		/* invalidate the stored iova */
		display->cmd_buffer_iova = 0;

		/* return the virtual address mapping */
		msm_gem_put_vaddr(display->tx_cmd_buf);
		msm_gem_vunmap(display->tx_cmd_buf, OBJ_LOCK_NORMAL);

	} else {
		rc = msm_gem_get_iova(display->tx_cmd_buf,
				display->aspace, &(display->cmd_buffer_iova));
		if (rc) {
			pr_err("failed to get the iova rc %d\n", rc);
			goto end;
		}

		display->vaddr =
			(void *) msm_gem_get_vaddr(display->tx_cmd_buf);

		if (IS_ERR_OR_NULL(display->vaddr)) {
			pr_err("failed to get va rc %d\n", rc);
			goto end;
		}
	}

	for (cnt = 0; cnt < display->ctrl_count; cnt++) {
		display_ctrl = &display->ctrl[cnt];
		display_ctrl->ctrl->cmd_buffer_size = display->cmd_buffer_size;
		display_ctrl->ctrl->cmd_buffer_iova = display->cmd_buffer_iova;
		display_ctrl->ctrl->vaddr = display->vaddr;
		display_ctrl->ctrl->secure_mode = is_detach;
	}

end:
	/* release panel_lock */
	dsi_panel_release_panel_lock(display->panel);
}

static irqreturn_t dsi_display_panel_te_irq_handler(int irq, void *data)
{
	struct dsi_display *display = (struct dsi_display *)data;

	/*
	 * This irq handler is used for sole purpose of identifying
	 * ESD attacks on panel and we can safely assume IRQ_HANDLED
	 * in case of display not being initialized yet
	 */
	if (!display)
		return IRQ_HANDLED;

	complete_all(&display->esd_te_gate);
	return IRQ_HANDLED;
}

static void dsi_display_change_te_irq_status(struct dsi_display *display,
					bool enable)
{
	if (!display) {
		pr_err("Invalid params\n");
		return;
	}

	/* Handle unbalanced irq enable/disbale calls */
	if (enable && !display->is_te_irq_enabled) {
		enable_irq(gpio_to_irq(display->disp_te_gpio));
		display->is_te_irq_enabled = true;
	} else if (!enable && display->is_te_irq_enabled) {
		disable_irq(gpio_to_irq(display->disp_te_gpio));
		display->is_te_irq_enabled = false;
	}
}

static void dsi_display_register_te_irq(struct dsi_display *display)
{
	int rc = 0;
	struct platform_device *pdev;
	struct device *dev;

	pdev = display->pdev;
	if (!pdev) {
		pr_err("invalid platform device\n");
		return;
	}

	dev = &pdev->dev;
	if (!dev) {
		pr_err("invalid device\n");
		return;
	}

	if (!gpio_is_valid(display->disp_te_gpio)) {
		rc = -EINVAL;
		goto error;
	}

	init_completion(&display->esd_te_gate);

	rc = devm_request_irq(dev, gpio_to_irq(display->disp_te_gpio),
			dsi_display_panel_te_irq_handler, IRQF_TRIGGER_FALLING,
			"TE_GPIO", display);
	if (rc) {
		pr_err("TE request_irq failed for ESD rc:%d\n", rc);
		goto error;
	}

	disable_irq(gpio_to_irq(display->disp_te_gpio));
	display->is_te_irq_enabled = false;

	return;

error:
	/* disable the TE based ESD check */
	pr_warn("Unable to register for TE IRQ\n");
	if (display->panel->esd_config.status_mode == ESD_MODE_PANEL_TE)
		display->panel->esd_config.esd_enabled = false;
}

static bool dsi_display_is_te_based_esd(struct dsi_display *display)
{
	u32 status_mode = 0;

	if (!display->panel) {
		pr_err("Invalid panel data\n");
		return false;
	}

	status_mode = display->panel->esd_config.status_mode;

	if (status_mode == ESD_MODE_PANEL_TE &&
			gpio_is_valid(display->disp_te_gpio))
		return true;
	return false;
}

/* Allocate memory for cmd dma tx buffer */
static int dsi_host_alloc_cmd_tx_buffer(struct dsi_display *display)
{
	int rc = 0, cnt = 0;
	struct dsi_display_ctrl *display_ctrl;

	display->tx_cmd_buf = msm_gem_new(display->drm_dev,
			SZ_4K,
			MSM_BO_UNCACHED);

	if ((display->tx_cmd_buf) == NULL) {
		pr_err("Failed to allocate cmd tx buf memory\n");
		rc = -ENOMEM;
		goto error;
	}

	display->cmd_buffer_size = SZ_4K;

	display->aspace = msm_gem_smmu_address_space_get(
			display->drm_dev, MSM_SMMU_DOMAIN_UNSECURE);
	if (!display->aspace) {
		pr_err("failed to get aspace\n");
		rc = -EINVAL;
		goto free_gem;
	}
	/* register to aspace */
	rc = msm_gem_address_space_register_cb(display->aspace,
			dsi_display_aspace_cb_locked, (void *)display);
	if (rc) {
		pr_err("failed to register callback %d", rc);
		goto free_gem;
	}

	rc = msm_gem_get_iova(display->tx_cmd_buf, display->aspace,
				&(display->cmd_buffer_iova));
	if (rc) {
		pr_err("failed to get the iova rc %d\n", rc);
		goto free_aspace_cb;
	}

	display->vaddr =
		(void *) msm_gem_get_vaddr(display->tx_cmd_buf);
	if (IS_ERR_OR_NULL(display->vaddr)) {
		pr_err("failed to get va rc %d\n", rc);
		rc = -EINVAL;
		goto put_iova;
	}

	for (cnt = 0; cnt < display->ctrl_count; cnt++) {
		display_ctrl = &display->ctrl[cnt];
		display_ctrl->ctrl->cmd_buffer_size = SZ_4K;
		display_ctrl->ctrl->cmd_buffer_iova =
					display->cmd_buffer_iova;
		display_ctrl->ctrl->vaddr = display->vaddr;
		display_ctrl->ctrl->tx_cmd_buf = display->tx_cmd_buf;
	}

	return rc;

put_iova:
	msm_gem_put_iova(display->tx_cmd_buf, display->aspace);
free_aspace_cb:
	msm_gem_address_space_unregister_cb(display->aspace,
			dsi_display_aspace_cb_locked, display);
free_gem:
	mutex_lock(&display->drm_dev->struct_mutex);
	msm_gem_free_object(display->tx_cmd_buf);
	mutex_unlock(&display->drm_dev->struct_mutex);
error:
	return rc;
}

static bool dsi_display_validate_reg_read(struct dsi_panel *panel)
{
	int i, j = 0;
	int len = 0, *lenp;
	int group = 0, count = 0;
	struct drm_panel_esd_config *config;

	if (!panel)
		return false;

	config = &(panel->esd_config);

	lenp = config->status_valid_params ?: config->status_cmds_rlen;
	count = config->status_cmd.count;

	for (i = 0; i < count; i++)
		len += lenp[i];

	for (i = 0; i < len; i++)
		j += len;

	for (j = 0; j < config->groups; ++j) {
		for (i = 0; i < len; ++i) {
			if (config->return_buf[i] !=
				config->status_value[group + i])
				break;
		}

		if (i == len)
			return true;
		group += len;
	}

	return false;
}

static void dsi_display_parse_te_gpio(struct dsi_display *display)
{
	struct platform_device *pdev;
	struct device *dev;

	pdev = display->pdev;
	if (!pdev) {
		pr_err("Inavlid platform device\n");
		return;
	}

	dev = &pdev->dev;
	if (!dev) {
		pr_err("Inavlid platform device\n");
		return;
	}

	display->disp_te_gpio = of_get_named_gpio(dev->of_node,
					"qcom,platform-te-gpio", 0);
}

static int dsi_display_read_status(struct dsi_display_ctrl *ctrl,
		struct dsi_panel *panel)
{
	int i, rc = 0, count = 0, start = 0, *lenp;
	struct drm_panel_esd_config *config;
	struct dsi_cmd_desc *cmds;
	u32 flags = 0;

	if (!panel || !ctrl || !ctrl->ctrl)
		return -EINVAL;

	/*
	 * When DSI controller is not in initialized state, we do not want to
	 * report a false ESD failure and hence we defer until next read
	 * happen.
	 */
	if (dsi_ctrl_validate_host_state(ctrl->ctrl))
		return 1;

	config = &(panel->esd_config);
	lenp = config->status_valid_params ?: config->status_cmds_rlen;
	count = config->status_cmd.count;
	cmds = config->status_cmd.cmds;
	flags |= (DSI_CTRL_CMD_FETCH_MEMORY | DSI_CTRL_CMD_READ);

	for (i = 0; i < count; ++i) {
		memset(config->status_buf, 0x0, SZ_4K);
		if (cmds[i].last_command) {
			cmds[i].msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;
			flags |= DSI_CTRL_CMD_LAST_COMMAND;
		}
		if (config->status_cmd.state == DSI_CMD_SET_STATE_LP)
			cmds[i].msg.flags |= MIPI_DSI_MSG_USE_LPM;
		cmds[i].msg.rx_buf = config->status_buf;
		cmds[i].msg.rx_len = config->status_cmds_rlen[i];
		rc = dsi_ctrl_cmd_transfer(ctrl->ctrl, &cmds[i].msg, flags);
		if (rc <= 0) {
			pr_err("rx cmd transfer failed rc=%d\n", rc);
			return rc;
		}

		memcpy(config->return_buf + start,
			config->status_buf, lenp[i]);
		start += lenp[i];
	}

	return rc;
}

static int dsi_display_validate_status(struct dsi_display_ctrl *ctrl,
		struct dsi_panel *panel)
{
	int rc = 0;

	rc = dsi_display_read_status(ctrl, panel);
	if (rc <= 0) {
		goto exit;
	} else {
		/*
		 * panel status read successfully.
		 * check for validity of the data read back.
		 */
		rc = dsi_display_validate_reg_read(panel);
		if (!rc) {
			rc = -EINVAL;
			goto exit;
		}
	}

exit:
	return rc;
}

static int dsi_display_status_reg_read(struct dsi_display *display)
{
	int rc = 0, i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	pr_debug(" ++\n");

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	if (display->tx_cmd_buf == NULL) {
		rc = dsi_host_alloc_cmd_tx_buffer(display);
		if (rc) {
			pr_err("failed to allocate cmd tx buffer memory\n");
			goto done;
		}
	}

	rc = dsi_display_cmd_engine_enable(display);
	if (rc) {
		pr_err("cmd engine enable failed\n");
		return -EPERM;
	}

	rc = dsi_display_validate_status(m_ctrl, display->panel);
	if (rc <= 0) {
		pr_err("[%s] read status failed on master,rc=%d\n",
		       display->name, rc);
		goto exit;
	}

	if (!display->panel->sync_broadcast_en)
		goto exit;

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (ctrl == m_ctrl)
			continue;

		rc = dsi_display_validate_status(ctrl, display->panel);
		if (rc <= 0) {
			pr_err("[%s] read status failed on slave,rc=%d\n",
			       display->name, rc);
			goto exit;
		}
	}
exit:
	dsi_display_cmd_engine_disable(display);
done:
	return rc;
}

static int dsi_display_status_bta_request(struct dsi_display *display)
{
	int rc = 0;

	pr_debug(" ++\n");
	/* TODO: trigger SW BTA and wait for acknowledgment */

	return rc;
}

static int dsi_display_status_check_te(struct dsi_display *display)
{
	int rc = 1;
	int const esd_te_timeout = msecs_to_jiffies(3*20);

	dsi_display_change_te_irq_status(display, true);

	reinit_completion(&display->esd_te_gate);
	if (!wait_for_completion_timeout(&display->esd_te_gate,
				esd_te_timeout)) {
		pr_err("TE check failed\n");
		rc = -EINVAL;
	}

	dsi_display_change_te_irq_status(display, false);

	return rc;
}

int dsi_display_check_status(struct drm_connector *connector, void *display,
					bool te_check_override)
{
	struct dsi_display *dsi_display = display;
	struct dsi_panel *panel;
	u32 status_mode;
	int rc = 0x1;
	u32 mask;

	if (!dsi_display || !dsi_display->panel)
		return -EINVAL;

	panel = dsi_display->panel;

	dsi_panel_acquire_panel_lock(panel);

	if (!panel->panel_initialized) {
		pr_debug("Panel not initialized\n");
		dsi_panel_release_panel_lock(panel);
		return rc;
	}

	/* Prevent another ESD check,when ESD recovery is underway */
	if (panel->esd_recovery_pending) {
		dsi_panel_release_panel_lock(panel);
		return rc;
	}

	if (te_check_override && gpio_is_valid(dsi_display->disp_te_gpio))
		status_mode = ESD_MODE_PANEL_TE;
	else
		status_mode = panel->esd_config.status_mode;

	dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
		DSI_ALL_CLKS, DSI_CLK_ON);

	/* Mask error interrupts before attempting ESD read */
	mask = BIT(DSI_FIFO_OVERFLOW) | BIT(DSI_FIFO_UNDERFLOW);
	dsi_display_set_ctrl_esd_check_flag(dsi_display, true);
	dsi_display_mask_ctrl_error_interrupts(dsi_display, mask, true);

	if (status_mode == ESD_MODE_REG_READ) {
		rc = dsi_display_status_reg_read(dsi_display);
	} else if (status_mode == ESD_MODE_SW_BTA) {
		rc = dsi_display_status_bta_request(dsi_display);
	} else if (status_mode == ESD_MODE_PANEL_TE) {
		rc = dsi_display_status_check_te(dsi_display);
	} else {
		pr_warn("unsupported check status mode\n");
		panel->esd_config.esd_enabled = false;
	}

	/* Unmask error interrupts */
	if (rc > 0) {
		dsi_display_set_ctrl_esd_check_flag(dsi_display, false);
		dsi_display_mask_ctrl_error_interrupts(dsi_display, mask,
							false);
	} else {
		/* Handle Panel failures during display disable sequence */
		panel->esd_recovery_pending = true;
	}

	dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
		DSI_ALL_CLKS, DSI_CLK_OFF);
	dsi_panel_release_panel_lock(panel);

	return rc;
}

static int dsi_display_cmd_prepare(const char *cmd_buf, u32 cmd_buf_len,
		struct dsi_cmd_desc *cmd, u8 *payload, u32 payload_len)
{
	int i;

	memset(cmd, 0x00, sizeof(*cmd));
	cmd->msg.type = cmd_buf[0];
	cmd->last_command = (cmd_buf[1] == 1 ? true : false);
	cmd->msg.channel = cmd_buf[2];
	cmd->msg.flags = cmd_buf[3];
	cmd->msg.ctrl = 0;
	cmd->post_wait_ms = cmd->msg.wait_ms = cmd_buf[4];
	cmd->msg.tx_len = ((cmd_buf[5] << 8) | (cmd_buf[6]));

	if (cmd->msg.tx_len > payload_len) {
		pr_err("Incorrect payload length tx_len %ld, payload_len %d\n",
				cmd->msg.tx_len, payload_len);
		return -EINVAL;
	}

	for (i = 0; i < cmd->msg.tx_len; i++)
		payload[i] = cmd_buf[7 + i];

	cmd->msg.tx_buf = payload;
	return 0;
}

static int dsi_display_ctrl_get_host_init_state(struct dsi_display *dsi_display,
		bool *state)
{
	struct dsi_display_ctrl *ctrl;
	int i, rc = -EINVAL;

	for (i = 0 ; i < dsi_display->ctrl_count; i++) {
		ctrl = &dsi_display->ctrl[i];
		rc = dsi_ctrl_get_host_engine_init_state(ctrl->ctrl, state);
		if (rc)
			break;
	}
	return rc;
}

int dsi_display_cmd_transfer(struct drm_connector *connector,
		void *display, const char *cmd_buf,
		u32 cmd_buf_len)
{
	struct dsi_display *dsi_display = display;
	struct dsi_cmd_desc cmd;
	u8 cmd_payload[MAX_CMD_PAYLOAD_SIZE];
	int rc = 0;
	bool state = false;

	if (!dsi_display || !cmd_buf) {
		pr_err("[DSI] invalid params\n");
		return -EINVAL;
	}

	pr_debug("[DSI] Display command transfer\n");

	rc = dsi_display_cmd_prepare(cmd_buf, cmd_buf_len,
			&cmd, cmd_payload, MAX_CMD_PAYLOAD_SIZE);
	if (rc) {
		pr_err("[DSI] command prepare failed. rc %d\n", rc);
		return rc;
	}

	mutex_lock(&dsi_display->display_lock);
	rc = dsi_display_ctrl_get_host_init_state(dsi_display, &state);
	if (rc || !state) {
		pr_err("[DSI] Invalid host state %d rc %d\n",
				state, rc);
		rc = -EPERM;
		goto end;
	}

	rc = dsi_display->host.ops->transfer(&dsi_display->host,
			&cmd.msg);
end:
	mutex_unlock(&dsi_display->display_lock);
	return rc;
}

int dsi_display_soft_reset(void *display)
{
	struct dsi_display *dsi_display;
	struct dsi_display_ctrl *ctrl;
	int rc = 0;
	int i;

	if (!display)
		return -EINVAL;

	dsi_display = display;

	for (i = 0 ; i < dsi_display->ctrl_count; i++) {
		ctrl = &dsi_display->ctrl[i];
		rc = dsi_ctrl_soft_reset(ctrl->ctrl);
		if (rc) {
			pr_err("[%s] failed to soft reset host_%d, rc=%d\n",
					dsi_display->name, i, rc);
			break;
		}
	}

	return rc;
}

enum dsi_pixel_format dsi_display_get_dst_format(
		struct drm_connector *connector,
		void *display)
{
	enum dsi_pixel_format format = DSI_PIXEL_FORMAT_MAX;
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display || !dsi_display->panel) {
		pr_err("Invalid params(s) dsi_display %pK, panel %pK\n",
			dsi_display,
			((dsi_display) ? dsi_display->panel : NULL));
		return format;
	}

	format = dsi_display->panel->host_config.dst_format;
	return format;
}

static void _dsi_display_setup_misr(struct dsi_display *display)
{
	int i;

	for (i = 0; i < display->ctrl_count; i++) {
		dsi_ctrl_setup_misr(display->ctrl[i].ctrl,
				display->misr_enable,
				display->misr_frame_count);
	}
}

/**
 * dsi_display_get_cont_splash_status - Get continuous splash status.
 * @dsi_display:         DSI display handle.
 *
 * Return: boolean to signify whether continuous splash is enabled.
 */
static bool dsi_display_get_cont_splash_status(struct dsi_display *display)
{
	u32 val = 0;
	int i;
	struct dsi_display_ctrl *ctrl;
	struct dsi_ctrl_hw *hw;

	for (i = 0; i < display->ctrl_count ; i++) {
		ctrl = &(display->ctrl[i]);
		if (!ctrl || !ctrl->ctrl)
			continue;

		hw = &(ctrl->ctrl->hw);
		val = hw->ops.get_cont_splash_status(hw);
		if (!val)
			return false;
	}
	return true;
}

int dsi_display_set_power(struct drm_connector *connector,
		int power_mode, void *disp)
{
	struct dsi_display *display = disp;
	int rc = 0;

	if (!display || !display->panel) {
		pr_err("invalid display/panel\n");
		return -EINVAL;
	}

	switch (power_mode) {
	case SDE_MODE_DPMS_LP1:
		rc = dsi_panel_set_lp1(display->panel);
		break;
	case SDE_MODE_DPMS_LP2:
		rc = dsi_panel_set_lp2(display->panel);
		break;
	default:
		rc = dsi_panel_set_nolp(display->panel);
		break;
	}
	return rc;
}

static ssize_t debugfs_dump_info_read(struct file *file,
				      char __user *user_buf,
				      size_t user_len,
				      loff_t *ppos)
{
	struct dsi_display *display = file->private_data;
	char *buf;
	u32 len = 0;
	int i;

	if (!display)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += snprintf(buf + len, (SZ_4K - len), "name = %s\n", display->name);
	len += snprintf(buf + len, (SZ_4K - len),
			"\tResolution = %dx%d\n",
			display->config.video_timing.h_active,
			display->config.video_timing.v_active);

	for (i = 0; i < display->ctrl_count; i++) {
		len += snprintf(buf + len, (SZ_4K - len),
				"\tCTRL_%d:\n\t\tctrl = %s\n\t\tphy = %s\n",
				i, display->ctrl[i].ctrl->name,
				display->ctrl[i].phy->name);
	}

	len += snprintf(buf + len, (SZ_4K - len),
			"\tPanel = %s\n", display->panel->name);

	len += snprintf(buf + len, (SZ_4K - len),
			"\tClock master = %s\n",
			display->ctrl[display->clk_master_idx].ctrl->name);

	if (copy_to_user(user_buf, buf, len)) {
		kfree(buf);
		return -EFAULT;
	}

	*ppos += len;

	kfree(buf);
	return len;
}

static ssize_t debugfs_misr_setup(struct file *file,
				  const char __user *user_buf,
				  size_t user_len,
				  loff_t *ppos)
{
	struct dsi_display *display = file->private_data;
	char *buf;
	int rc = 0;
	size_t len;
	u32 enable, frame_count;

	if (!display)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(MISR_BUFF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	/* leave room for termination char */
	len = min_t(size_t, user_len, MISR_BUFF_SIZE - 1);
	if (copy_from_user(buf, user_buf, len)) {
		rc = -EINVAL;
		goto error;
	}

	buf[len] = '\0'; /* terminate the string */

	if (sscanf(buf, "%u %u", &enable, &frame_count) != 2) {
		rc = -EINVAL;
		goto error;
	}

	display->misr_enable = enable;
	display->misr_frame_count = frame_count;

	mutex_lock(&display->display_lock);
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI core clocks, rc=%d\n",
		       display->name, rc);
		goto unlock;
	}

	_dsi_display_setup_misr(display);

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);
	if (rc) {
		pr_err("[%s] failed to disable DSI core clocks, rc=%d\n",
		       display->name, rc);
		goto unlock;
	}

	rc = user_len;
unlock:
	mutex_unlock(&display->display_lock);
error:
	kfree(buf);
	return rc;
}

static ssize_t debugfs_misr_read(struct file *file,
				 char __user *user_buf,
				 size_t user_len,
				 loff_t *ppos)
{
	struct dsi_display *display = file->private_data;
	char *buf;
	u32 len = 0;
	int rc = 0;
	struct dsi_ctrl *dsi_ctrl;
	int i;
	u32 misr;
	size_t max_len = min_t(size_t, user_len, MISR_BUFF_SIZE);

	if (!display)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(max_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	mutex_lock(&display->display_lock);
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI core clocks, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		dsi_ctrl = display->ctrl[i].ctrl;
		misr = dsi_ctrl_collect_misr(display->ctrl[i].ctrl);

		len += snprintf((buf + len), max_len - len,
			"DSI_%d MISR: 0x%x\n", dsi_ctrl->cell_index, misr);

		if (len >= max_len)
			break;
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);
	if (rc) {
		pr_err("[%s] failed to disable DSI core clocks, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	if (copy_to_user(user_buf, buf, len)) {
		rc = -EFAULT;
		goto error;
	}

	*ppos += len;

error:
	mutex_unlock(&display->display_lock);
	kfree(buf);
	return len;
}

static ssize_t debugfs_esd_trigger_check(struct file *file,
				  const char __user *user_buf,
				  size_t user_len,
				  loff_t *ppos)
{
	struct dsi_display *display = file->private_data;
	char *buf;
	int rc = 0;
	u32 esd_trigger;

	if (!display)
		return -ENODEV;

	if (*ppos)
		return 0;

	if (user_len > sizeof(u32))
		return -EINVAL;

	buf = kzalloc(user_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, user_buf, user_len)) {
		rc = -EINVAL;
		goto error;
	}

	buf[user_len] = '\0'; /* terminate the string */

	if (kstrtouint(buf, 10, &esd_trigger)) {
		rc = -EINVAL;
		goto error;
	}

	if (esd_trigger != 1) {
		rc = -EINVAL;
		goto error;
	}

	display->esd_trigger = esd_trigger;

	if (display->esd_trigger) {
		rc = dsi_panel_trigger_esd_attack(display->panel);
		if (rc) {
			pr_err("Failed to trigger ESD attack\n");
			return rc;
		}
	}

	rc = user_len;
error:
	kfree(buf);
	return rc;
}

static ssize_t debugfs_alter_esd_check_mode(struct file *file,
				  const char __user *user_buf,
				  size_t user_len,
				  loff_t *ppos)
{
	struct dsi_display *display = file->private_data;
	struct drm_panel_esd_config *esd_config;
	char *buf;
	int rc = 0;
	size_t len = min_t(size_t, user_len, ESD_MODE_STRING_MAX_LEN);

	if (!display)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, user_buf, user_len)) {
		rc = -EINVAL;
		goto error;
	}

	buf[len] = '\0'; /* terminate the string */
	if (!display->panel) {
		rc = -EINVAL;
		goto error;
	}

	esd_config = &display->panel->esd_config;
	if (!esd_config) {
		pr_err("Invalid panel esd config\n");
		rc = -EINVAL;
		goto error;
	}

	if (!esd_config->esd_enabled)
		goto error;

	if (!strcmp(buf, "te_signal_check\n")) {
		esd_config->status_mode = ESD_MODE_PANEL_TE;
		dsi_display_change_te_irq_status(display, true);
	}

	if (!strcmp(buf, "reg_read\n")) {
		rc = dsi_panel_parse_esd_reg_read_configs(display->panel);
		if (rc) {
			pr_err("failed to alter esd check mode,rc=%d\n",
						rc);
			rc = user_len;
			goto error;
		}
		esd_config->status_mode = ESD_MODE_REG_READ;
		if (dsi_display_is_te_based_esd(display))
			dsi_display_change_te_irq_status(display, false);
	}

	rc = len;
error:
	kfree(buf);
	return rc;
}

static ssize_t debugfs_read_esd_check_mode(struct file *file,
				 char __user *user_buf,
				 size_t user_len,
				 loff_t *ppos)
{
	struct dsi_display *display = file->private_data;
	struct drm_panel_esd_config *esd_config;
	char *buf;
	int rc = 0;
	size_t len = min_t(size_t, user_len, ESD_MODE_STRING_MAX_LEN);

	if (!display)
		return -ENODEV;

	if (*ppos)
		return 0;

	if (!display->panel) {
		pr_err("invalid panel data\n");
		return -EINVAL;
	}

	buf = kzalloc(len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	esd_config = &display->panel->esd_config;
	if (!esd_config) {
		pr_err("Invalid panel esd config\n");
		rc = -EINVAL;
		goto error;
	}

	if (!esd_config->esd_enabled) {
		rc = snprintf(buf, len, "ESD feature not enabled");
		goto output_mode;
	}

	if (esd_config->status_mode == ESD_MODE_REG_READ)
		rc = snprintf(buf, len, "reg_read");

	if (esd_config->status_mode == ESD_MODE_PANEL_TE)
		rc = snprintf(buf, len, "te_signal_check");

output_mode:
	if (!rc) {
		rc = -EINVAL;
		goto error;
	}

	if (copy_to_user(user_buf, buf, len)) {
		rc = -EFAULT;
		goto error;
	}

	*ppos += len;

error:
	kfree(buf);
	return len;
}

static const struct file_operations dump_info_fops = {
	.open = simple_open,
	.read = debugfs_dump_info_read,
};

static const struct file_operations misr_data_fops = {
	.open = simple_open,
	.read = debugfs_misr_read,
	.write = debugfs_misr_setup,
};

static const struct file_operations esd_trigger_fops = {
	.open = simple_open,
	.write = debugfs_esd_trigger_check,
};

static const struct file_operations esd_check_mode_fops = {
	.open = simple_open,
	.write = debugfs_alter_esd_check_mode,
	.read = debugfs_read_esd_check_mode,
};

static int dsi_display_debugfs_init(struct dsi_display *display)
{
	int rc = 0;
	struct dentry *dir, *dump_file, *misr_data;
	char name[MAX_NAME_SIZE];
	int i;

	dir = debugfs_create_dir(display->name, NULL);
	if (IS_ERR_OR_NULL(dir)) {
		rc = PTR_ERR(dir);
		pr_err("[%s] debugfs create dir failed, rc = %d\n",
		       display->name, rc);
		goto error;
	}

	dump_file = debugfs_create_file("dump_info",
					0400,
					dir,
					display,
					&dump_info_fops);
	if (IS_ERR_OR_NULL(dump_file)) {
		rc = PTR_ERR(dump_file);
		pr_err("[%s] debugfs create dump info file failed, rc=%d\n",
		       display->name, rc);
		goto error_remove_dir;
	}

	dump_file = debugfs_create_file("esd_trigger",
					0644,
					dir,
					display,
					&esd_trigger_fops);
	if (IS_ERR_OR_NULL(dump_file)) {
		rc = PTR_ERR(dump_file);
		pr_err("[%s] debugfs for esd trigger file failed, rc=%d\n",
		       display->name, rc);
		goto error_remove_dir;
	}

	dump_file = debugfs_create_file("esd_check_mode",
					0644,
					dir,
					display,
					&esd_check_mode_fops);
	if (IS_ERR_OR_NULL(dump_file)) {
		rc = PTR_ERR(dump_file);
		pr_err("[%s] debugfs for esd check mode failed, rc=%d\n",
		       display->name, rc);
		goto error_remove_dir;
	}

	misr_data = debugfs_create_file("misr_data",
					0600,
					dir,
					display,
					&misr_data_fops);
	if (IS_ERR_OR_NULL(misr_data)) {
		rc = PTR_ERR(misr_data);
		pr_err("[%s] debugfs create misr datafile failed, rc=%d\n",
		       display->name, rc);
		goto error_remove_dir;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		struct msm_dsi_phy *phy = display->ctrl[i].phy;

		if (!phy || !phy->name)
			continue;

		snprintf(name, ARRAY_SIZE(name),
				"%s_allow_phy_power_off", phy->name);
		dump_file = debugfs_create_bool(name, 0600, dir,
				&phy->allow_phy_power_off);
		if (IS_ERR_OR_NULL(dump_file)) {
			rc = PTR_ERR(dump_file);
			pr_err("[%s] debugfs create %s failed, rc=%d\n",
			       display->name, name, rc);
			goto error_remove_dir;
		}

		snprintf(name, ARRAY_SIZE(name),
				"%s_regulator_min_datarate_bps", phy->name);
		dump_file = debugfs_create_u32(name, 0600, dir,
				&phy->regulator_min_datarate_bps);
		if (IS_ERR_OR_NULL(dump_file)) {
			rc = PTR_ERR(dump_file);
			pr_err("[%s] debugfs create %s failed, rc=%d\n",
			       display->name, name, rc);
			goto error_remove_dir;
		}
	}

	if (!debugfs_create_bool("ulps_enable", 0600, dir,
			&display->panel->ulps_enabled)) {
		pr_err("[%s] debugfs create ulps enable file failed\n",
		       display->name);
		goto error_remove_dir;
	}

	if (!debugfs_create_bool("ulps_suspend_enable", 0600, dir,
			&display->panel->ulps_suspend_enabled)) {
		pr_err("[%s] debugfs create ulps-suspend enable file failed\n",
		       display->name);
		goto error_remove_dir;
	}

	display->root = dir;
	dsi_parser_dbg_init(display->parser, dir);

	return rc;
error_remove_dir:
	debugfs_remove(dir);
error:
	return rc;
}

static int dsi_display_debugfs_deinit(struct dsi_display *display)
{
	debugfs_remove_recursive(display->root);

	return 0;
}

static void adjust_timing_by_ctrl_count(const struct dsi_display *display,
					struct dsi_display_mode *mode)
{
	if (display->ctrl_count > 1) {
		mode->timing.h_active /= display->ctrl_count;
		mode->timing.h_front_porch /= display->ctrl_count;
		mode->timing.h_sync_width /= display->ctrl_count;
		mode->timing.h_back_porch /= display->ctrl_count;
		mode->timing.h_skew /= display->ctrl_count;
		mode->pixel_clk_khz /= display->ctrl_count;
	}
}

static int dsi_display_is_ulps_req_valid(struct dsi_display *display,
		bool enable)
{
	/* TODO: make checks based on cont. splash */
	int splash_enabled = false;

	pr_debug("checking ulps req validity\n");

	if (!dsi_panel_ulps_feature_enabled(display->panel) &&
			!display->panel->ulps_suspend_enabled) {
		pr_debug("%s: ULPS feature is not enabled\n", __func__);
		return false;
	}

	if (!dsi_panel_initialized(display->panel) &&
			!display->panel->ulps_suspend_enabled) {
		pr_debug("%s: panel not yet initialized\n", __func__);
		return false;
	}

	if (enable && display->ulps_enabled) {
		pr_debug("ULPS already enabled\n");
		return false;
	} else if (!enable && !display->ulps_enabled) {
		pr_debug("ULPS already disabled\n");
		return false;
	}

	/*
	 * No need to enter ULPS when transitioning from splash screen to
	 * boot animation since it is expected that the clocks would be turned
	 * right back on.
	 */
	if (enable && splash_enabled)
		return false;

	return true;
}


/**
 * dsi_display_set_ulps() - set ULPS state for DSI lanes.
 * @dsi_display:         DSI display handle.
 * @enable:           enable/disable ULPS.
 *
 * ULPS can be enabled/disabled after DSI host engine is turned on.
 *
 * Return: error code.
 */
static int dsi_display_set_ulps(struct dsi_display *display, bool enable)
{
	int rc = 0;
	int i = 0;
	struct dsi_display_ctrl *m_ctrl, *ctrl;


	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (!dsi_display_is_ulps_req_valid(display, enable)) {
		pr_debug("%s: skipping ULPS config, enable=%d\n",
			__func__, enable);
		return 0;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	rc = dsi_ctrl_set_ulps(m_ctrl->ctrl, enable);
	if (rc) {
		pr_err("Ulps controller state change(%d) failed\n", enable);
		return rc;
	}

	rc = dsi_phy_set_ulps(m_ctrl->phy, &display->config, enable,
				display->clamp_enabled);
	if (rc) {
		pr_err("Ulps PHY state change(%d) failed\n", enable);
		return rc;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_ulps(ctrl->ctrl, enable);
		if (rc) {
			pr_err("Ulps controller state change(%d) failed\n",
				enable);
			return rc;
		}

		rc = dsi_phy_set_ulps(ctrl->phy, &display->config, enable,
					display->clamp_enabled);
		if (rc) {
			pr_err("Ulps PHY state change(%d) failed\n", enable);
			return rc;
		}

	}
	display->ulps_enabled = enable;
	return 0;
}

/**
 * dsi_display_set_clamp() - set clamp state for DSI IO.
 * @dsi_display:         DSI display handle.
 * @enable:           enable/disable clamping.
 *
 * Return: error code.
 */
static int dsi_display_set_clamp(struct dsi_display *display, bool enable)
{
	int rc = 0;
	int i = 0;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	bool ulps_enabled = false;


	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	ulps_enabled = display->ulps_enabled;

	/*
	 * Clamp control can be either through the DSI controller or
	 * the DSI PHY depending on hardware variation
	 */
	rc = dsi_ctrl_set_clamp_state(m_ctrl->ctrl, enable, ulps_enabled);
	if (rc) {
		pr_err("DSI ctrl clamp state change(%d) failed\n", enable);
		return rc;
	}

	rc = dsi_phy_set_clamp_state(m_ctrl->phy, enable);
	if (rc) {
		pr_err("DSI phy clamp state change(%d) failed\n", enable);
		return rc;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_clamp_state(ctrl->ctrl, enable, ulps_enabled);
		if (rc) {
			pr_err("DSI Clamp state change(%d) failed\n", enable);
			return rc;
		}

		rc = dsi_phy_set_clamp_state(ctrl->phy, enable);
		if (rc) {
			pr_err("DSI phy clamp state change(%d) failed\n",
				enable);
			return rc;
		}

		pr_debug("Clamps %s for ctrl%d\n",
			enable ? "enabled" : "disabled", i);
	}

	display->clamp_enabled = enable;
	return 0;
}

/**
 * dsi_display_setup_ctrl() - setup DSI controller.
 * @dsi_display:         DSI display handle.
 *
 * Return: error code.
 */
static int dsi_display_ctrl_setup(struct dsi_display *display)
{
	int rc = 0;
	int i = 0;
	struct dsi_display_ctrl *ctrl, *m_ctrl;


	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	rc = dsi_ctrl_setup(m_ctrl->ctrl);
	if (rc) {
		pr_err("DSI controller setup failed\n");
		return rc;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_setup(ctrl->ctrl);
		if (rc) {
			pr_err("DSI controller setup failed\n");
			return rc;
		}
	}
	return 0;
}

static int dsi_display_phy_enable(struct dsi_display *display);

/**
 * dsi_display_phy_idle_on() - enable DSI PHY while coming out of idle screen.
 * @dsi_display:         DSI display handle.
 * @mmss_clamp:          True if clamp is enabled.
 *
 * Return: error code.
 */
static int dsi_display_phy_idle_on(struct dsi_display *display,
		bool mmss_clamp)
{
	int rc = 0;
	int i = 0;
	struct dsi_display_ctrl *m_ctrl, *ctrl;


	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (mmss_clamp && !display->phy_idle_power_off) {
		dsi_display_phy_enable(display);
		return 0;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	rc = dsi_phy_idle_ctrl(m_ctrl->phy, true);
	if (rc) {
		pr_err("DSI controller setup failed\n");
		return rc;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_phy_idle_ctrl(ctrl->phy, true);
		if (rc) {
			pr_err("DSI controller setup failed\n");
			return rc;
		}
	}
	display->phy_idle_power_off = false;
	return 0;
}

/**
 * dsi_display_phy_idle_off() - disable DSI PHY while going to idle screen.
 * @dsi_display:         DSI display handle.
 *
 * Return: error code.
 */
static int dsi_display_phy_idle_off(struct dsi_display *display)
{
	int rc = 0;
	int i = 0;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		struct msm_dsi_phy *phy = display->ctrl[i].phy;

		if (!phy)
			continue;

		if (!phy->allow_phy_power_off) {
			pr_debug("phy doesn't support this feature\n");
			return 0;
		}
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	rc = dsi_phy_idle_ctrl(m_ctrl->phy, false);
	if (rc) {
		pr_err("[%s] failed to enable cmd engine, rc=%d\n",
		       display->name, rc);
		return rc;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_phy_idle_ctrl(ctrl->phy, false);
		if (rc) {
			pr_err("DSI controller setup failed\n");
			return rc;
		}
	}
	display->phy_idle_power_off = true;
	return 0;
}

void dsi_display_enable_event(struct drm_connector *connector,
		struct dsi_display *display,
		uint32_t event_idx, struct dsi_event_cb_info *event_info,
		bool enable)
{
	uint32_t irq_status_idx = DSI_STATUS_INTERRUPT_COUNT;
	int i;

	if (!display) {
		pr_err("invalid display\n");
		return;
	}

	if (event_info)
		event_info->event_idx = event_idx;

	switch (event_idx) {
	case SDE_CONN_EVENT_VID_DONE:
		irq_status_idx = DSI_SINT_VIDEO_MODE_FRAME_DONE;
		break;
	case SDE_CONN_EVENT_CMD_DONE:
		irq_status_idx = DSI_SINT_CMD_FRAME_DONE;
		break;
	case SDE_CONN_EVENT_VID_FIFO_OVERFLOW:
	case SDE_CONN_EVENT_CMD_FIFO_UNDERFLOW:
		if (event_info) {
			for (i = 0; i < display->ctrl_count; i++)
				display->ctrl[i].ctrl->recovery_cb =
							*event_info;
		}
	default:
		/* nothing to do */
		pr_debug("[%s] unhandled event %d\n", display->name, event_idx);
		return;
	}

	if (enable) {
		for (i = 0; i < display->ctrl_count; i++)
			dsi_ctrl_enable_status_interrupt(
					display->ctrl[i].ctrl, irq_status_idx,
					event_info);
	} else {
		for (i = 0; i < display->ctrl_count; i++)
			dsi_ctrl_disable_status_interrupt(
					display->ctrl[i].ctrl, irq_status_idx);
	}
}

/**
 * dsi_config_host_engine_state_for_cont_splash()- update host engine state
 *                                                 during continuous splash.
 * @display: Handle to dsi display
 *
 */
static void dsi_config_host_engine_state_for_cont_splash
					(struct dsi_display *display)
{
	int i;
	struct dsi_display_ctrl *ctrl;
	enum dsi_engine_state host_state = DSI_CTRL_ENGINE_ON;

	/* Sequence does not matter for split dsi usecases */
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;

		dsi_ctrl_update_host_engine_state_for_cont_splash(ctrl->ctrl,
							host_state);
	}
}

static int dsi_display_ctrl_power_on(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	/* Sequence does not matter for split dsi usecases */
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;

		rc = dsi_ctrl_set_power_state(ctrl->ctrl,
					      DSI_CTRL_POWER_VREG_ON);
		if (rc) {
			pr_err("[%s] Failed to set power state, rc=%d\n",
			       ctrl->ctrl->name, rc);
			goto error;
		}
	}

	return rc;
error:
	for (i = i - 1; i >= 0; i--) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;
		(void)dsi_ctrl_set_power_state(ctrl->ctrl,
			DSI_CTRL_POWER_VREG_OFF);
	}
	return rc;
}

static int dsi_display_ctrl_power_off(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	/* Sequence does not matter for split dsi usecases */
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;

		rc = dsi_ctrl_set_power_state(ctrl->ctrl,
			DSI_CTRL_POWER_VREG_OFF);
		if (rc) {
			pr_err("[%s] Failed to power off, rc=%d\n",
			       ctrl->ctrl->name, rc);
			goto error;
		}
	}
error:
	return rc;
}

static void dsi_display_parse_cmdline_topology(struct dsi_display *display,
					unsigned int display_type)
{
	char *boot_str = NULL;
	char *str = NULL;
	char *sw_te = NULL;
	unsigned long value;

	if (display_type >= MAX_DSI_ACTIVE_DISPLAY) {
		pr_err("display_type=%d not supported\n", display_type);
		return;
	}

	if (display_type == DSI_PRIMARY)
		boot_str = dsi_display_primary;
	else
		boot_str = dsi_display_secondary;

	sw_te = strnstr(boot_str, ":swte", strlen(boot_str));
	if (sw_te)
		display->sw_te_using_wd = true;

	str = strnstr(boot_str, ":config", strlen(boot_str));
	if (!str)
		return;

	if (kstrtol(str + strlen(":config"), INT_BASE_10,
				(unsigned long *)&value)) {
		pr_err("invalid config index override: %s\n", boot_str);
		return;
	}
	display->cmdline_topology = value;

	str = strnstr(boot_str, ":timing", strlen(boot_str));
	if (!str)
		return;

	if (kstrtol(str + strlen(":timing"), INT_BASE_10,
				(unsigned long *)&value)) {
		pr_err("invalid timing index override: %s. resetting both timing and config\n",
			boot_str);
		display->cmdline_topology = NO_OVERRIDE;
		return;
	}
	display->cmdline_timing = value;
}

/**
 * dsi_display_parse_boot_display_selection()- Parse DSI boot display name
 *
 * Return:	returns error status
 */
static int dsi_display_parse_boot_display_selection(void)
{
	char *pos = NULL;
	char disp_buf[MAX_CMDLINE_PARAM_LEN] = {'\0'};
	int i, j;

	for (i = 0; i < MAX_DSI_ACTIVE_DISPLAY; i++) {
		strlcpy(disp_buf, boot_displays[i].boot_param,
			MAX_CMDLINE_PARAM_LEN);

		pos = strnstr(disp_buf, ":", MAX_CMDLINE_PARAM_LEN);

		/* Use ':' as a delimiter to retrieve the display name */
		if (!pos) {
			pr_debug("display name[%s]is not valid\n", disp_buf);
			continue;
		}

		for (j = 0; (disp_buf + j) < pos; j++)
			boot_displays[i].name[j] = *(disp_buf + j);

		boot_displays[i].name[j] = '\0';

		boot_displays[i].boot_disp_en = true;
	}

	return 0;
}

static int dsi_display_phy_power_on(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	/* Sequence does not matter for split dsi usecases */
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;

		rc = dsi_phy_set_power_state(ctrl->phy, true);
		if (rc) {
			pr_err("[%s] Failed to set power state, rc=%d\n",
			       ctrl->phy->name, rc);
			goto error;
		}
	}

	return rc;
error:
	for (i = i - 1; i >= 0; i--) {
		ctrl = &display->ctrl[i];
		if (!ctrl->phy)
			continue;
		(void)dsi_phy_set_power_state(ctrl->phy, false);
	}
	return rc;
}

static int dsi_display_phy_power_off(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	/* Sequence does not matter for split dsi usecases */
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->phy)
			continue;

		rc = dsi_phy_set_power_state(ctrl->phy, false);
		if (rc) {
			pr_err("[%s] Failed to power off, rc=%d\n",
			       ctrl->ctrl->name, rc);
			goto error;
		}
	}
error:
	return rc;
}

static int dsi_display_set_clk_src(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	/*
	 * In case of split DSI usecases, the clock for master controller should
	 * be enabled before the other controller. Master controller in the
	 * clock context refers to the controller that sources the clock.
	 */
	m_ctrl = &display->ctrl[display->clk_master_idx];

	rc = dsi_ctrl_set_clock_source(m_ctrl->ctrl,
		   &display->clock_info.src_clks);
	if (rc) {
		pr_err("[%s] failed to set source clocks for master, rc=%d\n",
			   display->name, rc);
		return rc;
	}

	/* Turn on rest of the controllers */
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_clock_source(ctrl->ctrl,
			   &display->clock_info.src_clks);
		if (rc) {
			pr_err("[%s] failed to set source clocks, rc=%d\n",
				   display->name, rc);
			return rc;
		}
	}
	return 0;
}

static int dsi_display_phy_reset_config(struct dsi_display *display,
		bool enable)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	for (i = 0 ; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_phy_reset_config(ctrl->ctrl, enable);
		if (rc) {
			pr_err("[%s] failed to %s phy reset, rc=%d\n",
			       display->name, enable ? "mask" : "unmask", rc);
			return rc;
		}
	}
	return 0;
}

static void dsi_display_toggle_resync_fifo(struct dsi_display *display)
{
	struct dsi_display_ctrl *ctrl;
	int i;

	if (!display)
		return;

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		dsi_phy_toggle_resync_fifo(ctrl->phy);
	}

	/*
	 * After retime buffer synchronization we need to turn of clk_en_sel
	 * bit on each phy.
	 */
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		dsi_phy_reset_clk_en_sel(ctrl->phy);
	}

}

static int dsi_display_ctrl_update(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	for (i = 0 ; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_host_timing_update(ctrl->ctrl);
		if (rc) {
			pr_err("[%s] failed to update host_%d, rc=%d\n",
				   display->name, i, rc);
			goto error_host_deinit;
		}
	}

	return 0;
error_host_deinit:
	for (i = i - 1; i >= 0; i--) {
		ctrl = &display->ctrl[i];
		(void)dsi_ctrl_host_deinit(ctrl->ctrl);
	}

	return rc;
}

static int dsi_display_ctrl_init(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	/* when ULPS suspend feature is enabled, we will keep the lanes in
	 * ULPS during suspend state and clamp DSI phy. Hence while resuming
	 * we will programe DSI controller as part of core clock enable.
	 * After that we should not re-configure DSI controller again here for
	 * usecases where we are resuming from ulps suspend as it might put
	 * the HW in bad state.
	 */
	if (!display->panel->ulps_suspend_enabled || !display->ulps_enabled) {
		for (i = 0 ; i < display->ctrl_count; i++) {
			ctrl = &display->ctrl[i];
			rc = dsi_ctrl_host_init(ctrl->ctrl,
					display->is_cont_splash_enabled);
			if (rc) {
				pr_err("[%s] failed to init host_%d, rc=%d\n",
				       display->name, i, rc);
				goto error_host_deinit;
			}
		}
	} else {
		for (i = 0 ; i < display->ctrl_count; i++) {
			ctrl = &display->ctrl[i];
			rc = dsi_ctrl_update_host_init_state(ctrl->ctrl, true);
			if (rc)
				pr_debug("host init update failed rc=%d\n", rc);
		}
	}

	return rc;
error_host_deinit:
	for (i = i - 1; i >= 0; i--) {
		ctrl = &display->ctrl[i];
		(void)dsi_ctrl_host_deinit(ctrl->ctrl);
	}
	return rc;
}

static int dsi_display_ctrl_deinit(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	for (i = 0 ; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_host_deinit(ctrl->ctrl);
		if (rc) {
			pr_err("[%s] failed to deinit host_%d, rc=%d\n",
			       display->name, i, rc);
		}
	}

	return rc;
}

static int dsi_display_ctrl_host_enable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	/* Host engine states are already taken care for
	 * continuous splash case
	 */
	if (display->is_cont_splash_enabled) {
		pr_debug("cont splash enabled, host enable not required\n");
		return 0;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	rc = dsi_ctrl_set_host_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_ON);
	if (rc) {
		pr_err("[%s] failed to enable host engine, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_host_engine_state(ctrl->ctrl,
						    DSI_CTRL_ENGINE_ON);
		if (rc) {
			pr_err("[%s] failed to enable sl host engine, rc=%d\n",
			       display->name, rc);
			goto error_disable_master;
		}
	}

	return rc;
error_disable_master:
	(void)dsi_ctrl_set_host_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_OFF);
error:
	return rc;
}

static int dsi_display_ctrl_host_disable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_host_engine_state(ctrl->ctrl,
						    DSI_CTRL_ENGINE_OFF);
		if (rc)
			pr_err("[%s] failed to disable host engine, rc=%d\n",
			       display->name, rc);
	}

	rc = dsi_ctrl_set_host_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_OFF);
	if (rc) {
		pr_err("[%s] failed to disable host engine, rc=%d\n",
		       display->name, rc);
		goto error;
	}

error:
	return rc;
}

static int dsi_display_vid_engine_enable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	m_ctrl = &display->ctrl[display->video_master_idx];

	rc = dsi_ctrl_set_vid_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_ON);
	if (rc) {
		pr_err("[%s] failed to enable vid engine, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_vid_engine_state(ctrl->ctrl,
						   DSI_CTRL_ENGINE_ON);
		if (rc) {
			pr_err("[%s] failed to enable vid engine, rc=%d\n",
			       display->name, rc);
			goto error_disable_master;
		}
	}

	return rc;
error_disable_master:
	(void)dsi_ctrl_set_vid_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_OFF);
error:
	return rc;
}

static int dsi_display_vid_engine_disable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	m_ctrl = &display->ctrl[display->video_master_idx];

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_vid_engine_state(ctrl->ctrl,
						   DSI_CTRL_ENGINE_OFF);
		if (rc)
			pr_err("[%s] failed to disable vid engine, rc=%d\n",
			       display->name, rc);
	}

	rc = dsi_ctrl_set_vid_engine_state(m_ctrl->ctrl, DSI_CTRL_ENGINE_OFF);
	if (rc)
		pr_err("[%s] failed to disable mvid engine, rc=%d\n",
		       display->name, rc);

	return rc;
}

static int dsi_display_phy_enable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	enum dsi_phy_pll_source m_src = DSI_PLL_SOURCE_STANDALONE;

	m_ctrl = &display->ctrl[display->clk_master_idx];
	if (display->ctrl_count > 1)
		m_src = DSI_PLL_SOURCE_NATIVE;

	rc = dsi_phy_enable(m_ctrl->phy,
			    &display->config,
			    m_src,
			    true,
			    display->is_cont_splash_enabled);
	if (rc) {
		pr_err("[%s] failed to enable DSI PHY, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_phy_enable(ctrl->phy,
				    &display->config,
				    DSI_PLL_SOURCE_NON_NATIVE,
				    true,
				    display->is_cont_splash_enabled);
		if (rc) {
			pr_err("[%s] failed to enable DSI PHY, rc=%d\n",
			       display->name, rc);
			goto error_disable_master;
		}
	}

	return rc;

error_disable_master:
	(void)dsi_phy_disable(m_ctrl->phy);
error:
	return rc;
}

static int dsi_display_phy_disable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	m_ctrl = &display->ctrl[display->clk_master_idx];

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_phy_disable(ctrl->phy);
		if (rc)
			pr_err("[%s] failed to disable DSI PHY, rc=%d\n",
			       display->name, rc);
	}

	rc = dsi_phy_disable(m_ctrl->phy);
	if (rc)
		pr_err("[%s] failed to disable DSI PHY, rc=%d\n",
		       display->name, rc);

	return rc;
}

static int dsi_display_wake_up(struct dsi_display *display)
{
	return 0;
}

static int dsi_display_broadcast_cmd(struct dsi_display *display,
				     const struct mipi_dsi_msg *msg)
{
	int rc = 0;
	u32 flags, m_flags;
	struct dsi_display_ctrl *ctrl, *m_ctrl;
	int i;

	m_flags = (DSI_CTRL_CMD_BROADCAST | DSI_CTRL_CMD_BROADCAST_MASTER |
		   DSI_CTRL_CMD_DEFER_TRIGGER | DSI_CTRL_CMD_FETCH_MEMORY);
	flags = (DSI_CTRL_CMD_BROADCAST | DSI_CTRL_CMD_DEFER_TRIGGER |
		 DSI_CTRL_CMD_FETCH_MEMORY);

	if ((msg->flags & MIPI_DSI_MSG_LASTCOMMAND)) {
		flags |= DSI_CTRL_CMD_LAST_COMMAND;
		m_flags |= DSI_CTRL_CMD_LAST_COMMAND;
	}
	/*
	 * 1. Setup commands in FIFO
	 * 2. Trigger commands
	 */
	m_ctrl = &display->ctrl[display->cmd_master_idx];
	rc = dsi_ctrl_cmd_transfer(m_ctrl->ctrl, msg, m_flags);
	if (rc) {
		pr_err("[%s] cmd transfer failed on master,rc=%d\n",
		       display->name, rc);
		goto error;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (ctrl == m_ctrl)
			continue;

		rc = dsi_ctrl_cmd_transfer(ctrl->ctrl, msg, flags);
		if (rc) {
			pr_err("[%s] cmd transfer failed, rc=%d\n",
			       display->name, rc);
			goto error;
		}

		rc = dsi_ctrl_cmd_tx_trigger(ctrl->ctrl, flags);
		if (rc) {
			pr_err("[%s] cmd trigger failed, rc=%d\n",
			       display->name, rc);
			goto error;
		}
	}

	rc = dsi_ctrl_cmd_tx_trigger(m_ctrl->ctrl, m_flags);
	if (rc) {
		pr_err("[%s] cmd trigger failed for master, rc=%d\n",
		       display->name, rc);
		goto error;
	}

error:
	return rc;
}

static int dsi_display_phy_sw_reset(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	/* For continuous splash use case ctrl states are updated
	 * separately and hence we do an early return
	 */
	if (display->is_cont_splash_enabled) {
		pr_debug("cont splash enabled, phy sw reset not required\n");
		return 0;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	rc = dsi_ctrl_phy_sw_reset(m_ctrl->ctrl);
	if (rc) {
		pr_err("[%s] failed to reset phy, rc=%d\n", display->name, rc);
		goto error;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_phy_sw_reset(ctrl->ctrl);
		if (rc) {
			pr_err("[%s] failed to reset phy, rc=%d\n",
			       display->name, rc);
			goto error;
		}
	}

error:
	return rc;
}

static int dsi_host_attach(struct mipi_dsi_host *host,
			   struct mipi_dsi_device *dsi)
{
	return 0;
}

static int dsi_host_detach(struct mipi_dsi_host *host,
			   struct mipi_dsi_device *dsi)
{
	return 0;
}

static ssize_t dsi_host_transfer(struct mipi_dsi_host *host,
				 const struct mipi_dsi_msg *msg)
{
	struct dsi_display *display = to_dsi_display(host);
	int rc = 0, ret = 0;

	if (!host || !msg) {
		pr_err("Invalid params\n");
		return 0;
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable all DSI clocks, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	rc = dsi_display_wake_up(display);
	if (rc) {
		pr_err("[%s] failed to wake up display, rc=%d\n",
		       display->name, rc);
		goto error_disable_clks;
	}

	rc = dsi_display_cmd_engine_enable(display);
	if (rc) {
		pr_err("[%s] failed to enable cmd engine, rc=%d\n",
		       display->name, rc);
		goto error_disable_clks;
	}

	if (display->tx_cmd_buf == NULL) {
		rc = dsi_host_alloc_cmd_tx_buffer(display);
		if (rc) {
			pr_err("failed to allocate cmd tx buffer memory\n");
			goto error_disable_cmd_engine;
		}
	}

	if (display->ctrl_count > 1 && !(msg->flags & MIPI_DSI_MSG_UNICAST)) {
		rc = dsi_display_broadcast_cmd(display, msg);
		if (rc) {
			pr_err("[%s] cmd broadcast failed, rc=%d\n",
			       display->name, rc);
			goto error_disable_cmd_engine;
		}
	} else {
		int ctrl_idx = (msg->flags & MIPI_DSI_MSG_UNICAST) ?
				msg->ctrl : 0;

		rc = dsi_ctrl_cmd_transfer(display->ctrl[ctrl_idx].ctrl, msg,
					  DSI_CTRL_CMD_FETCH_MEMORY);
		if (rc) {
			pr_err("[%s] cmd transfer failed, rc=%d\n",
			       display->name, rc);
			goto error_disable_cmd_engine;
		}
	}

error_disable_cmd_engine:
	ret = dsi_display_cmd_engine_disable(display);
	if (ret) {
		pr_err("[%s]failed to disable DSI cmd engine, rc=%d\n",
				display->name, ret);
	}
error_disable_clks:
	ret = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);
	if (ret) {
		pr_err("[%s] failed to disable all DSI clocks, rc=%d\n",
		       display->name, ret);
	}
error:
	return rc;
}


static struct mipi_dsi_host_ops dsi_host_ops = {
	.attach = dsi_host_attach,
	.detach = dsi_host_detach,
	.transfer = dsi_host_transfer,
};

static int dsi_display_mipi_host_init(struct dsi_display *display)
{
	int rc = 0;
	struct mipi_dsi_host *host = &display->host;

	host->dev = &display->pdev->dev;
	host->ops = &dsi_host_ops;

	rc = mipi_dsi_host_register(host);
	if (rc) {
		pr_err("[%s] failed to register mipi dsi host, rc=%d\n",
		       display->name, rc);
		goto error;
	}

error:
	return rc;
}
static int dsi_display_mipi_host_deinit(struct dsi_display *display)
{
	int rc = 0;
	struct mipi_dsi_host *host = &display->host;

	mipi_dsi_host_unregister(host);

	host->dev = NULL;
	host->ops = NULL;

	return rc;
}

static int dsi_display_clocks_deinit(struct dsi_display *display)
{
	int rc = 0;
	struct dsi_clk_link_set *src = &display->clock_info.src_clks;
	struct dsi_clk_link_set *mux = &display->clock_info.mux_clks;
	struct dsi_clk_link_set *shadow = &display->clock_info.shadow_clks;

	if (src->byte_clk) {
		devm_clk_put(&display->pdev->dev, src->byte_clk);
		src->byte_clk = NULL;
	}

	if (src->pixel_clk) {
		devm_clk_put(&display->pdev->dev, src->pixel_clk);
		src->pixel_clk = NULL;
	}

	if (mux->byte_clk) {
		devm_clk_put(&display->pdev->dev, mux->byte_clk);
		mux->byte_clk = NULL;
	}

	if (mux->pixel_clk) {
		devm_clk_put(&display->pdev->dev, mux->pixel_clk);
		mux->pixel_clk = NULL;
	}

	if (shadow->byte_clk) {
		devm_clk_put(&display->pdev->dev, shadow->byte_clk);
		shadow->byte_clk = NULL;
	}

	if (shadow->pixel_clk) {
		devm_clk_put(&display->pdev->dev, shadow->pixel_clk);
		shadow->pixel_clk = NULL;
	}

	return rc;
}

static bool dsi_display_check_prefix(const char *clk_prefix,
					const char *clk_name)
{
	return !!strnstr(clk_name, clk_prefix, strlen(clk_name));
}

static int dsi_display_get_clocks_count(struct dsi_display *display)
{
	if (display->fw)
		return dsi_parser_count_strings(display->parser_node,
			"qcom,dsi-select-clocks");
	else
		return of_property_count_strings(display->disp_node,
			"qcom,dsi-select-clocks");
}

static void dsi_display_get_clock_name(struct dsi_display *display,
					int index, const char **clk_name)
{
	if (display->fw)
		dsi_parser_read_string_index(display->parser_node,
			"qcom,dsi-select-clocks", index, clk_name);
	else
		of_property_read_string_index(display->disp_node,
			"qcom,dsi-select-clocks", index, clk_name);
}

static int dsi_display_clocks_init(struct dsi_display *display)
{
	int i, rc = 0, num_clk = 0;
	const char *clk_name;
	const char *src_byte = "src_byte", *src_pixel = "src_pixel";
	const char *mux_byte = "mux_byte", *mux_pixel = "mux_pixel";
	const char *shadow_byte = "shadow_byte", *shadow_pixel = "shadow_pixel";
	struct clk *dsi_clk;
	struct dsi_clk_link_set *src = &display->clock_info.src_clks;
	struct dsi_clk_link_set *mux = &display->clock_info.mux_clks;
	struct dsi_clk_link_set *shadow = &display->clock_info.shadow_clks;

	num_clk = dsi_display_get_clocks_count(display);

	pr_debug("clk count=%d\n", num_clk);

	for (i = 0; i < num_clk; i++) {
		dsi_display_get_clock_name(display, i, &clk_name);

		pr_debug("clock name:%s\n", clk_name);

		dsi_clk = devm_clk_get(&display->pdev->dev, clk_name);
		if (IS_ERR_OR_NULL(dsi_clk)) {
			rc = PTR_ERR(dsi_clk);

			pr_err("failed to get %s, rc=%d\n", clk_name, rc);
			goto error;
		}

		if (dsi_display_check_prefix(src_byte, clk_name)) {
			src->byte_clk = dsi_clk;
			continue;
		}

		if (dsi_display_check_prefix(src_pixel, clk_name)) {
			src->pixel_clk = dsi_clk;
			continue;
		}

		if (dsi_display_check_prefix(mux_byte, clk_name)) {
			mux->byte_clk = dsi_clk;
			continue;
		}

		if (dsi_display_check_prefix(mux_pixel, clk_name)) {
			mux->pixel_clk = dsi_clk;
			continue;
		}

		if (dsi_display_check_prefix(shadow_byte, clk_name)) {
			shadow->byte_clk = dsi_clk;
			continue;
		}

		if (dsi_display_check_prefix(shadow_pixel, clk_name)) {
			shadow->pixel_clk = dsi_clk;
			continue;
		}
	}

	return 0;
error:
	(void)dsi_display_clocks_deinit(display);
	return rc;
}

static int dsi_display_clk_ctrl_cb(void *priv,
	struct dsi_clk_ctrl_info clk_state_info)
{
	int rc = 0;
	struct dsi_display *display = NULL;
	void *clk_handle = NULL;

	if (!priv) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	display = priv;

	if (clk_state_info.client == DSI_CLK_REQ_MDP_CLIENT) {
		clk_handle = display->mdp_clk_handle;
	} else if (clk_state_info.client == DSI_CLK_REQ_DSI_CLIENT) {
		clk_handle = display->dsi_clk_handle;
	} else {
		pr_err("invalid clk handle, return error\n");
		return -EINVAL;
	}

	/*
	 * TODO: Wait for CMD_MDP_DONE interrupt if MDP client tries
	 * to turn off DSI clocks.
	 */
	rc = dsi_display_clk_ctrl(clk_handle,
		clk_state_info.clk_type, clk_state_info.clk_state);
	if (rc) {
		pr_err("[%s] failed to %d DSI %d clocks, rc=%d\n",
		       display->name, clk_state_info.clk_state,
		       clk_state_info.clk_type, rc);
		return rc;
	}
	return 0;
}

static void dsi_display_ctrl_isr_configure(struct dsi_display *display, bool en)
{
	int i;
	struct dsi_display_ctrl *ctrl;

	if (!display)
		return;

	for (i = 0; (i < display->ctrl_count) &&
			(i < MAX_DSI_CTRLS_PER_DISPLAY); i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl)
			continue;
		dsi_ctrl_isr_configure(ctrl->ctrl, en);
	}
}

int dsi_pre_clkoff_cb(void *priv,
			   enum dsi_clk_type clk,
			   enum dsi_lclk_type l_type,
			   enum dsi_clk_state new_state)
{
	int rc = 0, i;
	struct dsi_display *display = priv;
	struct dsi_display_ctrl *ctrl;

	if ((clk & DSI_LINK_CLK) && (new_state == DSI_CLK_OFF) &&
		(l_type && DSI_LINK_LP_CLK)) {
		/*
		 * If ULPS feature is enabled, enter ULPS first.
		 * However, when blanking the panel, we should enter ULPS
		 * only if ULPS during suspend feature is enabled.
		 */
		if (!dsi_panel_initialized(display->panel)) {
			if (display->panel->ulps_suspend_enabled)
				rc = dsi_display_set_ulps(display, true);
		} else if (dsi_panel_ulps_feature_enabled(display->panel)) {
			rc = dsi_display_set_ulps(display, true);
		}
		if (rc)
			pr_err("%s: failed enable ulps, rc = %d\n",
			       __func__, rc);
	}

	if ((clk & DSI_CORE_CLK) && (new_state == DSI_CLK_OFF)) {
		/*
		 * Enable DSI clamps only if entering idle power collapse or
		 * when ULPS during suspend is enabled..
		 */
		if (dsi_panel_initialized(display->panel) ||
			display->panel->ulps_suspend_enabled) {
			dsi_display_phy_idle_off(display);
			rc = dsi_display_set_clamp(display, true);
			if (rc)
				pr_err("%s: Failed to enable dsi clamps. rc=%d\n",
					__func__, rc);

			rc = dsi_display_phy_reset_config(display, false);
			if (rc)
				pr_err("%s: Failed to reset phy, rc=%d\n",
						__func__, rc);
		} else {
			/* Make sure that controller is not in ULPS state when
			 * the DSI link is not active.
			 */
			rc = dsi_display_set_ulps(display, false);
			if (rc)
				pr_err("%s: failed to disable ulps. rc=%d\n",
					__func__, rc);
		}
		/* dsi will not be able to serve irqs from here on */
		dsi_display_ctrl_irq_update(display, false);

		/* cache the MISR values */
		for (i = 0; i < display->ctrl_count; i++) {
			ctrl = &display->ctrl[i];
			if (!ctrl->ctrl)
				continue;
			dsi_ctrl_cache_misr(ctrl->ctrl);
		}

	}

	return rc;
}

int dsi_post_clkon_cb(void *priv,
			   enum dsi_clk_type clk,
			   enum dsi_lclk_type l_type,
			   enum dsi_clk_state curr_state)
{
	int rc = 0;
	struct dsi_display *display = priv;
	bool mmss_clamp = false;

	if ((clk & DSI_LINK_CLK) && (l_type & DSI_LINK_LP_CLK)) {
		mmss_clamp = display->clamp_enabled;
		/*
		 * controller setup is needed if coming out of idle
		 * power collapse with clamps enabled.
		 */
		if (mmss_clamp)
			dsi_display_ctrl_setup(display);

		/*
		 * Phy setup is needed if coming out of idle
		 * power collapse with clamps enabled.
		 */
		if (display->phy_idle_power_off || mmss_clamp)
			dsi_display_phy_idle_on(display, mmss_clamp);

		if (display->ulps_enabled && mmss_clamp) {
			/*
			 * ULPS Entry Request. This is needed if the lanes were
			 * in ULPS prior to power collapse, since after
			 * power collapse and reset, the DSI controller resets
			 * back to idle state and not ULPS. This ulps entry
			 * request will transition the state of the DSI
			 * controller to ULPS which will match the state of the
			 * DSI phy. This needs to be done prior to disabling
			 * the DSI clamps.
			 *
			 * Also, reset the ulps flag so that ulps_config
			 * function would reconfigure the controller state to
			 * ULPS.
			 */
			display->ulps_enabled = false;
			rc = dsi_display_set_ulps(display, true);
			if (rc) {
				pr_err("%s: Failed to enter ULPS. rc=%d\n",
					__func__, rc);
				goto error;
			}
		}

		rc = dsi_display_phy_reset_config(display, true);
		if (rc) {
			pr_err("%s: Failed to reset phy, rc=%d\n",
						__func__, rc);
			goto error;
		}

		rc = dsi_display_set_clamp(display, false);
		if (rc) {
			pr_err("%s: Failed to disable dsi clamps. rc=%d\n",
				__func__, rc);
			goto error;
		}
	}

	if ((clk & DSI_LINK_CLK) && (l_type & DSI_LINK_HS_CLK)) {
		/*
		 * Toggle the resync FIFO everytime clock changes, except
		 * when cont-splash screen transition is going on.
		 * Toggling resync FIFO during cont splash transition
		 * can lead to blinks on the display.
		 */
		if (!display->is_cont_splash_enabled)
			dsi_display_toggle_resync_fifo(display);

		if (display->ulps_enabled) {
			rc = dsi_display_set_ulps(display, false);
			if (rc) {
				pr_err("%s: failed to disable ulps, rc= %d\n",
				       __func__, rc);
				goto error;
			}
		}
	}

	/* enable dsi to serve irqs */
	if (clk & DSI_CORE_CLK)
		dsi_display_ctrl_irq_update(display, true);

error:
	return rc;
}

int dsi_post_clkoff_cb(void *priv,
			    enum dsi_clk_type clk_type,
			    enum dsi_lclk_type l_type,
			    enum dsi_clk_state curr_state)
{
	int rc = 0;
	struct dsi_display *display = priv;

	if (!display) {
		pr_err("%s: Invalid arg\n", __func__);
		return -EINVAL;
	}

	if ((clk_type & DSI_CORE_CLK) &&
	    (curr_state == DSI_CLK_OFF)) {
		rc = dsi_display_phy_power_off(display);
		if (rc)
			pr_err("[%s] failed to power off PHY, rc=%d\n",
				   display->name, rc);

		rc = dsi_display_ctrl_power_off(display);
		if (rc)
			pr_err("[%s] failed to power DSI vregs, rc=%d\n",
				   display->name, rc);
	}
	return rc;
}

int dsi_pre_clkon_cb(void *priv,
			  enum dsi_clk_type clk_type,
			  enum dsi_lclk_type l_type,
			  enum dsi_clk_state new_state)
{
	int rc = 0;
	struct dsi_display *display = priv;

	if (!display) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if ((clk_type & DSI_CORE_CLK) && (new_state == DSI_CLK_ON)) {
		/*
		 * Enable DSI core power
		 * 1.> PANEL_PM are controlled as part of
		 *     panel_power_ctrl. Needed not be handled here.
		 * 2.> CORE_PM are controlled by dsi clk manager.
		 * 3.> CTRL_PM need to be enabled/disabled
		 *     only during unblank/blank. Their state should
		 *     not be changed during static screen.
		 */

	  pr_debug("updating power states for ctrl and phy\n");
		rc = dsi_display_ctrl_power_on(display);
		if (rc) {
			pr_err("[%s] failed to power on dsi controllers, rc=%d\n",
				   display->name, rc);
			return rc;
		}

		rc = dsi_display_phy_power_on(display);
		if (rc) {
			pr_err("[%s] failed to power on dsi phy, rc = %d\n",
				   display->name, rc);
			return rc;
		}

		pr_debug("%s: Enable DSI core power\n", __func__);
	}

	return rc;
}

static void __set_lane_map_v2(u8 *lane_map_v2,
	enum dsi_phy_data_lanes lane0,
	enum dsi_phy_data_lanes lane1,
	enum dsi_phy_data_lanes lane2,
	enum dsi_phy_data_lanes lane3)
{
	lane_map_v2[DSI_LOGICAL_LANE_0] = lane0;
	lane_map_v2[DSI_LOGICAL_LANE_1] = lane1;
	lane_map_v2[DSI_LOGICAL_LANE_2] = lane2;
	lane_map_v2[DSI_LOGICAL_LANE_3] = lane3;
}

static int dsi_display_parse_lane_map(struct dsi_display *display)
{
	int rc = 0, i = 0;
	const char *data;
	u8 temp[DSI_LANE_MAX - 1];

	if (!display) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	/* lane-map-v2 supersedes lane-map-v1 setting */
	rc = of_property_read_u8_array(display->pdev->dev.of_node,
		"qcom,lane-map-v2", temp, (DSI_LANE_MAX - 1));
	if (!rc) {
		for (i = DSI_LOGICAL_LANE_0; i < (DSI_LANE_MAX - 1); i++)
			display->lane_map.lane_map_v2[i] = BIT(temp[i]);
		return 0;
	} else if (rc != EINVAL) {
		pr_debug("Incorrect mapping, configure default\n");
		goto set_default;
	}

	/* lane-map older version, for DSI controller version < 2.0 */
	data = of_get_property(display->pdev->dev.of_node,
		"qcom,lane-map", NULL);
	if (!data)
		goto set_default;

	if (!strcmp(data, "lane_map_3012")) {
		display->lane_map.lane_map_v1 = DSI_LANE_MAP_3012;
		__set_lane_map_v2(display->lane_map.lane_map_v2,
			DSI_PHYSICAL_LANE_1,
			DSI_PHYSICAL_LANE_2,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_0);
	} else if (!strcmp(data, "lane_map_2301")) {
		display->lane_map.lane_map_v1 = DSI_LANE_MAP_2301;
		__set_lane_map_v2(display->lane_map.lane_map_v2,
			DSI_PHYSICAL_LANE_2,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_0,
			DSI_PHYSICAL_LANE_1);
	} else if (!strcmp(data, "lane_map_1230")) {
		display->lane_map.lane_map_v1 = DSI_LANE_MAP_1230;
		__set_lane_map_v2(display->lane_map.lane_map_v2,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_0,
			DSI_PHYSICAL_LANE_1,
			DSI_PHYSICAL_LANE_2);
	} else if (!strcmp(data, "lane_map_0321")) {
		display->lane_map.lane_map_v1 = DSI_LANE_MAP_0321;
		__set_lane_map_v2(display->lane_map.lane_map_v2,
			DSI_PHYSICAL_LANE_0,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_2,
			DSI_PHYSICAL_LANE_1);
	} else if (!strcmp(data, "lane_map_1032")) {
		display->lane_map.lane_map_v1 = DSI_LANE_MAP_1032;
		__set_lane_map_v2(display->lane_map.lane_map_v2,
			DSI_PHYSICAL_LANE_1,
			DSI_PHYSICAL_LANE_0,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_2);
	} else if (!strcmp(data, "lane_map_2103")) {
		display->lane_map.lane_map_v1 = DSI_LANE_MAP_2103;
		__set_lane_map_v2(display->lane_map.lane_map_v2,
			DSI_PHYSICAL_LANE_2,
			DSI_PHYSICAL_LANE_1,
			DSI_PHYSICAL_LANE_0,
			DSI_PHYSICAL_LANE_3);
	} else if (!strcmp(data, "lane_map_3210")) {
		display->lane_map.lane_map_v1 = DSI_LANE_MAP_3210;
		__set_lane_map_v2(display->lane_map.lane_map_v2,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_2,
			DSI_PHYSICAL_LANE_1,
			DSI_PHYSICAL_LANE_0);
	} else {
		pr_warn("%s: invalid lane map %s specified. defaulting to lane_map0123\n",
			__func__, data);
		goto set_default;
	}
	return 0;

set_default:
	/* default lane mapping */
	__set_lane_map_v2(display->lane_map.lane_map_v2, DSI_PHYSICAL_LANE_0,
		DSI_PHYSICAL_LANE_1, DSI_PHYSICAL_LANE_2, DSI_PHYSICAL_LANE_3);
	display->lane_map.lane_map_v1 = DSI_LANE_MAP_0123;
	return 0;
}

static int dsi_display_get_phandle_index(
			struct dsi_display *display,
			const char *propname, int count, int index)
{
	struct device_node *disp_node = display->disp_node;
	u32 *val = NULL;
	int rc = 0;

	val = kzalloc(count, GFP_KERNEL);
	if (!val) {
		rc = -ENOMEM;
		goto end;
	}

	if (index >= count)
		goto end;

	if (display->fw)
		rc = dsi_parser_read_u32_array(display->parser_node,
			propname, val, count);
	else
		rc = of_property_read_u32_array(disp_node, propname,
			val, count);
	if (rc)
		goto end;

	rc = val[index];

	pr_debug("%s index=%d\n", propname, rc);
end:
	kfree(val);
	return rc;
}

static int dsi_display_get_phandle_count(struct dsi_display *display,
			const char *propname)
{
	if (display->fw)
		return dsi_parser_count_u32_elems(display->parser_node,
				propname);
	else
		return of_property_count_u32_elems(display->disp_node,
				propname);
}

static int dsi_display_parse_dt(struct dsi_display *display)
{
	int i, rc = 0;
	u32 phy_count = 0;
	struct device_node *of_node = display->pdev->dev.of_node;
	struct device_node *disp_node = display->disp_node;

	display->ctrl_count = dsi_display_get_phandle_count(display,
				"qcom,dsi-ctrl-num");
	phy_count = dsi_display_get_phandle_count(display,
				"qcom,dsi-ctrl-num");

	pr_debug("ctrl count=%d, phy count=%d\n",
			display->ctrl_count, phy_count);

	if (!phy_count || !display->ctrl_count) {
		pr_err("no ctrl/phys found\n");
		rc = -ENODEV;
		goto error;
	}

	if (phy_count != display->ctrl_count) {
		pr_err("different ctrl and phy counts\n");
		rc = -ENODEV;
		goto error;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		struct dsi_display_ctrl *ctrl = &display->ctrl[i];
		int index;

		index = dsi_display_get_phandle_index(display,
				"qcom,dsi-ctrl-num", display->ctrl_count, i);
		ctrl->ctrl_of_node = of_parse_phandle(of_node,
				"qcom,dsi-ctrl", index);
		of_node_put(ctrl->ctrl_of_node);

		index = dsi_display_get_phandle_index(display,
				"qcom,dsi-phy-num", display->ctrl_count, i);
		ctrl->phy_of_node = of_parse_phandle(of_node,
				"qcom,dsi-phy", index);
		of_node_put(ctrl->phy_of_node);
	}

	display->panel_of = of_parse_phandle(disp_node, "qcom,dsi-panel", 0);
	if (!display->panel_of) {
		pr_err("No Panel device present\n");
		rc = -ENODEV;
		goto error;
	}

	/* Parse TE gpio */
	dsi_display_parse_te_gpio(display);

	/* Parse external bridge from port 0, reg 0 */
	display->ext_bridge_of = of_graph_get_remote_node(of_node, 0, 0);

	pr_debug("success\n");
error:
	return rc;
}

static int dsi_display_res_init(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		ctrl->ctrl = dsi_ctrl_get(ctrl->ctrl_of_node);
		if (IS_ERR_OR_NULL(ctrl->ctrl)) {
			rc = PTR_ERR(ctrl->ctrl);
			pr_err("failed to get dsi controller, rc=%d\n", rc);
			ctrl->ctrl = NULL;
			goto error_ctrl_put;
		}

		ctrl->phy = dsi_phy_get(ctrl->phy_of_node);
		if (IS_ERR_OR_NULL(ctrl->phy)) {
			rc = PTR_ERR(ctrl->phy);
			pr_err("failed to get phy controller, rc=%d\n", rc);
			dsi_ctrl_put(ctrl->ctrl);
			ctrl->phy = NULL;
			goto error_ctrl_put;
		}
	}

	display->panel = dsi_panel_get(&display->pdev->dev,
				display->panel_of,
				display->parser_node,
				display->root,
				display->cmdline_topology);
	if (IS_ERR_OR_NULL(display->panel)) {
		rc = PTR_ERR(display->panel);
		pr_err("failed to get panel, rc=%d\n", rc);
		display->panel = NULL;
		goto error_ctrl_put;
	}

	rc = dsi_display_parse_lane_map(display);
	if (rc) {
		pr_err("Lane map not found, rc=%d\n", rc);
		goto error_ctrl_put;
	}

	rc = dsi_display_clocks_init(display);
	if (rc) {
		pr_err("Failed to parse clock data, rc=%d\n", rc);
		goto error_ctrl_put;
	}

	return 0;
error_ctrl_put:
	for (i = i - 1; i >= 0; i--) {
		ctrl = &display->ctrl[i];
		dsi_ctrl_put(ctrl->ctrl);
		dsi_phy_put(ctrl->phy);
	}
	return rc;
}

static int dsi_display_res_deinit(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	rc = dsi_display_clocks_deinit(display);
	if (rc)
		pr_err("clocks deinit failed, rc=%d\n", rc);

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		dsi_phy_put(ctrl->phy);
		dsi_ctrl_put(ctrl->ctrl);
	}

	if (display->panel)
		dsi_panel_put(display->panel);

	return rc;
}

static int dsi_display_validate_mode_set(struct dsi_display *display,
					 struct dsi_display_mode *mode,
					 u32 flags)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	/*
	 * To set a mode:
	 * 1. Controllers should be turned off.
	 * 2. Link clocks should be off.
	 * 3. Phy should be disabled.
	 */

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if ((ctrl->power_state > DSI_CTRL_POWER_VREG_ON) ||
		    (ctrl->phy_enabled)) {
			rc = -EINVAL;
			goto error;
		}
	}

error:
	return rc;
}

static bool dsi_display_is_seamless_dfps_possible(
		const struct dsi_display *display,
		const struct dsi_display_mode *tgt,
		const enum dsi_dfps_type dfps_type)
{
	struct dsi_display_mode *cur;

	if (!display || !tgt || !display->panel) {
		pr_err("Invalid params\n");
		return false;
	}

	cur = display->panel->cur_mode;

	if (cur->timing.h_active != tgt->timing.h_active) {
		pr_debug("timing.h_active differs %d %d\n",
				cur->timing.h_active, tgt->timing.h_active);
		return false;
	}

	if (cur->timing.h_back_porch != tgt->timing.h_back_porch) {
		pr_debug("timing.h_back_porch differs %d %d\n",
				cur->timing.h_back_porch,
				tgt->timing.h_back_porch);
		return false;
	}

	if (cur->timing.h_sync_width != tgt->timing.h_sync_width) {
		pr_debug("timing.h_sync_width differs %d %d\n",
				cur->timing.h_sync_width,
				tgt->timing.h_sync_width);
		return false;
	}

	if (cur->timing.h_front_porch != tgt->timing.h_front_porch) {
		pr_debug("timing.h_front_porch differs %d %d\n",
				cur->timing.h_front_porch,
				tgt->timing.h_front_porch);
		if (dfps_type != DSI_DFPS_IMMEDIATE_HFP)
			return false;
	}

	if (cur->timing.h_skew != tgt->timing.h_skew) {
		pr_debug("timing.h_skew differs %d %d\n",
				cur->timing.h_skew,
				tgt->timing.h_skew);
		return false;
	}

	/* skip polarity comparison */

	if (cur->timing.v_active != tgt->timing.v_active) {
		pr_debug("timing.v_active differs %d %d\n",
				cur->timing.v_active,
				tgt->timing.v_active);
		return false;
	}

	if (cur->timing.v_back_porch != tgt->timing.v_back_porch) {
		pr_debug("timing.v_back_porch differs %d %d\n",
				cur->timing.v_back_porch,
				tgt->timing.v_back_porch);
		return false;
	}

	if (cur->timing.v_sync_width != tgt->timing.v_sync_width) {
		pr_debug("timing.v_sync_width differs %d %d\n",
				cur->timing.v_sync_width,
				tgt->timing.v_sync_width);
		return false;
	}

	if (cur->timing.v_front_porch != tgt->timing.v_front_porch) {
		pr_debug("timing.v_front_porch differs %d %d\n",
				cur->timing.v_front_porch,
				tgt->timing.v_front_porch);
		if (dfps_type != DSI_DFPS_IMMEDIATE_VFP)
			return false;
	}

	/* skip polarity comparison */

	if (cur->timing.refresh_rate == tgt->timing.refresh_rate)
		pr_debug("timing.refresh_rate identical %d %d\n",
				cur->timing.refresh_rate,
				tgt->timing.refresh_rate);

	if (cur->pixel_clk_khz != tgt->pixel_clk_khz)
		pr_debug("pixel_clk_khz differs %d %d\n",
				cur->pixel_clk_khz, tgt->pixel_clk_khz);

	if (cur->dsi_mode_flags != tgt->dsi_mode_flags)
		pr_debug("flags differs %d %d\n",
				cur->dsi_mode_flags, tgt->dsi_mode_flags);

	return true;
}

static int dsi_display_dfps_update(struct dsi_display *display,
				   struct dsi_display_mode *dsi_mode)
{
	struct dsi_mode_info *timing;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	struct dsi_display_mode *panel_mode;
	struct dsi_dfps_capabilities dfps_caps;
	int rc = 0;
	int i = 0;

	if (!display || !dsi_mode || !display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}
	timing = &dsi_mode->timing;

	dsi_panel_get_dfps_caps(display->panel, &dfps_caps);
	if (!dfps_caps.dfps_support) {
		pr_err("dfps not supported\n");
		return -ENOTSUPP;
	}

	if (dfps_caps.type == DSI_DFPS_IMMEDIATE_CLK) {
		pr_err("dfps clock method not supported\n");
		return -ENOTSUPP;
	}

	/* For split DSI, update the clock master first */

	pr_debug("configuring seamless dynamic fps\n\n");

	m_ctrl = &display->ctrl[display->clk_master_idx];
	rc = dsi_ctrl_async_timing_update(m_ctrl->ctrl, timing);
	if (rc) {
		pr_err("[%s] failed to dfps update host_%d, rc=%d\n",
				display->name, i, rc);
		goto error;
	}

	/* Update the rest of the controllers */
	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_async_timing_update(ctrl->ctrl, timing);
		if (rc) {
			pr_err("[%s] failed to dfps update host_%d, rc=%d\n",
					display->name, i, rc);
			goto error;
		}
	}

	panel_mode = display->panel->cur_mode;
	memcpy(panel_mode, dsi_mode, sizeof(*panel_mode));
	/*
	 * dsi_mode_flags flags are used to communicate with other drm driver
	 * components, and are transient. They aren't inherently part of the
	 * display panel's mode and shouldn't be saved into the cached currently
	 * active mode.
	 */
	panel_mode->dsi_mode_flags = 0;

error:
	return rc;
}

static int dsi_display_dfps_calc_front_porch(
		u32 old_fps,
		u32 new_fps,
		u32 a_total,
		u32 b_total,
		u32 b_fp,
		u32 *b_fp_out)
{
	s32 b_fp_new;
	int add_porches, diff;

	if (!b_fp_out) {
		pr_err("Invalid params");
		return -EINVAL;
	}

	if (!a_total || !new_fps) {
		pr_err("Invalid pixel total or new fps in mode request\n");
		return -EINVAL;
	}

	/*
	 * Keep clock, other porches constant, use new fps, calc front porch
	 * new_vtotal = old_vtotal * (old_fps / new_fps )
	 * new_vfp - old_vfp = new_vtotal - old_vtotal
	 * new_vfp = old_vfp + old_vtotal * ((old_fps - new_fps)/ new_fps)
	 */
	diff = abs(old_fps - new_fps);
	add_porches = mult_frac(b_total, diff, new_fps);

	if (old_fps > new_fps)
		b_fp_new = b_fp + add_porches;
	else
		b_fp_new = b_fp - add_porches;

	pr_debug("fps %u a %u b %u b_fp %u new_fp %d\n",
			new_fps, a_total, b_total, b_fp, b_fp_new);

	if (b_fp_new < 0) {
		pr_err("Invalid new_hfp calcluated%d\n", b_fp_new);
		return -EINVAL;
	}

	/**
	 * TODO: To differentiate from clock method when communicating to the
	 * other components, perhaps we should set clk here to original value
	 */
	*b_fp_out = b_fp_new;

	return 0;
}

/**
 * dsi_display_get_dfps_timing() - Get the new dfps values.
 * @display:         DSI display handle.
 * @adj_mode:        Mode value structure to be changed.
 *                   It contains old timing values and latest fps value.
 *                   New timing values are updated based on new fps.
 * @curr_refresh_rate:  Current fps rate.
 *                      If zero , current fps rate is taken from
 *                      display->panel->cur_mode.
 * Return: error code.
 */
static int dsi_display_get_dfps_timing(struct dsi_display *display,
			struct dsi_display_mode *adj_mode,
				u32 curr_refresh_rate)
{
	struct dsi_dfps_capabilities dfps_caps;
	struct dsi_display_mode per_ctrl_mode;
	struct dsi_mode_info *timing;
	struct dsi_ctrl *m_ctrl;

	int rc = 0;

	if (!display || !adj_mode) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}
	m_ctrl = display->ctrl[display->clk_master_idx].ctrl;

	dsi_panel_get_dfps_caps(display->panel, &dfps_caps);
	if (!dfps_caps.dfps_support) {
		pr_err("dfps not supported by panel\n");
		return -EINVAL;
	}

	per_ctrl_mode = *adj_mode;
	adjust_timing_by_ctrl_count(display, &per_ctrl_mode);

	if (!curr_refresh_rate) {
		if (!dsi_display_is_seamless_dfps_possible(display,
				&per_ctrl_mode, dfps_caps.type)) {
			pr_err("seamless dynamic fps not supported for mode\n");
			return -EINVAL;
		}
		if (display->panel->cur_mode) {
			curr_refresh_rate =
				display->panel->cur_mode->timing.refresh_rate;
		} else {
			pr_err("cur_mode is not initialized\n");
			return -EINVAL;
		}
	}
	/* TODO: Remove this direct reference to the dsi_ctrl */
	timing = &per_ctrl_mode.timing;

	switch (dfps_caps.type) {
	case DSI_DFPS_IMMEDIATE_VFP:
		rc = dsi_display_dfps_calc_front_porch(
				curr_refresh_rate,
				timing->refresh_rate,
				DSI_H_TOTAL(timing),
				DSI_V_TOTAL(timing),
				timing->v_front_porch,
				&adj_mode->timing.v_front_porch);
		break;

	case DSI_DFPS_IMMEDIATE_HFP:
		rc = dsi_display_dfps_calc_front_porch(
				curr_refresh_rate,
				timing->refresh_rate,
				DSI_V_TOTAL(timing),
				DSI_H_TOTAL(timing),
				timing->h_front_porch,
				&adj_mode->timing.h_front_porch);
		if (!rc)
			adj_mode->timing.h_front_porch *= display->ctrl_count;
		break;

	default:
		pr_err("Unsupported DFPS mode %d\n", dfps_caps.type);
		rc = -ENOTSUPP;
	}

	return rc;
}

static bool dsi_display_validate_mode_seamless(struct dsi_display *display,
		struct dsi_display_mode *adj_mode)
{
	int rc = 0;

	if (!display || !adj_mode) {
		pr_err("Invalid params\n");
		return false;
	}

	/* Currently the only seamless transition is dynamic fps */
	rc = dsi_display_get_dfps_timing(display, adj_mode, 0);
	if (rc) {
		pr_debug("Dynamic FPS not supported for seamless\n");
	} else {
		pr_debug("Mode switch is seamless Dynamic FPS\n");
		adj_mode->dsi_mode_flags |= DSI_MODE_FLAG_DFPS |
				DSI_MODE_FLAG_VBLANK_PRE_MODESET;
	}

	return rc;
}

static int dsi_display_set_mode_sub(struct dsi_display *display,
				    struct dsi_display_mode *mode,
				    u32 flags)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;
	struct dsi_display_mode_priv_info *priv_info;

	priv_info = mode->priv_info;
	if (!priv_info) {
		pr_err("[%s] failed to get private info of the display mode",
			display->name);
		return -EINVAL;
	}

	rc = dsi_panel_get_host_cfg_for_mode(display->panel,
					     mode,
					     &display->config);
	if (rc) {
		pr_err("[%s] failed to get host config for mode, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	memcpy(&display->config.lane_map, &display->lane_map,
	       sizeof(display->lane_map));

	if (mode->dsi_mode_flags &
			(DSI_MODE_FLAG_DFPS | DSI_MODE_FLAG_VRR)) {
		rc = dsi_display_dfps_update(display, mode);
		if (rc) {
			pr_err("[%s]DSI dfps update failed, rc=%d\n",
					display->name, rc);
			goto error;
		}
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_update_host_config(ctrl->ctrl, &display->config,
				mode->dsi_mode_flags, display->dsi_clk_handle);
		if (rc) {
			pr_err("[%s] failed to update ctrl config, rc=%d\n",
			       display->name, rc);
			goto error;
		}
	}

	if (priv_info->phy_timing_len) {
		for (i = 0; i < display->ctrl_count; i++) {
			ctrl = &display->ctrl[i];
			 rc = dsi_phy_set_timing_params(ctrl->phy,
				priv_info->phy_timing_val,
				priv_info->phy_timing_len);
			if (rc)
				pr_err("failed to add DSI PHY timing params");
		}
	}
error:
	return rc;
}

/**
 * _dsi_display_dev_init - initializes the display device
 * Initialization will acquire references to the resources required for the
 * display hardware to function.
 * @display:         Handle to the display
 * Returns:          Zero on success
 */
static int _dsi_display_dev_init(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		pr_err("invalid display\n");
		return -EINVAL;
	}

	if (!display->disp_node)
		return 0;

	mutex_lock(&display->display_lock);

	display->display_type = of_get_property(display->disp_node,
					"qcom,display-type", NULL);
	if (!display->display_type)
		display->display_type = "unknown";

	display->parser = dsi_parser_get(&display->pdev->dev);
	if (display->fw && display->parser)
		display->parser_node = dsi_parser_get_head_node(
				display->parser, display->fw->data,
				display->fw->size);

	rc = dsi_display_parse_dt(display);
	if (rc) {
		pr_err("[%s] failed to parse dt, rc=%d\n", display->name, rc);
		goto error;
	}

	rc = dsi_display_res_init(display);
	if (rc) {
		pr_err("[%s] failed to initialize resources, rc=%d\n",
		       display->name, rc);
		goto error;
	}
error:
	mutex_unlock(&display->display_lock);
	return rc;
}

/**
 * _dsi_display_dev_deinit - deinitializes the display device
 * All the resources acquired during device init will be released.
 * @display:        Handle to the display
 * Returns:         Zero on success
 */
static int _dsi_display_dev_deinit(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		pr_err("invalid display\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_display_res_deinit(display);
	if (rc)
		pr_err("[%s] failed to deinitialize resource, rc=%d\n",
		       display->name, rc);

	mutex_unlock(&display->display_lock);

	return rc;
}

/**
 * dsi_display_cont_splash_config() - Initialize resources for continuous splash
 * @dsi_display:    Pointer to dsi display
 * Returns:     Zero on success
 */
int dsi_display_cont_splash_config(void *dsi_display)
{
	struct dsi_display *display = dsi_display;
	int rc = 0;

	/* Vote for gdsc required to read register address space */
	if (!display) {
		pr_err("invalid input display param\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	/* Vote for gdsc required to read register address space */
	display->cont_splash_client = sde_power_client_create(display->phandle,
						"cont_splash_client");
	rc = sde_power_resource_enable(display->phandle,
			display->cont_splash_client, true);
	if (rc) {
		pr_err("failed to vote gdsc for continuous splash, rc=%d\n",
							rc);
		mutex_unlock(&display->display_lock);
		return -EINVAL;
	}

	/* Verify whether continuous splash is enabled or not */
	display->is_cont_splash_enabled =
		dsi_display_get_cont_splash_status(display);
	if (!display->is_cont_splash_enabled) {
		pr_err("Continuous splash is not enabled\n");
		goto splash_disabled;
	}

	/* Update splash status for clock manager */
	dsi_display_clk_mngr_update_splash_status(display->clk_mngr,
				display->is_cont_splash_enabled);

	/* Set up ctrl isr before enabling core clk */
	dsi_display_ctrl_isr_configure(display, true);

	/* Vote for Core clk and link clk. Votes on ctrl and phy
	 * regulator are inplicit from  pre clk on callback
	 */
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI link clocks, rc=%d\n",
		       display->name, rc);
		goto clk_manager_update;
	}

	/* Vote on panel regulator will be removed during suspend path */
	rc = dsi_pwr_enable_regulator(&display->panel->power_info, true);
	if (rc) {
		pr_err("[%s] failed to enable vregs, rc=%d\n",
				display->panel->name, rc);
		goto clks_disabled;
	}

	dsi_config_host_engine_state_for_cont_splash(display);
	mutex_unlock(&display->display_lock);

	return rc;

clks_disabled:
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);

clk_manager_update:
	dsi_display_ctrl_isr_configure(display, false);
	/* Update splash status for clock manager */
	dsi_display_clk_mngr_update_splash_status(display->clk_mngr,
				false);

splash_disabled:
	(void)sde_power_resource_enable(display->phandle,
			display->cont_splash_client, false);
	display->is_cont_splash_enabled = false;
	mutex_unlock(&display->display_lock);
	return rc;
}

/**
 * dsi_display_splash_res_cleanup() - cleanup for continuous splash
 * @display:    Pointer to dsi display
 * Returns:     Zero on success
 */
int dsi_display_splash_res_cleanup(struct  dsi_display *display)
{
	int rc = 0;

	if (!display->is_cont_splash_enabled)
		return 0;

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);
	if (rc)
		pr_err("[%s] failed to disable DSI link clocks, rc=%d\n",
		       display->name, rc);

	rc = sde_power_resource_enable(display->phandle,
			display->cont_splash_client, false);
	if (rc)
		pr_err("failed to remove vote on gdsc for continuous splash, rc=%d\n",
				rc);

	display->is_cont_splash_enabled = false;
	/* Update splash status for clock manager */
	dsi_display_clk_mngr_update_splash_status(display->clk_mngr,
				display->is_cont_splash_enabled);

	return rc;
}

static int dsi_display_force_update_dsi_clk(struct dsi_display *display)
{
	int rc = 0;

	rc = dsi_display_link_clk_force_update_ctrl(display->dsi_clk_handle);

	if (!rc) {
		pr_info("dsi bit clk has been configured to %d\n",
			display->cached_clk_rate);

		atomic_set(&display->clkrate_change_pending, 0);
	} else {
		pr_err("Failed to configure dsi bit clock '%d'. rc = %d\n",
			display->cached_clk_rate, rc);
	}

	return rc;
}

static int dsi_display_request_update_dsi_bitrate(struct dsi_display *display,
					u32 bit_clk_rate)
{
	int rc = 0;
	int i;

	pr_debug("%s:bit rate:%d\n", __func__, bit_clk_rate);
	if (!display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (bit_clk_rate == 0) {
		pr_err("Invalid bit clock rate\n");
		return -EINVAL;
	}

	display->config.bit_clk_rate_hz = bit_clk_rate;

	for (i = 0; i < display->ctrl_count; i++) {
		struct dsi_display_ctrl *dsi_disp_ctrl = &display->ctrl[i];
		struct dsi_ctrl *ctrl = dsi_disp_ctrl->ctrl;
		u32 num_of_lanes = 0;
		u32 bpp = 3;
		u64 bit_rate, pclk_rate, bit_rate_per_lane, byte_clk_rate;
		struct dsi_host_common_cfg *host_cfg;

		mutex_lock(&ctrl->ctrl_lock);

		host_cfg = &display->panel->host_config;
		if (host_cfg->data_lanes & DSI_DATA_LANE_0)
			num_of_lanes++;
		if (host_cfg->data_lanes & DSI_DATA_LANE_1)
			num_of_lanes++;
		if (host_cfg->data_lanes & DSI_DATA_LANE_2)
			num_of_lanes++;
		if (host_cfg->data_lanes & DSI_DATA_LANE_3)
			num_of_lanes++;

		if (num_of_lanes == 0) {
			pr_err("Invalid lane count\n");
			rc = -EINVAL;
			goto error;
		}

		bit_rate = display->config.bit_clk_rate_hz * num_of_lanes;
		bit_rate_per_lane = bit_rate;
		do_div(bit_rate_per_lane, num_of_lanes);
		pclk_rate = bit_rate;
		do_div(pclk_rate, (8 * bpp));
		byte_clk_rate = bit_rate_per_lane;
		do_div(byte_clk_rate, 8);
		pr_debug("bit_clk_rate = %llu, bit_clk_rate_per_lane = %llu\n",
			 bit_rate, bit_rate_per_lane);
		pr_debug("byte_clk_rate = %llu, pclk_rate = %llu\n",
			  byte_clk_rate, pclk_rate);

		ctrl->clk_freq.byte_clk_rate = byte_clk_rate;
		ctrl->clk_freq.pix_clk_rate = pclk_rate;
		rc = dsi_clk_set_link_frequencies(display->dsi_clk_handle,
			ctrl->clk_freq, ctrl->cell_index);
		if (rc) {
			pr_err("Failed to update link frequencies\n");
			goto error;
		}

		ctrl->host_config.bit_clk_rate_hz = bit_clk_rate;
error:
		mutex_unlock(&ctrl->ctrl_lock);

		/* TODO: recover ctrl->clk_freq in case of failure */
		if (rc)
			return rc;
	}

	return 0;
}

static ssize_t sysfs_dynamic_dsi_clk_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct dsi_display *display;
	struct dsi_display_ctrl *m_ctrl;
	struct dsi_ctrl *ctrl;

	display = dev_get_drvdata(dev);
	if (!display) {
		pr_err("Invalid display\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	ctrl = m_ctrl->ctrl;
	if (ctrl)
		display->cached_clk_rate = ctrl->clk_freq.byte_clk_rate
					     * 8;

	rc = snprintf(buf, PAGE_SIZE, "%d\n", display->cached_clk_rate);
	pr_debug("%s: read dsi clk rate %d\n", __func__,
		display->cached_clk_rate);

	mutex_unlock(&display->display_lock);

	return rc;
}

static ssize_t sysfs_dynamic_dsi_clk_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int rc = 0;
	int clk_rate;
	struct dsi_display *display;

	display = dev_get_drvdata(dev);
	if (!display) {
		pr_err("Invalid display\n");
		return -EINVAL;
	}

	rc = kstrtoint(buf, DSI_CLOCK_BITRATE_RADIX, &clk_rate);
	if (rc) {
		pr_err("%s: kstrtoint failed. rc=%d\n", __func__, rc);
		return rc;
	}

	if (clk_rate <= 0) {
		pr_err("%s: bitrate should be greater than 0\n", __func__);
		return -EINVAL;
	}

	if (clk_rate == display->cached_clk_rate) {
		pr_info("%s: ignore duplicated DSI clk setting\n", __func__);
		return count;
	}

	pr_info("%s: bitrate param value: '%d'\n", __func__, clk_rate);

	mutex_lock(&display->display_lock);

	display->cached_clk_rate = clk_rate;
	rc = dsi_display_request_update_dsi_bitrate(display, clk_rate);
	if (!rc) {
		pr_info("%s: bit clk is ready to be configured to '%d'\n",
			__func__, clk_rate);
	} else {
		pr_err("%s: Failed to prepare to configure '%d'. rc = %d\n",
			__func__, clk_rate, rc);
		/*Caching clock failed, so don't go on doing so.*/
		atomic_set(&display->clkrate_change_pending, 0);
		display->cached_clk_rate = 0;

		mutex_unlock(&display->display_lock);

		return rc;
	}
	atomic_set(&display->clkrate_change_pending, 1);

	mutex_unlock(&display->display_lock);

	return count;

}

static DEVICE_ATTR(dynamic_dsi_clock, 0644,
			sysfs_dynamic_dsi_clk_read,
			sysfs_dynamic_dsi_clk_write);

static struct attribute *dynamic_dsi_clock_fs_attrs[] = {
	&dev_attr_dynamic_dsi_clock.attr,
	NULL,
};
static struct attribute_group dynamic_dsi_clock_fs_attrs_group = {
	.attrs = dynamic_dsi_clock_fs_attrs,
};

static int dsi_display_sysfs_init(struct dsi_display *display)
{
	int rc = 0;
	struct device *dev = &display->pdev->dev;

	if (display->panel->panel_mode == DSI_OP_CMD_MODE)
		rc = sysfs_create_group(&dev->kobj,
			&dynamic_dsi_clock_fs_attrs_group);

	return rc;

}

static int dsi_display_sysfs_deinit(struct dsi_display *display)
{
	struct device *dev = &display->pdev->dev;

	if (display->panel->panel_mode == DSI_OP_CMD_MODE)
		sysfs_remove_group(&dev->kobj,
			&dynamic_dsi_clock_fs_attrs_group);

	return 0;

}

/**
 * dsi_display_bind - bind dsi device with controlling device
 * @dev:        Pointer to base of platform device
 * @master:     Pointer to container of drm device
 * @data:       Pointer to private data
 * Returns:     Zero on success
 */
static int dsi_display_bind(struct device *dev,
		struct device *master,
		void *data)
{
	struct dsi_display_ctrl *display_ctrl;
	struct drm_device *drm;
	struct dsi_display *display;
	struct dsi_clk_info info;
	struct clk_ctrl_cb clk_cb;
	struct msm_drm_private *priv;
	void *handle = NULL;
	struct platform_device *pdev = to_platform_device(dev);
	char *client1 = "dsi_clk_client";
	char *client2 = "mdp_event_client";
	char dsi_client_name[DSI_CLIENT_NAME_SIZE];
	int i, rc = 0;

	if (!dev || !pdev || !master) {
		pr_err("invalid param(s), dev %pK, pdev %pK, master %pK\n",
				dev, pdev, master);
		return -EINVAL;
	}

	drm = dev_get_drvdata(master);
	display = platform_get_drvdata(pdev);
	if (!drm || !display) {
		pr_err("invalid param(s), drm %pK, display %pK\n",
				drm, display);
		return -EINVAL;
	}
	priv = drm->dev_private;

	if (!display->disp_node)
		return 0;

	mutex_lock(&display->display_lock);

	rc = dsi_display_debugfs_init(display);
	if (rc) {
		pr_err("[%s] debugfs init failed, rc=%d\n", display->name, rc);
		goto error;
	}

	atomic_set(&display->clkrate_change_pending, 0);
	display->cached_clk_rate = 0;

	rc = dsi_display_sysfs_init(display);
	if (rc) {
		pr_err("[%s] sysfs init failed, rc=%d\n", display->name, rc);
		goto error;
	}

	memset(&info, 0x0, sizeof(info));

	for (i = 0; i < display->ctrl_count; i++) {
		display_ctrl = &display->ctrl[i];
		rc = dsi_ctrl_drv_init(display_ctrl->ctrl, display->root);
		if (rc) {
			pr_err("[%s] failed to initialize ctrl[%d], rc=%d\n",
			       display->name, i, rc);
			goto error_ctrl_deinit;
		}
		display_ctrl->ctrl->horiz_index = i;

		rc = dsi_phy_drv_init(display_ctrl->phy);
		if (rc) {
			pr_err("[%s] Failed to initialize phy[%d], rc=%d\n",
				display->name, i, rc);
			(void)dsi_ctrl_drv_deinit(display_ctrl->ctrl);
			goto error_ctrl_deinit;
		}

		memcpy(&info.c_clks[i],
				(&display_ctrl->ctrl->clk_info.core_clks),
				sizeof(struct dsi_core_clk_info));
		memcpy(&info.l_hs_clks[i],
				(&display_ctrl->ctrl->clk_info.hs_link_clks),
				sizeof(struct dsi_link_hs_clk_info));
		memcpy(&info.l_lp_clks[i],
				(&display_ctrl->ctrl->clk_info.lp_link_clks),
				sizeof(struct dsi_link_lp_clk_info));

		info.c_clks[i].phandle = &priv->phandle;
		info.bus_handle[i] =
			display_ctrl->ctrl->axi_bus_info.bus_handle;
		info.ctrl_index[i] = display_ctrl->ctrl->cell_index;
		snprintf(dsi_client_name, DSI_CLIENT_NAME_SIZE,
						"dsi_core_client%u", i);
		info.c_clks[i].dsi_core_client = sde_power_client_create(
				info.c_clks[i].phandle, dsi_client_name);
		if (IS_ERR_OR_NULL(info.c_clks[i].dsi_core_client)) {
			pr_err("[%s] client creation failed for ctrl[%d]",
					dsi_client_name, i);
			goto error_ctrl_deinit;
		}
	}

	display->phandle = &priv->phandle;
	info.pre_clkoff_cb = dsi_pre_clkoff_cb;
	info.pre_clkon_cb = dsi_pre_clkon_cb;
	info.post_clkoff_cb = dsi_post_clkoff_cb;
	info.post_clkon_cb = dsi_post_clkon_cb;
	info.priv_data = display;
	info.master_ndx = display->clk_master_idx;
	info.dsi_ctrl_count = display->ctrl_count;
	snprintf(info.name, MAX_STRING_LEN,
			"DSI_MNGR-%s", display->name);

	display->clk_mngr = dsi_display_clk_mngr_register(&info);
	if (IS_ERR_OR_NULL(display->clk_mngr)) {
		rc = PTR_ERR(display->clk_mngr);
		display->clk_mngr = NULL;
		pr_err("dsi clock registration failed, rc = %d\n", rc);
		goto error_ctrl_deinit;
	}

	handle = dsi_register_clk_handle(display->clk_mngr, client1);
	if (IS_ERR_OR_NULL(handle)) {
		rc = PTR_ERR(handle);
		pr_err("failed to register %s client, rc = %d\n",
		       client1, rc);
		goto error_clk_deinit;
	} else {
		display->dsi_clk_handle = handle;
	}

	handle = dsi_register_clk_handle(display->clk_mngr, client2);
	if (IS_ERR_OR_NULL(handle)) {
		rc = PTR_ERR(handle);
		pr_err("failed to register %s client, rc = %d\n",
		       client2, rc);
		goto error_clk_client_deinit;
	} else {
		display->mdp_clk_handle = handle;
	}

	clk_cb.priv = display;
	clk_cb.dsi_clk_cb = dsi_display_clk_ctrl_cb;

	for (i = 0; i < display->ctrl_count; i++) {
		display_ctrl = &display->ctrl[i];

		rc = dsi_ctrl_clk_cb_register(display_ctrl->ctrl, &clk_cb);
		if (rc) {
			pr_err("[%s] failed to register ctrl clk_cb[%d], rc=%d\n",
			       display->name, i, rc);
			goto error_ctrl_deinit;
		}

		rc = dsi_phy_clk_cb_register(display_ctrl->phy, &clk_cb);
		if (rc) {
			pr_err("[%s] failed to register phy clk_cb[%d], rc=%d\n",
			       display->name, i, rc);
			goto error_ctrl_deinit;
		}
	}

	rc = dsi_display_mipi_host_init(display);
	if (rc) {
		pr_err("[%s] failed to initialize mipi host, rc=%d\n",
		       display->name, rc);
		goto error_ctrl_deinit;
	}

	rc = dsi_panel_drv_init(display->panel, &display->host);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			pr_err("[%s] failed to initialize panel driver, rc=%d\n",
			       display->name, rc);
		goto error_host_deinit;
	}

	pr_info("Successfully bind display panel '%s'\n", display->name);
	display->drm_dev = drm;

	for (i = 0; i < display->ctrl_count; i++) {
		display_ctrl = &display->ctrl[i];

		if (!display_ctrl->phy || !display_ctrl->ctrl)
			continue;

		rc = dsi_phy_set_clk_freq(display_ctrl->phy,
				&display_ctrl->ctrl->clk_freq);
		if (rc) {
			pr_err("[%s] failed to set phy clk freq, rc=%d\n",
					display->name, rc);
			goto error;
		}
	}

	/* register te irq handler */
	dsi_display_register_te_irq(display);

	goto error;

error_host_deinit:
	(void)dsi_display_mipi_host_deinit(display);
error_clk_client_deinit:
	(void)dsi_deregister_clk_handle(display->dsi_clk_handle);
error_clk_deinit:
	(void)dsi_display_clk_mngr_deregister(display->clk_mngr);
error_ctrl_deinit:
	for (i = i - 1; i >= 0; i--) {
		display_ctrl = &display->ctrl[i];
		(void)dsi_phy_drv_deinit(display_ctrl->phy);
		(void)dsi_ctrl_drv_deinit(display_ctrl->ctrl);
	}
	(void)dsi_display_sysfs_deinit(display);
	(void)dsi_display_debugfs_deinit(display);
error:
	mutex_unlock(&display->display_lock);
	return rc;
}

/**
 * dsi_display_unbind - unbind dsi from controlling device
 * @dev:        Pointer to base of platform device
 * @master:     Pointer to container of drm device
 * @data:       Pointer to private data
 */
static void dsi_display_unbind(struct device *dev,
		struct device *master, void *data)
{
	struct dsi_display_ctrl *display_ctrl;
	struct dsi_display *display;
	struct platform_device *pdev = to_platform_device(dev);
	int i, rc = 0;

	if (!dev || !pdev) {
		pr_err("invalid param(s)\n");
		return;
	}

	display = platform_get_drvdata(pdev);
	if (!display) {
		pr_err("invalid display\n");
		return;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_panel_drv_deinit(display->panel);
	if (rc)
		pr_err("[%s] failed to deinit panel driver, rc=%d\n",
		       display->name, rc);

	rc = dsi_display_mipi_host_deinit(display);
	if (rc)
		pr_err("[%s] failed to deinit mipi hosts, rc=%d\n",
		       display->name,
		       rc);

	for (i = 0; i < display->ctrl_count; i++) {
		display_ctrl = &display->ctrl[i];

		rc = dsi_phy_drv_deinit(display_ctrl->phy);
		if (rc)
			pr_err("[%s] failed to deinit phy%d driver, rc=%d\n",
			       display->name, i, rc);

		rc = dsi_ctrl_drv_deinit(display_ctrl->ctrl);
		if (rc)
			pr_err("[%s] failed to deinit ctrl%d driver, rc=%d\n",
			       display->name, i, rc);
	}

	atomic_set(&display->clkrate_change_pending, 0);
	(void)dsi_display_sysfs_deinit(display);
	(void)dsi_display_debugfs_deinit(display);

	mutex_unlock(&display->display_lock);
}

static const struct component_ops dsi_display_comp_ops = {
	.bind = dsi_display_bind,
	.unbind = dsi_display_unbind,
};

static struct platform_driver dsi_display_driver = {
	.probe = dsi_display_dev_probe,
	.remove = dsi_display_dev_remove,
	.driver = {
		.name = "msm-dsi-display",
		.of_match_table = dsi_display_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int dsi_display_init(struct dsi_display *display)
{
	int rc = 0;
	 struct platform_device *pdev = display->pdev;

	mutex_init(&display->display_lock);

	rc = _dsi_display_dev_init(display);
	if (rc) {
		pr_err("device init failed, rc=%d\n", rc);
		goto end;
	}

	rc = component_add(&pdev->dev, &dsi_display_comp_ops);
	if (rc)
		pr_err("component add failed, rc=%d\n", rc);

	pr_debug("component add success: %s\n", display->name);
end:
	return rc;
}

static void dsi_display_firmware_display(const struct firmware *fw,
				void *context)
{
	struct dsi_display *display = context;

	if (fw) {
		pr_debug("reading data from firmware, size=%zd\n",
			fw->size);

		display->fw = fw;
		display->name = "dsi_firmware_display";
	}

	if (dsi_display_init(display))
		return;

	pr_debug("success\n");
}

int dsi_display_dev_probe(struct platform_device *pdev)
{
	struct dsi_display *display = NULL;
	struct device_node *node = NULL, *disp_node = NULL;
	const char *dsi_type = NULL, *name = NULL;
	const char *disp_list = "qcom,dsi-display-list";
	const char *disp_active = "qcom,dsi-display-active";
	int i, count, rc = 0, index;
	bool firm_req = false;
	struct dsi_display_boot_param *boot_disp;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("pdev not found\n");
		rc = -ENODEV;
		goto end;
	}

	dsi_type = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!dsi_type)
		dsi_type = "primary";

	if (!strcmp(dsi_type, "primary"))
		index = DSI_PRIMARY;
	else
		index = DSI_SECONDARY;

	boot_disp = &boot_displays[index];

	display = devm_kzalloc(&pdev->dev, sizeof(*display), GFP_KERNEL);
	if (!display) {
		rc = -ENOMEM;
		goto end;
	}

	node = pdev->dev.of_node;
	count = of_count_phandle_with_args(node, disp_list,  NULL);

	for (i = 0; i < count; i++) {
		struct device_node *np;
		const char *disp_type = NULL;

		np = of_parse_phandle(node, disp_list, i);
		name = of_get_property(np, "label", NULL);
		if (!name) {
			pr_err("display name not defined\n");
			continue;
		}

		disp_type = of_get_property(np, "qcom,display-type", NULL);
		if (!disp_type) {
			pr_err("display type not defined for %s\n", name);
			continue;
		}

		/* primary/secondary display should match with current dsi */
		if (strcmp(dsi_type, disp_type))
			continue;

		if (boot_disp->boot_disp_en) {
			if (!strcmp(boot_disp->name, name)) {
				disp_node = np;
				break;
			}
		} else if (of_property_read_bool(np, disp_active)) {
			disp_node = np;

			if (IS_ENABLED(CONFIG_DSI_PARSER))
				firm_req = !request_firmware_nowait(
					THIS_MODULE, 1, "dsi_prop",
					&pdev->dev, GFP_KERNEL, display,
					dsi_display_firmware_display);
			break;
		}

		of_node_put(np);
	}

	boot_disp->node = pdev->dev.of_node;
	boot_disp->disp = display;

	display->disp_node = disp_node;
	display->name = name;
	display->pdev = pdev;
	display->boot_disp = boot_disp;

	dsi_display_parse_cmdline_topology(display, index);

	platform_set_drvdata(pdev, display);

	/* initialize display in firmware callback */
	if (!firm_req) {
		rc = dsi_display_init(display);
		if (rc)
			goto end;
	}

	return 0;
end:
	if (display)
		devm_kfree(&pdev->dev, display);

	return rc;
}

int dsi_display_dev_remove(struct platform_device *pdev)
{
	int rc = 0;
	struct dsi_display *display;

	if (!pdev) {
		pr_err("Invalid device\n");
		return -EINVAL;
	}

	display = platform_get_drvdata(pdev);

	/* decrement ref count */
	of_node_put(display->disp_node);

	(void)_dsi_display_dev_deinit(display);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, display);
	return rc;
}

int dsi_display_get_num_of_displays(void)
{
	int i, count = 0;

	for (i = 0; i < MAX_DSI_ACTIVE_DISPLAY; i++) {
		struct dsi_display *display = boot_displays[i].disp;

		if (display && display->disp_node)
			count++;
	}

	return count;
}

int dsi_display_get_active_displays(void **display_array, u32 max_display_count)
{
	int index = 0, count = 0;

	if (!display_array || !max_display_count) {
		pr_err("invalid params\n");
		return 0;
	}

	for (index = 0; index < MAX_DSI_ACTIVE_DISPLAY; index++) {
		struct dsi_display *display = boot_displays[index].disp;

		if (display && display->disp_node)
			display_array[count++] = display;
	}

	return count;
}

int dsi_display_drm_bridge_init(struct dsi_display *display,
		struct drm_encoder *enc)
{
	int rc = 0;
	struct dsi_bridge *bridge;
	struct msm_drm_private *priv = NULL;

	if (!display || !display->drm_dev || !enc) {
		pr_err("invalid param(s)\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	priv = display->drm_dev->dev_private;

	if (!priv) {
		pr_err("Private data is not present\n");
		rc = -EINVAL;
		goto error;
	}

	if (display->bridge) {
		pr_err("display is already initialize\n");
		goto error;
	}

	bridge = dsi_drm_bridge_init(display, display->drm_dev, enc);
	if (IS_ERR_OR_NULL(bridge)) {
		rc = PTR_ERR(bridge);
		pr_err("[%s] brige init failed, %d\n", display->name, rc);
		goto error;
	}

	display->bridge = bridge;
	priv->bridges[priv->num_bridges++] = &bridge->base;

error:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_drm_bridge_deinit(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	dsi_drm_bridge_cleanup(display->bridge);
	display->bridge = NULL;

	mutex_unlock(&display->display_lock);
	return rc;
}

/* Hook functions to call external connector, pointer validation is
 * done in dsi_display_drm_ext_bridge_init.
 */
static enum drm_connector_status dsi_display_drm_ext_detect(
		struct drm_connector *connector,
		bool force,
		void *disp)
{
	struct dsi_display *display = disp;

	return display->ext_conn->funcs->detect(display->ext_conn, force);
}

static int dsi_display_drm_ext_get_modes(
		struct drm_connector *connector, void *disp)
{
	struct dsi_display *display = disp;
	struct drm_display_mode *pmode, *pt;
	int count;

	/* if there are modes defined in panel, ignore external modes */
	if (display->panel->num_timing_nodes)
		return dsi_connector_get_modes(connector, disp);

	count = display->ext_conn->helper_private->get_modes(
			display->ext_conn);

	list_for_each_entry_safe(pmode, pt,
			&display->ext_conn->probed_modes, head) {
		list_move_tail(&pmode->head, &connector->probed_modes);
	}

	connector->display_info = display->ext_conn->display_info;

	return count;
}

static enum drm_mode_status dsi_display_drm_ext_mode_valid(
		struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *disp)
{
	struct dsi_display *display = disp;
	enum drm_mode_status status;

	/* always do internal mode_valid check */
	status = dsi_conn_mode_valid(connector, mode, disp);
	if (status != MODE_OK)
		return status;

	return display->ext_conn->helper_private->mode_valid(
			display->ext_conn, mode);
}

static int dsi_display_drm_ext_atomic_check(struct drm_connector *connector,
		void *disp,
		struct drm_connector_state *c_state)
{
	struct dsi_display *display = disp;

	return display->ext_conn->helper_private->atomic_check(
			display->ext_conn, c_state);
}

static int dsi_display_ext_get_info(struct drm_connector *connector,
	struct msm_display_info *info, void *disp)
{
	struct dsi_display *display;
	int i;

	if (!info || !disp) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	display = disp;
	if (!display->panel) {
		pr_err("invalid display panel\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	memset(info, 0, sizeof(struct msm_display_info));

	info->intf_type = DRM_MODE_CONNECTOR_DSI;
	info->num_of_h_tiles = display->ctrl_count;
	for (i = 0; i < info->num_of_h_tiles; i++)
		info->h_tile_instance[i] = display->ctrl[i].ctrl->cell_index;

	info->is_connected = connector->status != connector_status_disconnected;
	info->is_primary = true;
	info->capabilities |= (MSM_DISPLAY_CAP_VID_MODE |
		MSM_DISPLAY_CAP_EDID | MSM_DISPLAY_CAP_HOT_PLUG);

	mutex_unlock(&display->display_lock);
	return 0;
}

static int dsi_display_ext_get_mode_info(struct drm_connector *connector,
	const struct drm_display_mode *drm_mode,
	struct msm_mode_info *mode_info,
	u32 max_mixer_width, void *display)
{
	struct msm_display_topology *topology;

	if (!drm_mode || !mode_info)
		return -EINVAL;

	memset(mode_info, 0, sizeof(*mode_info));
	mode_info->frame_rate = drm_mode->vrefresh;
	mode_info->vtotal = drm_mode->vtotal;

	topology = &mode_info->topology;
	topology->num_lm = (max_mixer_width <= drm_mode->hdisplay) ? 2 : 1;
	topology->num_enc = 0;
	topology->num_intf = topology->num_lm;

	mode_info->comp_info.comp_type = MSM_DISPLAY_COMPRESSION_NONE;

	return 0;
}

static int dsi_host_ext_attach(struct mipi_dsi_host *host,
			   struct mipi_dsi_device *dsi)
{
	struct dsi_display *display = to_dsi_display(host);
	struct dsi_panel *panel;

	if (!host || !dsi || !display->panel) {
		pr_err("Invalid param\n");
		return -EINVAL;
	}

	pr_debug("DSI[%s]: channel=%d, lanes=%d, format=%d, mode_flags=%lx\n",
		dsi->name, dsi->channel, dsi->lanes,
		dsi->format, dsi->mode_flags);

	panel = display->panel;
	panel->host_config.data_lanes = 0;
	if (dsi->lanes > 0)
		panel->host_config.data_lanes |= DSI_DATA_LANE_0;
	if (dsi->lanes > 1)
		panel->host_config.data_lanes |= DSI_DATA_LANE_1;
	if (dsi->lanes > 2)
		panel->host_config.data_lanes |= DSI_DATA_LANE_2;
	if (dsi->lanes > 3)
		panel->host_config.data_lanes |= DSI_DATA_LANE_3;

	switch (dsi->format) {
	case MIPI_DSI_FMT_RGB888:
		panel->host_config.dst_format = DSI_PIXEL_FORMAT_RGB888;
		break;
	case MIPI_DSI_FMT_RGB666:
		panel->host_config.dst_format = DSI_PIXEL_FORMAT_RGB666_LOOSE;
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
		panel->host_config.dst_format = DSI_PIXEL_FORMAT_RGB666;
		break;
	case MIPI_DSI_FMT_RGB565:
	default:
		panel->host_config.dst_format = DSI_PIXEL_FORMAT_RGB565;
		break;
	}

	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO) {
		panel->panel_mode = DSI_OP_VIDEO_MODE;

		if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
			panel->video_config.traffic_mode =
					DSI_VIDEO_TRAFFIC_BURST_MODE;
		else if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE)
			panel->video_config.traffic_mode =
					DSI_VIDEO_TRAFFIC_SYNC_PULSES;
		else
			panel->video_config.traffic_mode =
					DSI_VIDEO_TRAFFIC_SYNC_START_EVENTS;

		panel->video_config.hsa_lp11_en =
			dsi->mode_flags & MIPI_DSI_MODE_VIDEO_HSA;
		panel->video_config.hbp_lp11_en =
			dsi->mode_flags & MIPI_DSI_MODE_VIDEO_HBP;
		panel->video_config.hfp_lp11_en =
			dsi->mode_flags & MIPI_DSI_MODE_VIDEO_HFP;
		panel->video_config.pulse_mode_hsa_he =
			dsi->mode_flags & MIPI_DSI_MODE_VIDEO_HSE;
		panel->video_config.bllp_lp11_en =
			dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BLLP;
		panel->video_config.eof_bllp_lp11_en =
			dsi->mode_flags & MIPI_DSI_MODE_VIDEO_EOF_BLLP;
	} else {
		panel->panel_mode = DSI_OP_CMD_MODE;
		pr_err("command mode not supported by ext bridge\n");
		return -ENOTSUPP;
	}

	panel->bl_config.type = DSI_BACKLIGHT_UNKNOWN;

	return 0;
}

static struct mipi_dsi_host_ops dsi_host_ext_ops = {
	.attach = dsi_host_ext_attach,
	.detach = dsi_host_detach,
	.transfer = dsi_host_transfer,
};

int dsi_display_drm_ext_bridge_init(struct dsi_display *display,
		struct drm_encoder *encoder, struct drm_connector *connector)
{
	struct drm_device *drm = encoder->dev;
	struct drm_bridge *bridge = encoder->bridge;
	struct drm_bridge *ext_bridge;
	struct drm_connector *ext_conn;
	struct sde_connector *sde_conn = to_sde_connector(connector);
	int rc;

	/* check if ext_bridge is already attached */
	if (display->ext_bridge)
		return 0;

	/* check if there is no external bridge defined */
	if (!display->ext_bridge_of)
		return 0;

	ext_bridge = of_drm_find_bridge(display->ext_bridge_of);
	if (IS_ERR_OR_NULL(ext_bridge)) {
		rc = PTR_ERR(ext_bridge);
		pr_err("failed to find ext bridge\n");
		goto error;
	}

	rc = drm_bridge_attach(bridge->encoder, ext_bridge, bridge);
	if (rc) {
		pr_err("[%s] ext brige attach failed, %d\n",
			display->name, rc);
		goto error;
	}

	display->ext_bridge = ext_bridge;

	/* ext bridge will init its own connector during attach,
	 * we need to extract it out of the connector list
	 */
	spin_lock_irq(&drm->mode_config.connector_list_lock);
	ext_conn = list_last_entry(&drm->mode_config.connector_list,
		struct drm_connector, head);
	if (ext_conn && ext_conn != connector &&
		ext_conn->encoder_ids[0] == bridge->encoder->base.id) {
		list_del_init(&ext_conn->head);
		display->ext_conn = ext_conn;
	}
	spin_unlock_irq(&drm->mode_config.connector_list_lock);

	/* if there is no valid external connector created, we'll use default
	 * setting from panel defined in DT file.
	 */
	if (!display->ext_conn ||
	    !display->ext_conn->funcs ||
	    !display->ext_conn->helper_private) {
		display->ext_conn = NULL;
		return 0;
	}

	/* otherwise, hook up the functions to use external connector */
	if (display->ext_conn->funcs->detect)
		sde_conn->ops.detect = dsi_display_drm_ext_detect;

	if (display->ext_conn->helper_private->get_modes)
		sde_conn->ops.get_modes = dsi_display_drm_ext_get_modes;

	if (display->ext_conn->helper_private->mode_valid)
		sde_conn->ops.mode_valid = dsi_display_drm_ext_mode_valid;

	if (display->ext_conn->helper_private->atomic_check)
		sde_conn->ops.atomic_check = dsi_display_drm_ext_atomic_check;

	sde_conn->ops.get_info =
			dsi_display_ext_get_info;
	sde_conn->ops.get_mode_info =
			dsi_display_ext_get_mode_info;

	/* add support to attach/detach */
	display->host.ops = &dsi_host_ext_ops;
	return 0;
error:
	return rc;
}

int dsi_display_get_info(struct drm_connector *connector,
		struct msm_display_info *info, void *disp)
{
	struct dsi_display *display;
	struct dsi_panel_phy_props phy_props;
	int i, rc;

	if (!info || !disp) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	display = disp;
	if (!display->panel) {
		pr_err("invalid display panel\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	rc = dsi_panel_get_phy_props(display->panel, &phy_props);
	if (rc) {
		pr_err("[%s] failed to get panel phy props, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	memset(info, 0, sizeof(struct msm_display_info));
	info->intf_type = DRM_MODE_CONNECTOR_DSI;
	info->num_of_h_tiles = display->ctrl_count;
	for (i = 0; i < info->num_of_h_tiles; i++)
		info->h_tile_instance[i] = display->ctrl[i].ctrl->cell_index;

	info->is_connected = true;
	info->is_primary = false;

	if (!strcmp(display->display_type, "primary"))
		info->is_primary = true;

	info->width_mm = phy_props.panel_width_mm;
	info->height_mm = phy_props.panel_height_mm;
	info->max_width = 1920;
	info->max_height = 1080;
	info->qsync_min_fps =
		display->panel->qsync_min_fps;

	switch (display->panel->panel_mode) {
	case DSI_OP_VIDEO_MODE:
		info->capabilities |= MSM_DISPLAY_CAP_VID_MODE;
		break;
	case DSI_OP_CMD_MODE:
		info->capabilities |= MSM_DISPLAY_CAP_CMD_MODE;
		info->is_te_using_watchdog_timer =
			display->panel->te_using_watchdog_timer |
			display->sw_te_using_wd;
		break;
	default:
		pr_err("unknwown dsi panel mode %d\n",
				display->panel->panel_mode);
		break;
	}

	if (display->panel->esd_config.esd_enabled)
		info->capabilities |= MSM_DISPLAY_ESD_ENABLED;

error:
	mutex_unlock(&display->display_lock);
	return rc;
}

static int dsi_display_get_mode_count_no_lock(struct dsi_display *display,
			u32 *count)
{
	struct dsi_dfps_capabilities dfps_caps;
	int num_dfps_rates, rc = 0;

	if (!display || !display->panel) {
		pr_err("invalid display:%d panel:%d\n", display != NULL,
				display ? display->panel != NULL : 0);
		return -EINVAL;
	}

	*count = display->panel->num_timing_nodes;

	rc = dsi_panel_get_dfps_caps(display->panel, &dfps_caps);
	if (rc) {
		pr_err("[%s] failed to get dfps caps from panel\n",
				display->name);
		return rc;
	}

	num_dfps_rates = !dfps_caps.dfps_support ? 1 :
			dfps_caps.max_refresh_rate -
			dfps_caps.min_refresh_rate + 1;

	/* Inflate num_of_modes by fps in dfps */
	*count = display->panel->num_timing_nodes * num_dfps_rates;

	return 0;
}

int dsi_display_get_mode_count(struct dsi_display *display,
			u32 *count)
{
	int rc;

	if (!display || !display->panel) {
		pr_err("invalid display:%d panel:%d\n", display != NULL,
				display ? display->panel != NULL : 0);
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	rc = dsi_display_get_mode_count_no_lock(display, count);
	mutex_unlock(&display->display_lock);

	return 0;
}

void dsi_display_put_mode(struct dsi_display *display,
	struct dsi_display_mode *mode)
{
	dsi_panel_put_mode(mode);
}

int dsi_display_get_modes(struct dsi_display *display,
			  struct dsi_display_mode **out_modes)
{
	struct dsi_dfps_capabilities dfps_caps;
	u32 num_dfps_rates, panel_mode_count, total_mode_count;
	u32 mode_idx, array_idx = 0;
	int i, rc = -EINVAL;

	if (!display || !out_modes) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	*out_modes = NULL;

	mutex_lock(&display->display_lock);

	rc = dsi_display_get_mode_count_no_lock(display, &total_mode_count);
	if (rc)
		goto error;

	/* free any previously probed modes */
	kfree(display->modes);

	display->modes = kcalloc(total_mode_count, sizeof(*display->modes),
			GFP_KERNEL);
	if (!display->modes) {
		rc = -ENOMEM;
		goto error;
	}

	rc = dsi_panel_get_dfps_caps(display->panel, &dfps_caps);
	if (rc) {
		pr_err("[%s] failed to get dfps caps from panel\n",
				display->name);
		goto error;
	}

	num_dfps_rates = !dfps_caps.dfps_support ? 1 :
			dfps_caps.max_refresh_rate -
			dfps_caps.min_refresh_rate + 1;

	panel_mode_count = display->panel->num_timing_nodes;

	for (mode_idx = 0; mode_idx < panel_mode_count; mode_idx++) {
		struct dsi_display_mode panel_mode;
		int topology_override = NO_OVERRIDE;

		if (display->cmdline_timing == mode_idx)
			topology_override = display->cmdline_topology;

		memset(&panel_mode, 0, sizeof(panel_mode));

		rc = dsi_panel_get_mode(display->panel, mode_idx,
						&panel_mode, topology_override);
		if (rc) {
			pr_err("[%s] failed to get mode idx %d from panel\n",
				   display->name, mode_idx);
			goto error;
		}

		if (display->ctrl_count > 1) { /* TODO: remove if */
			panel_mode.timing.h_active *= display->ctrl_count;
			panel_mode.timing.h_front_porch *= display->ctrl_count;
			panel_mode.timing.h_sync_width *= display->ctrl_count;
			panel_mode.timing.h_back_porch *= display->ctrl_count;
			panel_mode.timing.h_skew *= display->ctrl_count;
			panel_mode.pixel_clk_khz *= display->ctrl_count;
		}

		for (i = 0; i < num_dfps_rates; i++) {
			struct dsi_display_mode *sub_mode =
					&display->modes[array_idx];
			u32 curr_refresh_rate;

			if (!sub_mode) {
				pr_err("invalid mode data\n");
				rc = -EFAULT;
				goto error;
			}

			memcpy(sub_mode, &panel_mode, sizeof(panel_mode));

			if (dfps_caps.dfps_support) {
				curr_refresh_rate =
					sub_mode->timing.refresh_rate;
				sub_mode->timing.refresh_rate =
					dfps_caps.min_refresh_rate +
					(i % num_dfps_rates);

				dsi_display_get_dfps_timing(display,
					sub_mode, curr_refresh_rate);

				sub_mode->pixel_clk_khz =
					(DSI_H_TOTAL(&sub_mode->timing) *
					DSI_V_TOTAL(&sub_mode->timing) *
					sub_mode->timing.refresh_rate) / 1000;
			}
			array_idx++;
		}
	}

	*out_modes = display->modes;
	rc = 0;

error:
	if (rc)
		kfree(display->modes);

	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_get_panel_vfp(void *dsi_display,
	int h_active, int v_active)
{
	int i, rc = 0;
	u32 count, refresh_rate = 0;
	struct dsi_dfps_capabilities dfps_caps;
	struct dsi_display *display = (struct dsi_display *)dsi_display;

	if (!display)
		return -EINVAL;

	rc = dsi_display_get_mode_count(display, &count);
	if (rc)
		return rc;

	mutex_lock(&display->display_lock);

	if (display->panel && display->panel->cur_mode)
		refresh_rate = display->panel->cur_mode->timing.refresh_rate;

	dsi_panel_get_dfps_caps(display->panel, &dfps_caps);
	if (dfps_caps.dfps_support)
		refresh_rate = dfps_caps.max_refresh_rate;

	if (!refresh_rate) {
		mutex_unlock(&display->display_lock);
		pr_err("Null Refresh Rate\n");
		return -EINVAL;
	}

	h_active *= display->ctrl_count;

	for (i = 0; i < count; i++) {
		struct dsi_display_mode *m = &display->modes[i];

		if (m && v_active == m->timing.v_active &&
			h_active == m->timing.h_active &&
			refresh_rate == m->timing.refresh_rate) {
			rc = m->timing.v_front_porch;
			break;
		}
	}
	mutex_unlock(&display->display_lock);

	return rc;
}

int dsi_display_find_mode(struct dsi_display *display,
		const struct dsi_display_mode *cmp,
		struct dsi_display_mode **out_mode)
{
	u32 count, i;
	int rc;

	if (!display || !out_mode)
		return -EINVAL;

	*out_mode = NULL;

	rc = dsi_display_get_mode_count(display, &count);
	if (rc)
		return rc;

	if (!display->modes) {
		struct dsi_display_mode *m;

		rc = dsi_display_get_modes(display, &m);
		if (rc)
			return rc;
	}

	mutex_lock(&display->display_lock);
	for (i = 0; i < count; i++) {
		struct dsi_display_mode *m = &display->modes[i];

		if (cmp->timing.v_active == m->timing.v_active &&
			cmp->timing.h_active == m->timing.h_active &&
			cmp->timing.refresh_rate == m->timing.refresh_rate) {
			*out_mode = m;
			rc = 0;
			break;
		}
	}
	mutex_unlock(&display->display_lock);

	if (!*out_mode) {
		pr_err("[%s] failed to find mode for v_active %u h_active %u rate %u\n",
				display->name, cmp->timing.v_active,
				cmp->timing.h_active, cmp->timing.refresh_rate);
		rc = -ENOENT;
	}

	return rc;
}

/**
 * dsi_display_validate_mode_vrr() - Validate if varaible refresh case.
 * @display:     DSI display handle.
 * @cur_dsi_mode:   Current DSI mode.
 * @mode:        Mode value structure to be validated.
 *               MSM_MODE_FLAG_SEAMLESS_VRR flag is set if there
 *               is change in fps but vactive and hactive are same.
 * Return: error code.
 */
int dsi_display_validate_mode_vrr(struct dsi_display *display,
			struct dsi_display_mode *cur_dsi_mode,
			struct dsi_display_mode *mode)
{
	int rc = 0;
	struct dsi_display_mode adj_mode, cur_mode;
	struct dsi_dfps_capabilities dfps_caps;
	u32 curr_refresh_rate;

	if (!display || !mode) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (!display->panel || !display->panel->cur_mode) {
		pr_debug("Current panel mode not set\n");
		return rc;
	}

	mutex_lock(&display->display_lock);

	adj_mode = *mode;
	cur_mode = *cur_dsi_mode;

	if ((cur_mode.timing.refresh_rate != adj_mode.timing.refresh_rate) &&
		(cur_mode.timing.v_active == adj_mode.timing.v_active) &&
		(cur_mode.timing.h_active == adj_mode.timing.h_active)) {

		curr_refresh_rate = cur_mode.timing.refresh_rate;
		rc = dsi_panel_get_dfps_caps(display->panel, &dfps_caps);
		if (rc) {
			pr_err("[%s] failed to get dfps caps from panel\n",
					display->name);
			goto error;
		}

		cur_mode.timing.refresh_rate =
			adj_mode.timing.refresh_rate;

		rc = dsi_display_get_dfps_timing(display,
			&cur_mode, curr_refresh_rate);
		if (rc) {
			pr_err("[%s] seamless vrr not possible rc=%d\n",
			display->name, rc);
			goto error;
		}
		switch (dfps_caps.type) {
		/*
		 * Ignore any round off factors in porch calculation.
		 * Worse case is set to 5.
		 */
		case DSI_DFPS_IMMEDIATE_VFP:
			if (abs(DSI_V_TOTAL(&cur_mode.timing) -
				DSI_V_TOTAL(&adj_mode.timing)) > 5)
				pr_err("Mismatch vfp fps:%d new:%d given:%d\n",
				adj_mode.timing.refresh_rate,
				cur_mode.timing.v_front_porch,
				adj_mode.timing.v_front_porch);
			break;

		case DSI_DFPS_IMMEDIATE_HFP:
			if (abs(DSI_H_TOTAL(&cur_mode.timing) -
				DSI_H_TOTAL(&adj_mode.timing)) > 5)
				pr_err("Mismatch hfp fps:%d new:%d given:%d\n",
				adj_mode.timing.refresh_rate,
				cur_mode.timing.h_front_porch,
				adj_mode.timing.h_front_porch);
			break;

		default:
			pr_err("Unsupported DFPS mode %d\n",
				dfps_caps.type);
			rc = -ENOTSUPP;
		}

		pr_debug("Mode switch is seamless variable refresh\n");
		mode->dsi_mode_flags |= DSI_MODE_FLAG_VRR;
		SDE_EVT32(curr_refresh_rate, adj_mode.timing.refresh_rate,
				cur_mode.timing.h_front_porch,
				adj_mode.timing.h_front_porch);
	}

error:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_validate_mode(struct dsi_display *display,
			      struct dsi_display_mode *mode,
			      u32 flags)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;
	struct dsi_display_mode adj_mode;

	if (!display || !mode) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	adj_mode = *mode;
	adjust_timing_by_ctrl_count(display, &adj_mode);

	rc = dsi_panel_validate_mode(display->panel, &adj_mode);
	if (rc) {
		pr_err("[%s] panel mode validation failed, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_validate_timing(ctrl->ctrl, &adj_mode.timing);
		if (rc) {
			pr_err("[%s] ctrl mode validation failed, rc=%d\n",
			       display->name, rc);
			goto error;
		}

		rc = dsi_phy_validate_mode(ctrl->phy, &adj_mode.timing);
		if (rc) {
			pr_err("[%s] phy mode validation failed, rc=%d\n",
			       display->name, rc);
			goto error;
		}
	}

	if ((flags & DSI_VALIDATE_FLAG_ALLOW_ADJUST) &&
			(mode->dsi_mode_flags & DSI_MODE_FLAG_SEAMLESS)) {
		rc = dsi_display_validate_mode_seamless(display, mode);
		if (rc) {
			pr_err("[%s] seamless not possible rc=%d\n",
				display->name, rc);
			goto error;
		}
	}

error:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_set_mode(struct dsi_display *display,
			 struct dsi_display_mode *mode,
			 u32 flags)
{
	int rc = 0;
	struct dsi_display_mode adj_mode;

	if (!display || !mode || !display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	adj_mode = *mode;
	adjust_timing_by_ctrl_count(display, &adj_mode);

	/*For dynamic DSI setting, use specified clock rate */
	if (display->cached_clk_rate > 0)
		adj_mode.priv_info->clk_rate_hz = display->cached_clk_rate;

	rc = dsi_display_validate_mode_set(display, &adj_mode, flags);
	if (rc) {
		pr_err("[%s] mode cannot be set\n", display->name);
		goto error;
	}

	rc = dsi_display_set_mode_sub(display, &adj_mode, flags);
	if (rc) {
		pr_err("[%s] failed to set mode\n", display->name);
		goto error;
	}

	if (!display->panel->cur_mode) {
		display->panel->cur_mode =
			kzalloc(sizeof(struct dsi_display_mode), GFP_KERNEL);
		if (!display->panel->cur_mode) {
			rc = -ENOMEM;
			goto error;
		}
	}

	memcpy(display->panel->cur_mode, &adj_mode, sizeof(adj_mode));
error:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_set_tpg_state(struct dsi_display *display, bool enable)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_set_tpg_state(ctrl->ctrl, enable);
		if (rc) {
			pr_err("[%s] failed to set tpg state for host_%d\n",
			       display->name, i);
			goto error;
		}
	}

	display->is_tpg_enabled = enable;
error:
	return rc;
}

static int dsi_display_pre_switch(struct dsi_display *display)
{
	int rc = 0;

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI core clocks, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	rc = dsi_display_ctrl_update(display);
	if (rc) {
		pr_err("[%s] failed to update DSI controller, rc=%d\n",
			   display->name, rc);
		goto error_ctrl_clk_off;
	}

	rc = dsi_display_set_clk_src(display);
	if (rc) {
		pr_err("[%s] failed to set DSI link clock source, rc=%d\n",
			display->name, rc);
		goto error_ctrl_deinit;
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_LINK_CLK, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI link clocks, rc=%d\n",
			   display->name, rc);
		goto error_ctrl_deinit;
	}

	goto error;

error_ctrl_deinit:
	(void)dsi_display_ctrl_deinit(display);
error_ctrl_clk_off:
	(void)dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);
error:
	return rc;
}

static void dsi_display_handle_fifo_underflow(struct work_struct *work)
{
	struct dsi_display *display = NULL;

	display =  container_of(work, struct dsi_display, fifo_underflow_work);
	if (!display)
		return;
	pr_debug("handle DSI FIFO underflow error\n");

	dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);
	dsi_display_soft_reset(display);
	dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);
}

static void dsi_display_handle_fifo_overflow(struct work_struct *work)
{
	struct dsi_display *display = NULL;
	struct dsi_display_ctrl *ctrl;
	int i, rc;
	int mask = BIT(20); /* clock lane */
	int (*cb_func)(void *event_usr_ptr,
		uint32_t event_idx, uint32_t instance_idx,
		uint32_t data0, uint32_t data1,
		uint32_t data2, uint32_t data3);
	void *data;
	u32 version = 0;

	display =  container_of(work, struct dsi_display, fifo_overflow_work);
	if (!display || !display->panel ||
			(display->panel->panel_mode != DSI_OP_VIDEO_MODE))
		return;

	pr_debug("handle DSI FIFO overflow error\n");
	dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);

	/*
	 * below recovery sequence is not applicable to
	 * hw version 2.0.0, 2.1.0 and 2.2.0, so return early.
	 */
	ctrl = &display->ctrl[display->clk_master_idx];
	version = dsi_ctrl_get_hw_version(ctrl->ctrl);
	if (!version || (version < 0x20020001))
		goto end;

	/* reset ctrl and lanes */
	for (i = 0 ; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_reset(ctrl->ctrl, mask);
		rc = dsi_phy_lane_reset(ctrl->phy);
	}

	/* wait for display line count to be in active area */
	ctrl = &display->ctrl[display->clk_master_idx];
	if (ctrl->ctrl->recovery_cb.event_cb) {
		cb_func = ctrl->ctrl->recovery_cb.event_cb;
		data = ctrl->ctrl->recovery_cb.event_usr_ptr;
		rc = cb_func(data, SDE_CONN_EVENT_VID_FIFO_OVERFLOW,
				display->clk_master_idx, 0, 0, 0, 0);
		if (rc < 0) {
			pr_debug("sde callback failed\n");
			goto end;
		}
	}

	/* Enable Video mode for DSI controller */
	for (i = 0 ; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		dsi_ctrl_vid_engine_en(ctrl->ctrl, true);
	}
	/*
	 * Add sufficient delay to make sure
	 * pixel transmission has started
	 */
	udelay(200);
end:
	dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);
}

static void dsi_display_handle_lp_rx_timeout(struct work_struct *work)
{
	struct dsi_display *display = NULL;
	struct dsi_display_ctrl *ctrl;
	int i, rc;
	int mask = (BIT(20) | (0xF << 16)); /* clock lane and 4 data lane */
	int (*cb_func)(void *event_usr_ptr,
		uint32_t event_idx, uint32_t instance_idx,
		uint32_t data0, uint32_t data1,
		uint32_t data2, uint32_t data3);
	void *data;
	u32 version = 0;

	display =  container_of(work, struct dsi_display, lp_rx_timeout_work);
	if (!display || !display->panel ||
			(display->panel->panel_mode != DSI_OP_VIDEO_MODE))
		return;
	pr_debug("handle DSI LP RX Timeout error\n");

	dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);

	/*
	 * below recovery sequence is not applicable to
	 * hw version 2.0.0, 2.1.0 and 2.2.0, so return early.
	 */
	ctrl = &display->ctrl[display->clk_master_idx];
	version = dsi_ctrl_get_hw_version(ctrl->ctrl);
	if (!version || (version < 0x20020001))
		goto end;

	/* reset ctrl and lanes */
	for (i = 0 ; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_reset(ctrl->ctrl, mask);
		rc = dsi_phy_lane_reset(ctrl->phy);
	}

	ctrl = &display->ctrl[display->clk_master_idx];
	if (ctrl->ctrl->recovery_cb.event_cb) {
		cb_func = ctrl->ctrl->recovery_cb.event_cb;
		data = ctrl->ctrl->recovery_cb.event_usr_ptr;
		rc = cb_func(data, SDE_CONN_EVENT_VID_FIFO_OVERFLOW,
				display->clk_master_idx, 0, 0, 0, 0);
		if (rc < 0) {
			pr_debug("Target is in suspend/shutdown\n");
			goto end;
		}
	}

	/* Enable Video mode for DSI controller */
	for (i = 0 ; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		dsi_ctrl_vid_engine_en(ctrl->ctrl, true);
	}

	/*
	 * Add sufficient delay to make sure
	 * pixel transmission as started
	 */
	udelay(200);
end:
	dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);
}

static int dsi_display_cb_error_handler(void *data,
		uint32_t event_idx, uint32_t instance_idx,
		uint32_t data0, uint32_t data1,
		uint32_t data2, uint32_t data3)
{
	struct dsi_display *display =  data;

	if (!display)
		return -EINVAL;

	switch (event_idx) {
	case DSI_FIFO_UNDERFLOW:
		queue_work(display->err_workq, &display->fifo_underflow_work);
		break;
	case DSI_FIFO_OVERFLOW:
		queue_work(display->err_workq, &display->fifo_overflow_work);
		break;
	case DSI_LP_Rx_TIMEOUT:
		queue_work(display->err_workq, &display->lp_rx_timeout_work);
		break;
	default:
		pr_warn("unhandled error interrupt: %d\n", event_idx);
		break;
	}

	return 0;
}

static void dsi_display_register_error_handler(struct dsi_display *display)
{
	int i = 0;
	struct dsi_display_ctrl *ctrl;
	struct dsi_event_cb_info event_info;

	if (!display)
		return;

	display->err_workq = create_singlethread_workqueue("dsi_err_workq");
	if (!display->err_workq) {
		pr_err("failed to create dsi workq!\n");
		return;
	}

	INIT_WORK(&display->fifo_underflow_work,
				dsi_display_handle_fifo_underflow);
	INIT_WORK(&display->fifo_overflow_work,
				dsi_display_handle_fifo_overflow);
	INIT_WORK(&display->lp_rx_timeout_work,
				dsi_display_handle_lp_rx_timeout);

	memset(&event_info, 0, sizeof(event_info));

	event_info.event_cb = dsi_display_cb_error_handler;
	event_info.event_usr_ptr = display;

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		ctrl->ctrl->irq_info.irq_err_cb = event_info;
	}
}

static void dsi_display_unregister_error_handler(struct dsi_display *display)
{
	int i = 0;
	struct dsi_display_ctrl *ctrl;

	if (!display)
		return;

	for (i = 0; i < display->ctrl_count; i++) {
		ctrl = &display->ctrl[i];
		memset(&ctrl->ctrl->irq_info.irq_err_cb,
				0, sizeof(struct dsi_event_cb_info));
	}

	if (display->err_workq)
		destroy_workqueue(display->err_workq);
}

int dsi_display_prepare(struct dsi_display *display)
{
	int rc = 0;
	struct dsi_display_mode *mode;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (!display->panel->cur_mode) {
		pr_err("no valid mode set for the display");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	mode = display->panel->cur_mode;

	dsi_display_set_ctrl_esd_check_flag(display, false);

	if (mode->dsi_mode_flags & DSI_MODE_FLAG_DMS) {
		if (display->is_cont_splash_enabled) {
			pr_err("DMS is not supposed to be set on first frame\n");
			return -EINVAL;
		}
		/* update dsi ctrl for new mode */
		rc = dsi_display_pre_switch(display);
		if (rc)
			pr_err("[%s] panel pre-prepare-res-switch failed, rc=%d\n",
					display->name, rc);
		goto error;
	}

	if (!display->is_cont_splash_enabled) {
		/*
		 * For continuous splash usecase we skip panel
		 * pre prepare since the regulator vote is already
		 * taken care in splash resource init
		 */
		rc = dsi_panel_pre_prepare(display->panel);
		if (rc) {
			pr_err("[%s] panel pre-prepare failed, rc=%d\n",
					display->name, rc);
			goto error;
		}
	}

	/* Set up ctrl isr before enabling core clk */
	dsi_display_ctrl_isr_configure(display, true);

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI core clocks, rc=%d\n",
		       display->name, rc);
		goto error_panel_post_unprep;
	}

	/*
	 * If ULPS during suspend feature is enabled, then DSI PHY was
	 * left on during suspend. In this case, we do not need to reset/init
	 * PHY. This would have already been done when the CORE clocks are
	 * turned on. However, if cont splash is disabled, the first time DSI
	 * is powered on, phy init needs to be done unconditionally.
	 */
	if (!display->panel->ulps_suspend_enabled || !display->ulps_enabled) {
		rc = dsi_display_phy_sw_reset(display);
		if (rc) {
			pr_err("[%s] failed to reset phy, rc=%d\n",
				display->name, rc);
			goto error_ctrl_clk_off;
		}

		rc = dsi_display_phy_enable(display);
		if (rc) {
			pr_err("[%s] failed to enable DSI PHY, rc=%d\n",
			       display->name, rc);
			goto error_ctrl_clk_off;
		}
	}

	rc = dsi_display_set_clk_src(display);
	if (rc) {
		pr_err("[%s] failed to set DSI link clock source, rc=%d\n",
			display->name, rc);
		goto error_phy_disable;
	}

	rc = dsi_display_ctrl_init(display);
	if (rc) {
		pr_err("[%s] failed to setup DSI controller, rc=%d\n",
		       display->name, rc);
		goto error_phy_disable;
	}
	/* Set up DSI ERROR event callback */
	dsi_display_register_error_handler(display);

	rc = dsi_display_ctrl_host_enable(display);
	if (rc) {
		pr_err("[%s] failed to enable DSI host, rc=%d\n",
		       display->name, rc);
		goto error_ctrl_deinit;
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_LINK_CLK, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI link clocks, rc=%d\n",
		       display->name, rc);
		goto error_host_engine_off;
	}

	if (!display->is_cont_splash_enabled) {
		/*
		 * For continuous splash usecase, skip panel prepare and
		 * ctl reset since the pnael and ctrl is already in active
		 * state and panel on commands are not needed
		 */
		rc = dsi_display_soft_reset(display);
		if (rc) {
			pr_err("[%s] failed soft reset, rc=%d\n",
					display->name, rc);
			goto error_ctrl_link_off;
		}

		rc = dsi_panel_prepare(display->panel);
		if (rc) {
			pr_err("[%s] panel prepare failed, rc=%d\n",
					display->name, rc);
			goto error_ctrl_link_off;
		}
	}
	goto error;

error_ctrl_link_off:
	(void)dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_LINK_CLK, DSI_CLK_OFF);
error_host_engine_off:
	(void)dsi_display_ctrl_host_disable(display);
error_ctrl_deinit:
	(void)dsi_display_ctrl_deinit(display);
error_phy_disable:
	(void)dsi_display_phy_disable(display);
error_ctrl_clk_off:
	(void)dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);
error_panel_post_unprep:
	(void)dsi_panel_post_unprepare(display->panel);
error:
	mutex_unlock(&display->display_lock);
	return rc;
}

static int dsi_display_calc_ctrl_roi(const struct dsi_display *display,
		const struct dsi_display_ctrl *ctrl,
		const struct msm_roi_list *req_rois,
		struct dsi_rect *out_roi)
{
	const struct dsi_rect *bounds = &ctrl->ctrl->mode_bounds;
	struct dsi_display_mode *cur_mode;
	struct msm_roi_caps *roi_caps;
	struct dsi_rect req_roi = { 0 };
	int rc = 0;

	cur_mode = display->panel->cur_mode;
	if (!cur_mode)
		return 0;

	roi_caps = &cur_mode->priv_info->roi_caps;
	if (req_rois->num_rects > roi_caps->num_roi) {
		pr_err("request for %d rois greater than max %d\n",
				req_rois->num_rects,
				roi_caps->num_roi);
		rc = -EINVAL;
		goto exit;
	}

	/**
	 * if no rois, user wants to reset back to full resolution
	 * note: h_active is already divided by ctrl_count
	 */
	if (!req_rois->num_rects) {
		*out_roi = *bounds;
		goto exit;
	}

	/* intersect with the bounds */
	req_roi.x = req_rois->roi[0].x1;
	req_roi.y = req_rois->roi[0].y1;
	req_roi.w = req_rois->roi[0].x2 - req_rois->roi[0].x1;
	req_roi.h = req_rois->roi[0].y2 - req_rois->roi[0].y1;
	dsi_rect_intersect(&req_roi, bounds, out_roi);

exit:
	/* adjust the ctrl origin to be top left within the ctrl */
	out_roi->x = out_roi->x - bounds->x;

	pr_debug("ctrl%d:%d: req (%d,%d,%d,%d) bnd (%d,%d,%d,%d) out (%d,%d,%d,%d)\n",
			ctrl->dsi_ctrl_idx, ctrl->ctrl->cell_index,
			req_roi.x, req_roi.y, req_roi.w, req_roi.h,
			bounds->x, bounds->y, bounds->w, bounds->h,
			out_roi->x, out_roi->y, out_roi->w, out_roi->h);

	return rc;
}

static int dsi_display_qsync(struct dsi_display *display, bool enable)
{
	int i;
	int rc = 0;

	if (!display->panel->qsync_min_fps) {
		pr_err("%s:ERROR: qsync set, but no fps\n", __func__);
		return 0;
	}

	mutex_lock(&display->display_lock);

	for (i = 0; i < display->ctrl_count; i++) {

		if (enable) {
			/* send the commands to enable qsync */
			rc = dsi_panel_send_qsync_on_dcs(display->panel, i);
			if (rc) {
				pr_err("fail qsync ON cmds rc:%d\n", rc);
				goto exit;
			}
		} else {
			/* send the commands to enable qsync */
			rc = dsi_panel_send_qsync_off_dcs(display->panel, i);
			if (rc) {
				pr_err("fail qsync OFF cmds rc:%d\n", rc);
				goto exit;
			}
		}
	}

exit:
	SDE_EVT32(enable, display->panel->qsync_min_fps, rc);
	mutex_unlock(&display->display_lock);
	return rc;
}

static int dsi_display_set_roi(struct dsi_display *display,
		struct msm_roi_list *rois)
{
	struct dsi_display_mode *cur_mode;
	struct msm_roi_caps *roi_caps;
	int rc = 0;
	int i;

	if (!display || !rois || !display->panel)
		return -EINVAL;

	cur_mode = display->panel->cur_mode;
	if (!cur_mode)
		return 0;

	roi_caps = &cur_mode->priv_info->roi_caps;
	if (!roi_caps->enabled)
		return 0;

	for (i = 0; i < display->ctrl_count; i++) {
		struct dsi_display_ctrl *ctrl = &display->ctrl[i];
		struct dsi_rect ctrl_roi;
		bool changed = false;

		rc = dsi_display_calc_ctrl_roi(display, ctrl, rois, &ctrl_roi);
		if (rc) {
			pr_err("dsi_display_calc_ctrl_roi failed rc %d\n", rc);
			return rc;
		}

		rc = dsi_ctrl_set_roi(ctrl->ctrl, &ctrl_roi, &changed);
		if (rc) {
			pr_err("dsi_ctrl_set_roi failed rc %d\n", rc);
			return rc;
		}

		if (!changed)
			continue;

		/* send the new roi to the panel via dcs commands */
		rc = dsi_panel_send_roi_dcs(display->panel, i, &ctrl_roi);
		if (rc) {
			pr_err("dsi_panel_set_roi failed rc %d\n", rc);
			return rc;
		}

		/* re-program the ctrl with the timing based on the new roi */
		rc = dsi_ctrl_setup(ctrl->ctrl);
		if (rc) {
			pr_err("dsi_ctrl_setup failed rc %d\n", rc);
			return rc;
		}
	}

	return rc;
}

int dsi_display_pre_kickoff(struct drm_connector *connector,
		struct dsi_display *display,
		struct msm_display_kickoff_params *params)
{
	int rc = 0;
	int i;
	bool enable;

	/* check and setup MISR */
	if (display->misr_enable)
		_dsi_display_setup_misr(display);

	if (params->qsync_update) {
		enable = (params->qsync_mode > 0) ? true : false;
		rc = dsi_display_qsync(display, enable);
		if (rc)
			pr_err("%s failed to send qsync commands",
				__func__);
		SDE_EVT32(params->qsync_mode, rc);
	}

	rc = dsi_display_set_roi(display, params->rois);

	/* dynamic DSI clock setting */
	if (atomic_read(&display->clkrate_change_pending)) {
		mutex_lock(&display->display_lock);
		/*
		 * acquire panel_lock to make sure no commands are in progress
		 */
		dsi_panel_acquire_panel_lock(display->panel);

		/*
		 * Wait for DSI command engine not to be busy sending data
		 * from display engine.
		 * If waiting fails, return "rc" instead of below "ret" so as
		 * not to impact DRM commit. The clock updating would be
		 * deferred to the next DRM commit.
		 */
		for (i = 0; i < display->ctrl_count; i++) {
			struct dsi_ctrl *ctrl = display->ctrl[i].ctrl;
			int ret = 0;

			ret = dsi_ctrl_wait_for_cmd_mode_mdp_idle(ctrl);
			if (ret)
				goto wait_failure;
		}

		/*
		 * Don't check the return value so as not to impact DRM commit
		 * when error occurs.
		 */
		(void)dsi_display_force_update_dsi_clk(display);
wait_failure:
		/* release panel_lock */
		dsi_panel_release_panel_lock(display->panel);
		mutex_unlock(&display->display_lock);
	}

	return rc;
}

int dsi_display_config_ctrl_for_cont_splash(struct dsi_display *display)
{
	int rc = 0;

	if (!display || !display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (!display->panel->cur_mode) {
		pr_err("no valid mode set for the display");
		return -EINVAL;
	}

	if (!display->is_cont_splash_enabled)
		return 0;

	if (display->config.panel_mode == DSI_OP_VIDEO_MODE) {
		rc = dsi_display_vid_engine_enable(display);
		if (rc) {
			pr_err("[%s]failed to enable DSI video engine, rc=%d\n",
			       display->name, rc);
			goto error_out;
		}
	} else if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_cmd_engine_enable(display);
		if (rc) {
			pr_err("[%s]failed to enable DSI cmd engine, rc=%d\n",
			       display->name, rc);
			goto error_out;
		}
	} else {
		pr_err("[%s] Invalid configuration\n", display->name);
		rc = -EINVAL;
	}

error_out:
	return rc;
}

int dsi_display_enable(struct dsi_display *display)
{
	int rc = 0;
	struct dsi_display_mode *mode;

	if (!display || !display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (!display->panel->cur_mode) {
		pr_err("no valid mode set for the display");
		return -EINVAL;
	}

	/* Engine states and panel states are populated during splash
	 * resource init and hence we return early
	 */
	if (display->is_cont_splash_enabled) {

		dsi_display_config_ctrl_for_cont_splash(display);

		rc = dsi_display_splash_res_cleanup(display);
		if (rc) {
			pr_err("Continuous splash res cleanup failed, rc=%d\n",
				rc);
			return -EINVAL;
		}

		display->panel->panel_initialized = true;
		pr_debug("cont splash enabled, display enable not required\n");
		return 0;
	}

	mutex_lock(&display->display_lock);

	mode = display->panel->cur_mode;

	if (mode->dsi_mode_flags & DSI_MODE_FLAG_DMS) {
		rc = dsi_panel_post_switch(display->panel);
		if (rc) {
			pr_err("[%s] failed to switch DSI panel mode, rc=%d\n",
				   display->name, rc);
			goto error;
		}
	} else {
		rc = dsi_panel_enable(display->panel);
		if (rc) {
			pr_err("[%s] failed to enable DSI panel, rc=%d\n",
			       display->name, rc);
			goto error;
		}
	}

	if (mode->priv_info->dsc_enabled) {
		mode->priv_info->dsc.pic_width *= display->ctrl_count;
		rc = dsi_panel_update_pps(display->panel);
		if (rc) {
			pr_err("[%s] panel pps cmd update failed, rc=%d\n",
				display->name, rc);
			goto error;
		}
	}

	if (mode->dsi_mode_flags & DSI_MODE_FLAG_DMS) {
		rc = dsi_panel_switch(display->panel);
		if (rc)
			pr_err("[%s] failed to switch DSI panel mode, rc=%d\n",
				   display->name, rc);

		goto error;
	}

	if (display->config.panel_mode == DSI_OP_VIDEO_MODE) {
		rc = dsi_display_vid_engine_enable(display);
		if (rc) {
			pr_err("[%s]failed to enable DSI video engine, rc=%d\n",
			       display->name, rc);
			goto error_disable_panel;
		}
	} else if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_cmd_engine_enable(display);
		if (rc) {
			pr_err("[%s]failed to enable DSI cmd engine, rc=%d\n",
			       display->name, rc);
			goto error_disable_panel;
		}
	} else {
		pr_err("[%s] Invalid configuration\n", display->name);
		rc = -EINVAL;
		goto error_disable_panel;
	}

	goto error;

error_disable_panel:
	(void)dsi_panel_disable(display->panel);
error:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_post_enable(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_panel_post_enable(display->panel);
	if (rc)
		pr_err("[%s] panel post-enable failed, rc=%d\n",
		       display->name, rc);

	/* remove the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE)
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);

	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_pre_disable(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE)
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);

	rc = dsi_panel_pre_disable(display->panel);
	if (rc)
		pr_err("[%s] panel pre-disable failed, rc=%d\n",
		       display->name, rc);

	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_disable(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_display_wake_up(display);
	if (rc)
		pr_err("[%s] display wake up failed, rc=%d\n",
		       display->name, rc);

	if (display->config.panel_mode == DSI_OP_VIDEO_MODE) {
		rc = dsi_display_vid_engine_disable(display);
		if (rc)
			pr_err("[%s]failed to disable DSI vid engine, rc=%d\n",
			       display->name, rc);
	} else if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_cmd_engine_disable(display);
		if (rc)
			pr_err("[%s]failed to disable DSI cmd engine, rc=%d\n",
			       display->name, rc);
	} else {
		pr_err("[%s] Invalid configuration\n", display->name);
		rc = -EINVAL;
	}

	rc = dsi_panel_disable(display->panel);
	if (rc)
		pr_err("[%s] failed to disable DSI panel, rc=%d\n",
		       display->name, rc);

	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_update_pps(char *pps_cmd, void *disp)
{
	struct dsi_display *display;

	if (pps_cmd == NULL || disp == NULL) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	display = disp;
	mutex_lock(&display->display_lock);
	memcpy(display->panel->dsc_pps_cmd, pps_cmd, DSI_CMD_PPS_SIZE);
	mutex_unlock(&display->display_lock);

	return 0;
}

int dsi_display_unprepare(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_display_wake_up(display);
	if (rc)
		pr_err("[%s] display wake up failed, rc=%d\n",
		       display->name, rc);

	rc = dsi_panel_unprepare(display->panel);
	if (rc)
		pr_err("[%s] panel unprepare failed, rc=%d\n",
		       display->name, rc);

	rc = dsi_display_ctrl_host_disable(display);
	if (rc)
		pr_err("[%s] failed to disable DSI host, rc=%d\n",
		       display->name, rc);

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_LINK_CLK, DSI_CLK_OFF);
	if (rc)
		pr_err("[%s] failed to disable Link clocks, rc=%d\n",
		       display->name, rc);

	/* Free up DSI ERROR event callback */
	dsi_display_unregister_error_handler(display);

	rc = dsi_display_ctrl_deinit(display);
	if (rc)
		pr_err("[%s] failed to deinit controller, rc=%d\n",
		       display->name, rc);

	if (!display->panel->ulps_suspend_enabled) {
		rc = dsi_display_phy_disable(display);
		if (rc)
			pr_err("[%s] failed to disable DSI PHY, rc=%d\n",
			       display->name, rc);
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);
	if (rc)
		pr_err("[%s] failed to disable DSI clocks, rc=%d\n",
		       display->name, rc);

	/* destrory dsi isr set up */
	dsi_display_ctrl_isr_configure(display, false);

	rc = dsi_panel_post_unprepare(display->panel);
	if (rc)
		pr_err("[%s] panel post-unprepare failed, rc=%d\n",
		       display->name, rc);

	mutex_unlock(&display->display_lock);
	return rc;
}

static int __init dsi_display_register(void)
{
	dsi_phy_drv_register();
	dsi_ctrl_drv_register();

	dsi_display_parse_boot_display_selection();

	return platform_driver_register(&dsi_display_driver);
}

static void __exit dsi_display_unregister(void)
{
	platform_driver_unregister(&dsi_display_driver);
	dsi_ctrl_drv_unregister();
	dsi_phy_drv_unregister();
}
module_param_string(dsi_display0, dsi_display_primary, MAX_CMDLINE_PARAM_LEN,
								0600);
MODULE_PARM_DESC(dsi_display0,
	"msm_drm.dsi_display0=<display node>:<configX> where <display node> is 'primary dsi display node name' and <configX> where x represents index in the topology list");
module_param_string(dsi_display1, dsi_display_secondary, MAX_CMDLINE_PARAM_LEN,
								0600);
MODULE_PARM_DESC(dsi_display1,
	"msm_drm.dsi_display1=<display node>:<configX> where <display node> is 'secondary dsi display node name' and <configX> where x represents index in the topology list");
module_init(dsi_display_register);
module_exit(dsi_display_unregister);

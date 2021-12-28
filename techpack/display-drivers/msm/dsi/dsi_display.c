// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <video/mipi_display.h>


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
#include "sde_trace.h"

#include "mi_disp_feature.h"
#include "mi_dsi_display.h"
#include "mi_disp_print.h"
#include "mi_disp_lhbm.h"
#include "mi_panel_id.h"

#define to_dsi_display(x) container_of(x, struct dsi_display, host)
#define INT_BASE_10 10

#define MISR_BUFF_SIZE	256
#define ESD_MODE_STRING_MAX_LEN 256
#define ESD_TRIGGER_STRING_MAX_LEN 10

#define MAX_NAME_SIZE	64
#define MAX_TE_RECHECKS 5

#define DSI_CLOCK_BITRATE_RADIX 10
#define MAX_TE_SOURCE_ID  2

#define SEC_PANEL_NAME_MAX_LEN  256

u8 dbgfs_tx_cmd_buf[SZ_4K];
static char dsi_display_primary[MAX_CMDLINE_PARAM_LEN];
static char dsi_display_secondary[MAX_CMDLINE_PARAM_LEN];
static struct dsi_display_boot_param boot_displays[MAX_DSI_ACTIVE_DISPLAY] = {
	{.boot_param = dsi_display_primary},
	{.boot_param = dsi_display_secondary},
};

static void dsi_display_panel_id_notification(struct dsi_display *display);

static const struct of_device_id dsi_display_dt_match[] = {
	{.compatible = "qcom,dsi-display"},
	{}
};

char *mi_dsi_display_get_cmdline_panel_info(struct dsi_display *display)
{
	char *buffer = NULL, *buffer_dup = NULL;
	char *pname = NULL;
	char *panel_info = NULL;
	int index = DSI_PRIMARY;

	if (!display) {
		DISP_ERROR("Invalid params\n");
		return NULL;
	}

	if (!strcmp(display->display_type, "primary")) {
		index = DSI_PRIMARY;
	} else if (!strcmp(display->display_type, "secondary")) {
		index = DSI_SECONDARY;
	} else {
		DISP_ERROR("Invalid display_type params\n");
		return NULL;
	}

	buffer = kstrdup(boot_displays[index].boot_param, GFP_KERNEL);
	if (!buffer)
		return NULL;
	buffer_dup = buffer;

	buffer = strrchr(buffer, ',');
	if (buffer && *buffer) {
		pname = ++buffer;
	} else {
		goto exit;
	}

	buffer = strrchr(pname, ':');
	if (buffer)
		*buffer = '\0';

	panel_info = kstrdup(pname, GFP_KERNEL);

exit:
	kfree(buffer_dup);
	return panel_info;
}

bool is_skip_op_required(struct dsi_display *display)
{
	if (!display)
		return false;

	return (display->is_cont_splash_enabled || display->trusted_vm_env);
}

static bool is_sim_panel(struct dsi_display *display)
{
	if (!display || !display->panel)
		return false;

	return display->panel->te_using_watchdog_timer;
}

static void dsi_display_mask_ctrl_error_interrupts(struct dsi_display *display,
			u32 mask, bool enable)
{
	int i;
	struct dsi_display_ctrl *ctrl;

	if (!display)
		return;

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if ((!ctrl) || (!ctrl->ctrl))
			continue;

		mutex_lock(&ctrl->ctrl->ctrl_lock);

		dsi_ctrl_mask_error_status_interrupts(ctrl->ctrl, mask, enable);

		mutex_unlock(&ctrl->ctrl->ctrl_lock);
	}
}

static int dsi_display_config_clk_gating(struct dsi_display *display,
					bool enable)
{
	int rc = 0, i = 0;
	struct dsi_display_ctrl *mctrl, *ctrl;
	enum dsi_clk_gate_type clk_selection;
	enum dsi_clk_gate_type const default_clk_select = PIXEL_CLK | DSI_PHY;

	if (!display) {
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (display->panel->host_config.force_hs_clk_lane) {
		DSI_DEBUG("no dsi clock gating for continuous clock mode\n");
		return 0;
	}

	mctrl = &display->ctrl[display->clk_master_idx];
	if (!mctrl) {
		DSI_ERR("Invalid controller\n");
		return -EINVAL;
	}

	clk_selection = display->clk_gating_config;

	if (!enable) {
		/* for disable path, make sure to disable all clk gating */
		clk_selection = DSI_CLK_ALL;
	} else if (!clk_selection || clk_selection > DSI_CLK_NONE) {
		/* Default selection, no overrides */
		clk_selection = default_clk_select;
	} else if (clk_selection == DSI_CLK_NONE) {
		clk_selection = 0;
	}

	DSI_DEBUG("%s clock gating Byte:%s Pixel:%s PHY:%s\n",
		enable ? "Enabling" : "Disabling",
		clk_selection & BYTE_CLK ? "yes" : "no",
		clk_selection & PIXEL_CLK ? "yes" : "no",
		clk_selection & DSI_PHY ? "yes" : "no");
	rc = dsi_ctrl_config_clk_gating(mctrl->ctrl, enable, clk_selection);
	if (rc) {
		DSI_ERR("[%s] failed to %s clk gating for clocks %d, rc=%d\n",
				display->name, enable ? "enable" : "disable",
				clk_selection, rc);
		return rc;
	}

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == mctrl))
			continue;
		/**
		 * In Split DSI usecase we should not enable clock gating on
		 * DSI PHY1 to ensure no display atrifacts are seen.
		 */
		clk_selection &= ~DSI_PHY;
		rc = dsi_ctrl_config_clk_gating(ctrl->ctrl, enable,
				clk_selection);
		if (rc) {
			DSI_ERR("[%s] failed to %s clk gating for clocks %d, rc=%d\n",
				display->name, enable ? "enable" : "disable",
				clk_selection, rc);
			return rc;
		}
	}

	return 0;
}

static void dsi_display_set_ctrl_esd_check_flag(struct dsi_display *display,
			bool enable)
{
	int i;
	struct dsi_display_ctrl *ctrl;

	if (!display)
		return;

	display_for_each_ctrl(i, display) {
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

	display_for_each_ctrl(i, display) {
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
	u32 bl_scale, bl_scale_sv, bl_max;
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

	bl_max = panel->mi_cfg.normal_max_brightness_clone;
	bl_temp = bl_lvl;
	if (!panel->bl_config.dimming_enabled) {
		/* scale backlight */
		bl_scale = panel->bl_config.bl_scale;
		bl_temp = panel->bl_config.bl_level * bl_scale / MAX_BL_SCALE_LEVEL;

		bl_scale_sv = panel->bl_config.bl_scale_sv;
		bl_temp = (u32)bl_temp * bl_scale_sv / MAX_SV_BL_SCALE_LEVEL;

		if (bl_temp && (bl_temp < panel->bl_config.bl_min_level))
			bl_temp = panel->bl_config.bl_min_level;
	} else {
		/* use bl_temp as index of dimming bl lut to find the dimming panel backlight */
		if (bl_temp != 0 && panel->bl_config.dimming_bl_lut &&
	    		bl_temp < bl_max + 1) {
			bl_temp = (u32)(DIM_PARAM * bl_lvl) / (bl_lvl + bl_max);
		}
		if (bl_temp && (bl_temp < panel->bl_config.dimming_min_bl))
                        bl_temp = panel->bl_config.dimming_min_bl;
	}

	if (bl_temp > panel->bl_config.bl_max_level)
		bl_temp = panel->bl_config.bl_max_level;

	DSI_DEBUG("dimming enable :%d, bl_temp = %u\n", panel->bl_config.dimming_enabled, (u32)bl_temp);

	rc = dsi_panel_set_backlight(panel, (u32)bl_temp);
	if (rc)
		DSI_ERR("unable to set backlight\n");

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

static int dsi_display_cmd_engine_enable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	bool skip_op = display->trusted_vm_env;

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	mutex_lock(&m_ctrl->ctrl->ctrl_lock);

	rc = dsi_ctrl_set_cmd_engine_state(m_ctrl->ctrl,
				DSI_CTRL_ENGINE_ON, skip_op);
	if (rc) {
		DSI_ERR("[%s] enable mcmd engine failed, skip_op:%d rc:%d\n",
		       display->name, skip_op, rc);
		goto done;
	}

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_cmd_engine_state(ctrl->ctrl,
					DSI_CTRL_ENGINE_ON, skip_op);
		if (rc) {
			DSI_ERR(
			    "[%s] enable cmd engine failed, skip_op:%d rc:%d\n",
			       display->name, skip_op, rc);
			goto error_disable_master;
		}
	}

	goto done;
error_disable_master:
	(void)dsi_ctrl_set_cmd_engine_state(m_ctrl->ctrl,
				DSI_CTRL_ENGINE_OFF, skip_op);
done:
	mutex_unlock(&m_ctrl->ctrl->ctrl_lock);
	return rc;
}

static int dsi_display_cmd_engine_disable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	bool skip_op = display->trusted_vm_env;

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	mutex_lock(&m_ctrl->ctrl->ctrl_lock);

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_cmd_engine_state(ctrl->ctrl,
					DSI_CTRL_ENGINE_OFF, skip_op);
		if (rc)
			DSI_ERR(
			   "[%s] disable cmd engine failed, skip_op:%d rc:%d\n",
				display->name, skip_op, rc);
	}

	rc = dsi_ctrl_set_cmd_engine_state(m_ctrl->ctrl,
				DSI_CTRL_ENGINE_OFF, skip_op);
	if (rc)
		DSI_ERR("[%s] disable mcmd engine failed, skip_op:%d rc:%d\n",
			display->name, skip_op, rc);

	mutex_unlock(&m_ctrl->ctrl->ctrl_lock);
	return rc;
}

static void dsi_display_aspace_cb_locked(void *cb_data, bool is_detach)
{
	struct dsi_display *display;
	struct dsi_display_ctrl *display_ctrl;
	int rc, cnt;

	if (!cb_data) {
		DSI_ERR("aspace cb called with invalid cb_data\n");
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
			DSI_ERR("failed to get the iova rc %d\n", rc);
			goto end;
		}

		display->vaddr =
			(void *) msm_gem_get_vaddr(display->tx_cmd_buf);

		if (IS_ERR_OR_NULL(display->vaddr)) {
			DSI_ERR("failed to get va rc %d\n", rc);
			goto end;
		}
	}

	display_for_each_ctrl(cnt, display) {
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

	SDE_EVT32(SDE_EVTLOG_FUNC_CASE1);
	complete_all(&display->esd_te_gate);
	return IRQ_HANDLED;
}

static void dsi_display_change_te_irq_status(struct dsi_display *display,
					bool enable)
{
	if (!display) {
		DSI_ERR("Invalid params\n");
		return;
	}

	/* Handle unbalanced irq enable/disable calls */
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
	unsigned int te_irq;

	pdev = display->pdev;
	if (!pdev) {
		DSI_ERR("invalid platform device\n");
		return;
	}

	dev = &pdev->dev;
	if (!dev) {
		DSI_ERR("invalid device\n");
		return;
	}

	if (display->trusted_vm_env) {
		DSI_INFO("GPIO's are not enabled in trusted VM\n");
		return;
	}

	if (!gpio_is_valid(display->disp_te_gpio)) {
		rc = -EINVAL;
		goto error;
	}

	init_completion(&display->esd_te_gate);
	te_irq = gpio_to_irq(display->disp_te_gpio);

	/* Avoid deferred spurious irqs with disable_irq() */
	irq_set_status_flags(te_irq, IRQ_DISABLE_UNLAZY);

	rc = devm_request_irq(dev, te_irq, dsi_display_panel_te_irq_handler,
			      IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			      "TE_GPIO", display);
	if (rc) {
		DSI_ERR("TE request_irq failed for ESD rc:%d\n", rc);
		irq_clear_status_flags(te_irq, IRQ_DISABLE_UNLAZY);
		goto error;
	}

	disable_irq(te_irq);
	display->is_te_irq_enabled = false;

	return;

error:
	/* disable the TE based ESD check */
	DSI_WARN("Unable to register for TE IRQ\n");
	if (display->panel->esd_config.status_mode == ESD_MODE_PANEL_TE)
		display->panel->esd_config.esd_enabled = false;
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
		DSI_ERR("Failed to allocate cmd tx buf memory\n");
		rc = -ENOMEM;
		goto error;
	}

	display->cmd_buffer_size = SZ_4K;

	display->aspace = msm_gem_smmu_address_space_get(
			display->drm_dev, MSM_SMMU_DOMAIN_UNSECURE);

	if (PTR_ERR(display->aspace) == -ENODEV) {
		display->aspace = NULL;
		DSI_DEBUG("IOMMU not present, relying on VRAM\n");
	} else if (IS_ERR_OR_NULL(display->aspace)) {
		rc = PTR_ERR(display->aspace);
		display->aspace = NULL;
		DSI_ERR("failed to get aspace %d\n", rc);
		goto free_gem;
	} else if (display->aspace) {
		/* register to aspace */
		rc = msm_gem_address_space_register_cb(display->aspace,
				dsi_display_aspace_cb_locked, (void *)display);
		if (rc) {
			DSI_ERR("failed to register callback %d\n", rc);
			goto free_gem;
		}
	}

	rc = msm_gem_get_iova(display->tx_cmd_buf, display->aspace,
				&(display->cmd_buffer_iova));
	if (rc) {
		DSI_ERR("failed to get the iova rc %d\n", rc);
		goto free_aspace_cb;
	}

	display->vaddr =
		(void *) msm_gem_get_vaddr(display->tx_cmd_buf);
	if (IS_ERR_OR_NULL(display->vaddr)) {
		DSI_ERR("failed to get va rc %d\n", rc);
		rc = -EINVAL;
		goto put_iova;
	}

	display_for_each_ctrl(cnt, display) {
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

	for (j = 0; j < config->groups; ++j) {
		for (i = 0; i < len; ++i) {
			if (config->return_buf[i] !=
				config->status_value[group + i]) {
				DRM_ERROR("mismatch: 0x%x\n",
						config->return_buf[i]);
				break;
			}
		}

		if (i == len)
			return true;
		group += len;
	}

	return false;
}

static void dsi_display_parse_demura_data(struct dsi_display *display)
{
	int rc = 0;

	display->panel_id = ~0x0;
	if (display->fw) {
		DSI_DEBUG("FW definition unsupported for Demura panel data\n");
		return;
	}

	rc = of_property_read_u64(display->pdev->dev.of_node,
			"qcom,demura-panel-id", &display->panel_id);
	if (rc) {
		DSI_DEBUG("No panel ID is present for this display\n");
	} else if (!display->panel_id) {
		DSI_DEBUG("Dummy panel ID node present for this display\n");
		display->panel_id = ~0x0;
	} else {
		DSI_DEBUG("panel id found: %lx\n", display->panel_id);
	}
}

static void dsi_display_parse_te_data(struct dsi_display *display)
{
	struct platform_device *pdev;
	struct device *dev;
	int rc = 0;
	u32 val = 0;

	pdev = display->pdev;
	if (!pdev) {
		DSI_ERR("Invalid platform device\n");
		return;
	}

	dev = &pdev->dev;
	if (!dev) {
		DSI_ERR("Invalid platform device\n");
		return;
	}

	display->disp_te_gpio = of_get_named_gpio(dev->of_node,
					"qcom,platform-te-gpio", 0);

	if (display->fw)
		rc = dsi_parser_read_u32(display->parser_node,
			"qcom,panel-te-source", &val);
	else
		rc = of_property_read_u32(dev->of_node,
			"qcom,panel-te-source", &val);

	if (rc || (val  > MAX_TE_SOURCE_ID)) {
		DSI_ERR("invalid vsync source selection\n");
		val = 0;
	}

	display->te_source = val;
}

static void dsi_display_set_cmd_tx_ctrl_flags(struct dsi_display *display,
		struct dsi_cmd_desc *cmd)
{
	struct dsi_display_ctrl *ctrl, *m_ctrl;
	struct mipi_dsi_msg *msg = &cmd->msg;
	u32 flags = 0;
	int i = 0;

	m_ctrl = &display->ctrl[display->clk_master_idx];
	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;

		/*
		 * Set cmd transfer mode flags.
		 * 1) Default selection is CMD fetch from memory.
		 * 2) In secure session override and use FIFO rather than
		 *    memory.
		 * 3) If cmd_len is greater than FIFO size non embedded mode of
		 *    tx is used.
		 */
		flags = DSI_CTRL_CMD_FETCH_MEMORY;
		if (ctrl->ctrl->secure_mode) {
			flags &= ~DSI_CTRL_CMD_FETCH_MEMORY;
			flags |= DSI_CTRL_CMD_FIFO_STORE;
		} else if (msg->tx_len > DSI_EMBEDDED_MODE_DMA_MAX_SIZE_BYTES) {
			flags |= DSI_CTRL_CMD_NON_EMBEDDED_MODE;
		}

		/* Set flags needed for broadcast. Read commands are always unicast */
		if (!(msg->flags & MIPI_DSI_MSG_UNICAST_COMMAND) && (display->ctrl_count > 1))
			flags |= DSI_CTRL_CMD_BROADCAST | DSI_CTRL_CMD_DEFER_TRIGGER;

		/*
		 * Set flags for command scheduling.
		 * 1) In video mode command DMA scheduling is default.
		 * 2) In command mode command DMA scheduling depends on message
		 * flag and TE needs to be running.
		 */
		if (display->panel->panel_mode == DSI_OP_VIDEO_MODE) {
			flags |= DSI_CTRL_CMD_CUSTOM_DMA_SCHED;
		} else {
			if (msg->flags & MIPI_DSI_MSG_CMD_DMA_SCHED)
				flags |= DSI_CTRL_CMD_CUSTOM_DMA_SCHED;
			if (!display->enabled)
				flags &= ~DSI_CTRL_CMD_CUSTOM_DMA_SCHED;
		}

		/* Set flags for last command */
		if (!(msg->flags & MIPI_DSI_MSG_BATCH_COMMAND) || (flags & DSI_CTRL_CMD_FIFO_STORE)
				|| (flags & DSI_CTRL_CMD_NON_EMBEDDED_MODE))
			flags |= DSI_CTRL_CMD_LAST_COMMAND;

		/*
		 * Set flags for asynchronous wait.
		 * Asynchronous wait is supported in the following scenarios
		 * 1) queue_cmd_waits is set by connector and
		 *	- commands are not sent using DSI FIFO memory
		 *	- commands are not sent in non-embedded mode
		 *	- no explicit msg post_wait_ms is specified
		 *	- not a read command
		 * 2) if async override msg flag is present
		 */
		if (display->queue_cmd_waits)
			if (!(flags & DSI_CTRL_CMD_FIFO_STORE) &&
					!(flags & DSI_CTRL_CMD_NON_EMBEDDED_MODE) &&
					(cmd->post_wait_ms == 0) &&
					!(cmd->ctrl_flags & DSI_CTRL_CMD_READ))
				flags |= DSI_CTRL_CMD_ASYNC_WAIT;
		if (msg->flags & MIPI_DSI_MSG_ASYNC_OVERRIDE)
				flags |= DSI_CTRL_CMD_ASYNC_WAIT;
	}

	cmd->ctrl_flags |= flags;
}

static int dsi_display_read_status(struct dsi_display_ctrl *ctrl,
		struct dsi_display *display)
{
	int i, rc = 0, count = 0, start = 0, *lenp;
	struct drm_panel_esd_config *config;
	struct dsi_cmd_desc *cmds;
	struct dsi_panel *panel;
	u32 flags = 0;

	if (!display->panel || !ctrl || !ctrl->ctrl)
		return -EINVAL;

	panel = display->panel;

	/*
	 * When DSI controller is not in initialized state, we do not want to
	 * report a false ESD failure and hence we defer until next read
	 * happen.
	 */
	if (!dsi_ctrl_validate_host_state(ctrl->ctrl))
		return 1;

	config = &(panel->esd_config);
	lenp = config->status_valid_params ?: config->status_cmds_rlen;
	count = config->status_cmd.count;
	cmds = config->status_cmd.cmds;
	flags = DSI_CTRL_CMD_READ;

	for (i = 0; i < count; ++i) {
		memset(config->status_buf, 0x0, SZ_4K);

		if (config->status_cmd.state == DSI_CMD_SET_STATE_LP)
			cmds[i].msg.flags |= MIPI_DSI_MSG_USE_LPM;

		cmds[i].msg.flags |= MIPI_DSI_MSG_UNICAST_COMMAND;
		cmds[i].msg.rx_buf = config->status_buf;
		cmds[i].msg.rx_len = config->status_cmds_rlen[i];
		cmds[i].ctrl_flags = flags;
		dsi_display_set_cmd_tx_ctrl_flags(display,&cmds[i]);

		if (mi_get_panel_id(display->panel->mi_cfg.mi_panel_id) == L3_PANEL_PA) {
			if (*((u8 *)cmds[i].msg.tx_buf) == 0xBA) {
				mi_dsi_panel_switch_page_locked(panel, 2);
				DSI_DEBUG("[esd debug]:switch to page 2\n");
			}
		}

		rc = dsi_ctrl_transfer_prepare(ctrl->ctrl, cmds[i].ctrl_flags);
		if (rc) {
			DSI_ERR("prepare for rx cmd transfer failed rc=%d\n", rc);
			return rc;
		}

		rc = dsi_ctrl_cmd_transfer(ctrl->ctrl, &cmds[i]);
		if (rc <= 0) {
			DSI_ERR("rx cmd transfer failed rc=%d\n", rc);
		} else {
			memcpy(config->return_buf + start,
				config->status_buf, lenp[i]);
			start += lenp[i];
		}

		dsi_ctrl_transfer_unprepare(ctrl->ctrl, cmds[i].ctrl_flags);

		if (mi_get_panel_id(display->panel->mi_cfg.mi_panel_id) == L3_PANEL_PA) {
			if (*((u8 *)cmds[i].msg.tx_buf) == 0xBA) {
				mi_dsi_panel_switch_page_locked(panel, 0);
				DSI_DEBUG("[esd debug]:switch to page 0\n");
			}
		}
	}

	return rc;
}

static int dsi_display_validate_status(struct dsi_display_ctrl *ctrl,
		struct dsi_display *display)
{
	int rc = 0;

	rc = dsi_display_read_status(ctrl, display);
	if (rc <= 0) {
		goto exit;
	} else {
		/*
		 * panel status read successfully.
		 * check for validity of the data read back.
		 */
		rc = dsi_display_validate_reg_read(display->panel);
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

	DSI_DEBUG(" ++\n");

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	if (display->tx_cmd_buf == NULL) {
		rc = dsi_host_alloc_cmd_tx_buffer(display);
		if (rc) {
			DSI_ERR("failed to allocate cmd tx buffer memory\n");
			goto done;
		}
	}

	rc = dsi_display_validate_status(m_ctrl, display);
	if (rc <= 0) {
		DSI_ERR("[%s] read status failed on master,rc=%d\n",
		       display->name, rc);
		goto done;
	}

	if (!display->panel->sync_broadcast_en)
		goto done;

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (ctrl == m_ctrl)
			continue;

		rc = dsi_display_validate_status(ctrl, display);
		if (rc <= 0) {
			DSI_ERR("[%s] read status failed on slave,rc=%d\n",
			       display->name, rc);
			goto done;
		}
	}

done:
	return rc;
}

static int dsi_display_status_bta_request(struct dsi_display *display)
{
	int rc = 0;

	DSI_DEBUG(" ++\n");
	/* TODO: trigger SW BTA and wait for acknowledgment */

	return rc;
}

static void dsi_display_release_te_irq(struct dsi_display *display)
{
	int te_irq = 0;

	te_irq = gpio_to_irq(display->disp_te_gpio);
	if (te_irq)
		free_irq(te_irq, display);
}

static int dsi_display_status_check_te(struct dsi_display *display,
		int rechecks)
{
	int rc = 1, i = 0;
	int const esd_te_timeout = msecs_to_jiffies(15*20);

	if (!rechecks)
		return rc;

	/* register te irq handler */
	dsi_display_register_te_irq(display);

	dsi_display_change_te_irq_status(display, true);

	for (i = 0; i < rechecks; i++) {
		reinit_completion(&display->esd_te_gate);
		if (!wait_for_completion_timeout(&display->esd_te_gate,
					esd_te_timeout)) {
			DSI_ERR("TE check failed\n");
			dsi_display_change_te_irq_status(display, false);
			return -EINVAL;
		}
	}

	dsi_display_change_te_irq_status(display, false);
	dsi_display_release_te_irq(display);

	return rc;
}

int dsi_display_check_status(struct drm_connector *connector, void *display,
					bool te_check_override)
{
	struct dsi_display *dsi_display = display;
	struct drm_panel_esd_config *esd;
	struct dsi_panel *panel;
	u32 status_mode;
	int rc = 0x1;
	int te_rechecks = 1;

	if (!dsi_display || !dsi_display->panel)
		return -EINVAL;

	panel = dsi_display->panel;

	dsi_panel_acquire_panel_lock(panel);

	if (!panel->panel_initialized) {
		DSI_DEBUG("Panel not initialized\n");
		goto release_panel_lock;
	}

	/* Prevent another ESD check,when ESD recovery is underway */
	if (atomic_read(&panel->esd_recovery_pending))
		goto release_panel_lock;

	status_mode = panel->esd_config.status_mode;

	if ((status_mode == ESD_MODE_SW_SIM_SUCCESS) || is_sim_panel(display))
		goto release_panel_lock;

	if (status_mode == ESD_MODE_SW_SIM_FAILURE) {
		rc = -EINVAL;
		goto release_panel_lock;
	}
	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY, status_mode, te_check_override);

	if (te_check_override)
		te_rechecks = MAX_TE_RECHECKS;

	if ((dsi_display->trusted_vm_env) ||
			(panel->panel_mode == DSI_OP_VIDEO_MODE))
		te_rechecks = 0;

	if (status_mode == ESD_MODE_REG_READ) {
		esd = &(panel->esd_config);
		if (esd->offset_cmd.count != 0) {
			rc = mi_dsi_panel_write_cmd_set(dsi_display->panel, &esd->offset_cmd);
			DSI_DEBUG("wirte esd reg offset command rc = %d\n", rc);
		}
	}
	dsi_display_set_ctrl_esd_check_flag(dsi_display, true);

	if (status_mode == ESD_MODE_REG_READ) {
		rc = dsi_display_status_reg_read(dsi_display);
	} else if (status_mode == ESD_MODE_SW_BTA) {
		rc = dsi_display_status_bta_request(dsi_display);
	} else if (status_mode == ESD_MODE_PANEL_TE) {
		rc = dsi_display_status_check_te(dsi_display, te_rechecks);
		te_check_override = false;
	} else {
		DSI_WARN("Unsupported check status mode: %d\n", status_mode);
		panel->esd_config.esd_enabled = false;
	}

	if (rc <= 0 && te_check_override)
		rc = dsi_display_status_check_te(dsi_display, te_rechecks);
	if (rc > 0) {
		dsi_display_set_ctrl_esd_check_flag(dsi_display, false);
		if (te_check_override && panel->esd_config.esd_enabled == false)
			rc = dsi_display_status_check_te(dsi_display,
					te_rechecks);
	}

	/* Handle Panel failures during display disable sequence */
	if (rc <=0)
		atomic_set(&panel->esd_recovery_pending, 1);

release_panel_lock:
	dsi_panel_release_panel_lock(panel);
	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT, rc);

	return rc;
}

int dsi_display_ctrl_get_host_init_state(struct dsi_display *dsi_display,
		bool *state)
{
	struct dsi_display_ctrl *ctrl;
	int i, rc = -EINVAL;
	bool final_state = true;

	display_for_each_ctrl(i, dsi_display) {
		bool ctrl_state = false;
		ctrl = &dsi_display->ctrl[i];
		rc = dsi_ctrl_get_host_engine_init_state(ctrl->ctrl, &ctrl_state);
		final_state &= ctrl_state;
		if ((rc) || !(final_state))
			break;
	}
	*state = final_state;
	return rc;
}

static int dsi_display_cmd_rx(struct dsi_display *display,
			      struct dsi_cmd_desc *cmd)
{
	struct dsi_display_ctrl *m_ctrl = NULL;
	u32 flags = 0;
	int rc = 0;

	if (!display || !display->panel)
		return -EINVAL;

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	if (!m_ctrl || !m_ctrl->ctrl)
		return -EINVAL;

	/* acquire panel_lock to make sure no commands are in progress */
	dsi_panel_acquire_panel_lock(display->panel);
	if (!display->panel->panel_initialized) {
		DSI_DEBUG("panel not initialized\n");
		goto release_panel_lock;
	}

	flags = DSI_CTRL_CMD_READ;

	cmd->ctrl_flags = flags;
	dsi_display_set_cmd_tx_ctrl_flags(display, cmd);
	rc = dsi_ctrl_transfer_prepare(m_ctrl->ctrl, cmd->ctrl_flags);
	if (rc) {
		DSI_ERR("prepare for rx cmd transfer failed rc = %d\n", rc);
		goto release_panel_lock;
	}
	rc = dsi_ctrl_cmd_transfer(m_ctrl->ctrl, cmd);
	if (rc <= 0)
		DSI_ERR("rx cmd transfer failed rc = %d\n", rc);
	dsi_ctrl_transfer_unprepare(m_ctrl->ctrl, cmd->ctrl_flags);

release_panel_lock:
	dsi_panel_release_panel_lock(display->panel);
	return rc;
}


int dsi_display_cmd_transfer(struct drm_connector *connector,
		void *display, const char *cmd_buf,
		u32 cmd_buf_len)
{
	struct dsi_display *dsi_display = display;
	int rc = 0, cnt = 0, i = 0;
	bool state = false, transfer = false;
	struct dsi_panel_cmd_set *set;

	if (!dsi_display || !cmd_buf) {
		DSI_ERR("[DSI] invalid params\n");
		return -EINVAL;
	}

	DSI_DEBUG("[DSI] Display command transfer\n");

	if (!(cmd_buf[3] & MIPI_DSI_MSG_BATCH_COMMAND))
		transfer = true;

	mutex_lock(&dsi_display->display_lock);
	rc = dsi_display_ctrl_get_host_init_state(dsi_display, &state);

	/**
	 * Handle scenario where a command transfer is initiated through
	 * sysfs interface when device is in suepnd state.
	 */
	if (!rc && !state) {
		pr_warn_ratelimited("Command xfer attempted while device is in suspend state\n"
				);
		rc = -EPERM;
		goto end;
	}
	if (rc || !state) {
		DSI_ERR("[DSI] Invalid host state %d rc %d\n",
				state, rc);
		rc = -EPERM;
		goto end;
	}

	SDE_EVT32(dsi_display->tx_cmd_buf_ndx, cmd_buf_len);

	/*
	 * Reset the dbgfs buffer if the commands sent exceed the available
	 * buffer size. For video mode, limiting the buffer size to 2K to
	 * ensure no performance issues.
	 */
	if (dsi_display->panel->panel_mode == DSI_OP_CMD_MODE) {
		if ((dsi_display->tx_cmd_buf_ndx + cmd_buf_len) > SZ_4K) {
			memset(dbgfs_tx_cmd_buf, 0, SZ_4K);
			dsi_display->tx_cmd_buf_ndx = 0;
		}
	} else {
		if ((dsi_display->tx_cmd_buf_ndx + cmd_buf_len) > SZ_2K) {
			memset(dbgfs_tx_cmd_buf, 0, SZ_4K);
			dsi_display->tx_cmd_buf_ndx = 0;
		}
	}

	memcpy(&dbgfs_tx_cmd_buf[dsi_display->tx_cmd_buf_ndx], cmd_buf,
			cmd_buf_len);
	dsi_display->tx_cmd_buf_ndx += cmd_buf_len;
	if (transfer) {
		struct dsi_cmd_desc *cmds;

		set = &dsi_display->cmd_set;
		set->count = 0;
		dsi_panel_get_cmd_pkt_count(dbgfs_tx_cmd_buf,
				dsi_display->tx_cmd_buf_ndx, &cnt);
		dsi_panel_alloc_cmd_packets(set, cnt);
		dsi_panel_create_cmd_packets(dbgfs_tx_cmd_buf,
				dsi_display->tx_cmd_buf_ndx, cnt, set->cmds);
		cmds = set->cmds;
		dsi_display->tx_cmd_buf_ndx = 0;

		dsi_panel_acquire_panel_lock(dsi_display->panel);
		for (i = 0; i < cnt; i++) {
			rc = dsi_host_transfer_sub(&dsi_display->host, cmds);
			if (rc < 0) {
				DSI_ERR("failed to send command, rc=%d\n", rc);
				break;
			}
			if (cmds->post_wait_ms)
				usleep_range(cmds->post_wait_ms*1000,
						((cmds->post_wait_ms*1000)+10));
			cmds++;
		}
		dsi_panel_release_panel_lock(dsi_display->panel);

		memset(dbgfs_tx_cmd_buf, 0, SZ_4K);
		dsi_panel_destroy_cmd_packets(set);
		dsi_panel_dealloc_cmd_packets(set);
	}

end:
	mutex_unlock(&dsi_display->display_lock);
	return rc;
}

static void _dsi_display_continuous_clk_ctrl(struct dsi_display *display,
					     bool enable)
{
	int i;
	struct dsi_display_ctrl *ctrl;

	if (!display || !display->panel->host_config.force_hs_clk_lane)
		return;

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];

		/*
		 * For phy ver 4.0 chipsets, configure DSI controller and
		 * DSI PHY to force clk lane to HS mode always whereas
		 * for other phy ver chipsets, configure DSI controller only.
		 */
		if (ctrl->phy->hw.ops.set_continuous_clk) {
			dsi_ctrl_hs_req_sel(ctrl->ctrl, true);
			dsi_ctrl_set_continuous_clk(ctrl->ctrl, enable);
			dsi_phy_set_continuous_clk(ctrl->phy, enable);
		} else {
			dsi_ctrl_set_continuous_clk(ctrl->ctrl, enable);
		}
	}
}

int dsi_display_cmd_receive(void *display, const char *cmd_buf,
		u32 cmd_buf_len,  u8 *recv_buf, u32 recv_buf_len)
{
	struct dsi_display *dsi_display = display;
	struct dsi_cmd_desc cmd = {};
	bool state = false;
	int rc = -1;

	if (!dsi_display || !cmd_buf || !recv_buf) {
		DSI_ERR("[DSI] invalid params\n");
		return -EINVAL;
	}

	rc = dsi_panel_create_cmd_packets(cmd_buf, cmd_buf_len, 1, &cmd);
	if (rc) {
		DSI_ERR("[DSI] command packet create failed, rc = %d\n", rc);
		return rc;
	}

	cmd.msg.rx_buf = recv_buf;
	cmd.msg.rx_len = recv_buf_len;
	cmd.msg.flags |= MIPI_DSI_MSG_UNICAST_COMMAND;

	mutex_lock(&dsi_display->display_lock);

	if (is_sim_panel(display)) {
		DSI_DEBUG("Simulation panel doesn't support read commands\n");
		goto end;
	}

	rc = dsi_display_ctrl_get_host_init_state(dsi_display, &state);

	/**
	 * Handle scenario where a command transfer is initiated through
	 * sysfs interface when device is in suspend state.
	 */
	if (!rc && !state) {
		pr_warn_ratelimited("Command xfer attempted while device is in suspend state\n");
		rc = -EPERM;
		goto end;
	}
	if (rc || !state) {
		DSI_ERR("[DSI] Invalid host state = %d rc = %d\n",
			state, rc);
		rc = -EPERM;
		goto end;
	}

	SDE_EVT32(cmd_buf_len, recv_buf_len);

	rc = dsi_display_cmd_rx(dsi_display, &cmd);
	if (rc <= 0)
		DSI_ERR("[DSI] Display command receive failed, rc=%d\n", rc);

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

	display_for_each_ctrl(i, dsi_display) {
		ctrl = &dsi_display->ctrl[i];
		rc = dsi_ctrl_soft_reset(ctrl->ctrl);
		if (rc) {
			DSI_ERR("[%s] failed to soft reset host_%d, rc=%d\n",
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
		DSI_ERR("Invalid params(s) dsi_display %pK, panel %pK\n",
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

	display_for_each_ctrl(i, display) {
		dsi_ctrl_setup_misr(display->ctrl[i].ctrl,
				display->misr_enable,
				display->misr_frame_count);
	}
}

int dsi_display_set_power(struct drm_connector *connector,
		int power_mode, void *disp)
{
	struct dsi_display *display = disp;
	int rc = 0;

	if (!display || !display->panel) {
		DSI_ERR("invalid display/panel\n");
		return -EINVAL;
	}

	mutex_lock(&display->panel->mi_cfg.doze_lock);

	DISP_UTC_INFO("Display (%s), Power mode (%s)\n", display->display_type,
			get_display_power_mode_name(power_mode));

	switch (power_mode) {
	case SDE_MODE_DPMS_LP1:
		display->panel->power_mode = power_mode;
		rc = dsi_panel_set_lp1(display->panel);
		break;
	case SDE_MODE_DPMS_LP2:
		display->panel->power_mode = power_mode;
		rc = dsi_panel_set_lp2(display->panel);
		break;
	case SDE_MODE_DPMS_ON:
		if ((display->panel->power_mode == SDE_MODE_DPMS_LP1) ||
			(display->panel->power_mode == SDE_MODE_DPMS_LP2))
			rc = dsi_panel_set_nolp(display->panel);
		break;
	case SDE_MODE_DPMS_OFF:
		if (mi_disp_lhbm_fod_enabled(display->panel))
			mi_disp_lhbm_fod_allow_tx_lhbm(display, false);
		mi_disp_feature_event_notify_by_type(mi_get_disp_id(display->display_type),
			MI_DISP_EVENT_POWER, sizeof(power_mode), power_mode);
	default:
		mutex_unlock(&display->panel->mi_cfg.doze_lock);
		return rc;
	}

	SDE_EVT32(display->panel->power_mode, power_mode, rc);
	DSI_DEBUG("Power mode transition from %d to %d %s",
			display->panel->power_mode, power_mode,
			rc ? "failed" : "successful");
	if (!rc) {
		display->panel->power_mode = power_mode;

		mi_disp_feature_event_notify_by_type(mi_get_disp_id(display->display_type),
			MI_DISP_EVENT_POWER, sizeof(power_mode), power_mode);
	}

	mutex_unlock(&display->panel->mi_cfg.doze_lock);
	return rc;
}

#ifdef CONFIG_DEBUG_FS
static bool dsi_display_is_te_based_esd(struct dsi_display *display)
{
	u32 status_mode = 0;

	if (!display->panel) {
		DSI_ERR("Invalid panel data\n");
		return false;
	}

	status_mode = display->panel->esd_config.status_mode;

	if (status_mode == ESD_MODE_PANEL_TE &&
			gpio_is_valid(display->disp_te_gpio))
		return true;
	return false;
}

static ssize_t debugfs_dump_info_read(struct file *file,
				      char __user *user_buf,
				      size_t user_len,
				      loff_t *ppos)
{
	struct dsi_display *display = file->private_data;
	struct dsi_mode_info *m;
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

	m = &display->config.video_timing;

	len += snprintf(buf + len, (SZ_4K - len), "name = %s\n", display->name);
	len += snprintf(buf + len, (SZ_4K - len),
			"\tResolution = %d(%d|%d|%d|%d)x%d(%d|%d|%d|%d)@%dfps %llu Hz\n",
			m->h_active, m->h_back_porch, m->h_front_porch, m->h_sync_width,
			m->h_sync_polarity, m->v_active, m->v_back_porch, m->v_front_porch,
			m->v_sync_width, m->v_sync_polarity, m->refresh_rate, m->clk_rate_hz);

	display_for_each_ctrl(i, display) {
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

	if (len > user_len)
		len = user_len;

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

	if (!display->hw_ownership) {
		DSI_DEBUG("[%s] op not supported due to HW unavailability\n",
				display->name);
		rc = -EOPNOTSUPP;
		goto unlock;
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	if (rc) {
		DSI_ERR("[%s] failed to enable DSI core clocks, rc=%d\n",
		       display->name, rc);
		goto unlock;
	}

	_dsi_display_setup_misr(display);

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);
	if (rc) {
		DSI_ERR("[%s] failed to disable DSI core clocks, rc=%d\n",
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
	if (ZERO_OR_NULL_PTR(buf))
		return -ENOMEM;

	mutex_lock(&display->display_lock);

	if (!display->hw_ownership) {
		DSI_DEBUG("[%s] op not supported due to HW unavailability\n",
				display->name);
		rc = -EOPNOTSUPP;
		goto error;
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	if (rc) {
		DSI_ERR("[%s] failed to enable DSI core clocks, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	display_for_each_ctrl(i, display) {
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
		DSI_ERR("[%s] failed to disable DSI core clocks, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	if (copy_to_user(user_buf, buf, max_len)) {
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
	struct drm_panel_esd_config *esd_config = &display->panel->esd_config;
	u32 esd_trigger;
	size_t len;

	if (!display)
		return -ENODEV;

	if (*ppos)
		return 0;

	if (user_len > sizeof(u32))
		return -EINVAL;

	if (!user_len || !user_buf)
		return -EINVAL;

	if (!display->panel ||
		atomic_read(&display->panel->esd_recovery_pending))
		return user_len;

	if (!esd_config->esd_enabled) {
		DSI_ERR("ESD feature is not enabled\n");
		return -EINVAL;
	}

	buf = kzalloc(ESD_TRIGGER_STRING_MAX_LEN, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len = min_t(size_t, user_len, ESD_TRIGGER_STRING_MAX_LEN - 1);
	if (copy_from_user(buf, user_buf, len)) {
		rc = -EINVAL;
		goto error;
	}

	buf[len] = '\0'; /* terminate the string */

	if (kstrtouint(buf, 10, &esd_trigger)) {
		rc = -EINVAL;
		goto error;
	}

	if (esd_trigger != 1) {
		rc = -EINVAL;
		goto error;
	}

	display->esd_trigger = esd_trigger;

	mutex_lock(&display->display_lock);

	if (!display->hw_ownership) {
		DSI_DEBUG("[%s] op not supported due to HW unavailability\n",
				display->name);
		rc = -EOPNOTSUPP;
		goto unlock;
	}

	if (display->esd_trigger) {
		struct dsi_panel *panel = display->panel;

		DSI_INFO("ESD attack triggered by user\n");
		rc = panel->panel_ops.trigger_esd_attack(panel);
		if (rc) {
			DSI_ERR("Failed to trigger ESD attack\n");
			goto error;
		}
	}

	rc = len;
unlock:
	mutex_unlock(&display->display_lock);
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
	size_t len;

	if (!display)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(ESD_MODE_STRING_MAX_LEN, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf))
		return -ENOMEM;

	len = min_t(size_t, user_len, ESD_MODE_STRING_MAX_LEN - 1);
	if (copy_from_user(buf, user_buf, len)) {
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
		DSI_ERR("Invalid panel esd config\n");
		rc = -EINVAL;
		goto error;
	}

	if (!esd_config->esd_enabled) {
		rc = -EINVAL;
		goto error;
	}

	if (!strcmp(buf, "te_signal_check\n")) {
		DSI_INFO("TE based ESD check for panels is not allowed\n");
		rc = -EINVAL;
		goto error;
	}

	if (!strcmp(buf, "reg_read\n")) {
		DSI_INFO("ESD check is switched to reg read by user\n");
		rc = dsi_panel_parse_esd_reg_read_configs(display->panel);
		if (rc) {
			DSI_ERR("failed to alter esd check mode,rc=%d\n",
						rc);
			rc = user_len;
			goto error;
		}
		esd_config->status_mode = ESD_MODE_REG_READ;
		if (dsi_display_is_te_based_esd(display))
			dsi_display_change_te_irq_status(display, false);
	}

	if (!strcmp(buf, "esd_sw_sim_success\n"))
		esd_config->status_mode = ESD_MODE_SW_SIM_SUCCESS;

	if (!strcmp(buf, "esd_sw_sim_failure\n"))
		esd_config->status_mode = ESD_MODE_SW_SIM_FAILURE;

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
	size_t len = 0;

	if (!display)
		return -ENODEV;

	if (*ppos)
		return 0;

	if (!display->panel) {
		DSI_ERR("invalid panel data\n");
		return -EINVAL;
	}

	buf = kzalloc(ESD_MODE_STRING_MAX_LEN, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf))
		return -ENOMEM;

	esd_config = &display->panel->esd_config;
	if (!esd_config) {
		DSI_ERR("Invalid panel esd config\n");
		rc = -EINVAL;
		goto error;
	}

	len = min_t(size_t, user_len, ESD_MODE_STRING_MAX_LEN - 1);
	if (!esd_config->esd_enabled) {
		rc = snprintf(buf, len, "ESD feature not enabled");
		goto output_mode;
	}

	switch (esd_config->status_mode) {
	case ESD_MODE_REG_READ:
		rc = snprintf(buf, len, "reg_read");
		break;
	case ESD_MODE_PANEL_TE:
		rc = snprintf(buf, len, "te_signal_check");
		break;
	case ESD_MODE_SW_SIM_FAILURE:
		rc = snprintf(buf, len, "esd_sw_sim_failure");
		break;
	case ESD_MODE_SW_SIM_SUCCESS:
		rc = snprintf(buf, len, "esd_sw_sim_success");
		break;
	default:
		rc = snprintf(buf, len, "invalid");
		break;
	}

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

static ssize_t debugfs_update_cmd_scheduling_params(struct file *file,
				  const char __user *user_buf,
				  size_t user_len,
				  loff_t *ppos)
{
	struct dsi_display *display = file->private_data;
	struct dsi_display_ctrl *display_ctrl;
	char *buf;
	int rc = 0;
	u32 line = 0, window = 0;
	size_t len;
	int i;

	if (!display)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(256, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf))
		return -ENOMEM;

	len = min_t(size_t, user_len, 255);
	if (copy_from_user(buf, user_buf, len)) {
		rc = -EINVAL;
		goto error;
	}

	buf[len] = '\0'; /* terminate the string */

	if (sscanf(buf, "%d %d", &line, &window) != 2)
		return -EFAULT;

	display_for_each_ctrl(i, display) {
		struct dsi_ctrl *ctrl;

		display_ctrl = &display->ctrl[i];
		if (!display_ctrl->ctrl)
			continue;

		ctrl = display_ctrl->ctrl;
		ctrl->host_config.common_config.dma_sched_line = line;
		ctrl->host_config.common_config.dma_sched_window = window;
	}

	rc = len;

error:
	kfree(buf);
	return rc;
}

static ssize_t debugfs_read_cmd_scheduling_params(struct file *file,
				 char __user *user_buf,
				 size_t user_len,
				 loff_t *ppos)
{
	struct dsi_display *display = file->private_data;
	struct dsi_display_ctrl *m_ctrl;
	struct dsi_ctrl *ctrl;
	char *buf;
	u32 len = 0;
	int rc = 0;
	size_t max_len = min_t(size_t, user_len, SZ_4K);

	if (!display)
		return -ENODEV;

	if (*ppos)
		return 0;

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	ctrl = m_ctrl->ctrl;

	buf = kzalloc(max_len, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(buf))
		return -ENOMEM;

	len += scnprintf(buf, max_len, "Schedule command window start: %d\n",
			ctrl->host_config.common_config.dma_sched_line);
	len += scnprintf((buf + len), max_len - len,
			"Schedule command window width: %d\n",
			ctrl->host_config.common_config.dma_sched_window);

	if (len > max_len)
		len = max_len;

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

static const struct file_operations dsi_command_scheduling_fops = {
	.open = simple_open,
	.write = debugfs_update_cmd_scheduling_params,
	.read = debugfs_read_cmd_scheduling_params,
};

static int dsi_display_debugfs_init(struct dsi_display *display)
{
	int rc = 0;
	struct dentry *dir, *dump_file, *misr_data;
	char name[MAX_NAME_SIZE];
	char panel_name[SEC_PANEL_NAME_MAX_LEN];
	char secondary_panel_str[] = "_secondary";
	int i;

	strlcpy(panel_name, display->name, SEC_PANEL_NAME_MAX_LEN);
	if (strcmp(display->display_type, "secondary") == 0)
		strlcat(panel_name, secondary_panel_str, SEC_PANEL_NAME_MAX_LEN);

	dir = debugfs_create_dir(panel_name, NULL);
	if (IS_ERR_OR_NULL(dir)) {
		rc = PTR_ERR(dir);
		DSI_ERR("[%s] debugfs create dir failed, rc = %d\n",
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
		DSI_ERR("[%s] debugfs create dump info file failed, rc=%d\n",
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
		DSI_ERR("[%s] debugfs for esd trigger file failed, rc=%d\n",
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
		DSI_ERR("[%s] debugfs for esd check mode failed, rc=%d\n",
		       display->name, rc);
		goto error_remove_dir;
	}

	dump_file = debugfs_create_file("cmd_sched_params",
					0644,
					dir,
					display,
					&dsi_command_scheduling_fops);
	if (IS_ERR_OR_NULL(dump_file)) {
		rc = PTR_ERR(dump_file);
		DSI_ERR("[%s] debugfs for cmd scheduling file failed, rc=%d\n",
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
		DSI_ERR("[%s] debugfs create misr datafile failed, rc=%d\n",
		       display->name, rc);
		goto error_remove_dir;
	}

	display_for_each_ctrl(i, display) {
		struct msm_dsi_phy *phy = display->ctrl[i].phy;

		if (!phy || !phy->name)
			continue;

		snprintf(name, ARRAY_SIZE(name),
				"%s_allow_phy_power_off", phy->name);
		dump_file = debugfs_create_bool(name, 0600, dir,
				&phy->allow_phy_power_off);
		if (IS_ERR_OR_NULL(dump_file)) {
			rc = PTR_ERR(dump_file);
			DSI_ERR("[%s] debugfs create %s failed, rc=%d\n",
			       display->name, name, rc);
			goto error_remove_dir;
		}

		snprintf(name, ARRAY_SIZE(name),
				"%s_regulator_min_datarate_bps", phy->name);
		debugfs_create_u32(name, 0600, dir, &phy->regulator_min_datarate_bps);
	}

	if (!debugfs_create_bool("ulps_feature_enable", 0600, dir,
			&display->panel->ulps_feature_enabled)) {
		DSI_ERR("[%s] debugfs create ulps feature enable file failed\n",
		       display->name);
		goto error_remove_dir;
	}

	if (!debugfs_create_bool("ulps_suspend_feature_enable", 0600, dir,
			&display->panel->ulps_suspend_enabled)) {
		DSI_ERR("[%s] debugfs create ulps-suspend feature enable file failed\n",
		       display->name);
		goto error_remove_dir;
	}

	if (!debugfs_create_bool("ulps_status", 0400, dir,
			&display->ulps_enabled)) {
		DSI_ERR("[%s] debugfs create ulps status file failed\n",
		       display->name);
		goto error_remove_dir;
	}

	debugfs_create_u32("clk_gating_config", 0600, dir, &display->clk_gating_config);

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
	if (display->root) {
		debugfs_remove_recursive(display->root);
		display->root = NULL;
	}

	return 0;
}
#else
static int dsi_display_debugfs_init(struct dsi_display *display)
{
	return 0;
}
static int dsi_display_debugfs_deinit(struct dsi_display *display)
{
	return 0;
}
#endif /* CONFIG_DEBUG_FS */

static void adjust_timing_by_ctrl_count(const struct dsi_display *display,
					struct dsi_display_mode *mode)
{
	struct dsi_host_common_cfg *host = &display->panel->host_config;
	bool is_split_link = host->split_link.enabled;
	u32 sublinks_count = host->split_link.num_sublinks;

	if (is_split_link && sublinks_count > 1) {
		mode->timing.h_active /= sublinks_count;
		mode->timing.h_front_porch /= sublinks_count;
		mode->timing.h_sync_width /= sublinks_count;
		mode->timing.h_back_porch /= sublinks_count;
		mode->timing.h_skew /= sublinks_count;
		mode->pixel_clk_khz /= sublinks_count;
	} else {
		if (mode->priv_info->dsc_enabled)
			mode->priv_info->dsc.config.pic_width =
				mode->timing.h_active;
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

	DSI_DEBUG("checking ulps req validity\n");

	if (atomic_read(&display->panel->esd_recovery_pending)) {
		DSI_DEBUG("%s: ESD recovery sequence underway\n", __func__);
		return false;
	}

	if (!dsi_panel_ulps_feature_enabled(display->panel) &&
			!display->panel->ulps_suspend_enabled) {
		DSI_DEBUG("%s: ULPS feature is not enabled\n", __func__);
		return false;
	}

	if (!dsi_panel_initialized(display->panel) &&
			!display->panel->ulps_suspend_enabled) {
		DSI_DEBUG("%s: panel not yet initialized\n", __func__);
		return false;
	}

	if (enable && display->ulps_enabled) {
		DSI_DEBUG("ULPS already enabled\n");
		return false;
	} else if (!enable && !display->ulps_enabled) {
		DSI_DEBUG("ULPS already disabled\n");
		return false;
	}

	/*
	 * No need to enter ULPS when transitioning from splash screen to
	 * boot animation or trusted vm environments since it is expected
	 * that the clocks would be turned right back on.
	 */
	if (enable && is_skip_op_required(display))
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
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (!dsi_display_is_ulps_req_valid(display, enable)) {
		DSI_DEBUG("%s: skipping ULPS config, enable=%d\n",
			__func__, enable);
		return 0;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	/*
	 * ULPS entry-exit can be either through the DSI controller or
	 * the DSI PHY depending on hardware variation. For some chipsets,
	 * both controller version and phy version ulps entry-exit ops can
	 * be present. To handle such cases, send ulps request through PHY,
	 * if ulps request is handled in PHY, then no need to send request
	 * through controller.
	 */

	rc = dsi_phy_set_ulps(m_ctrl->phy, &display->config, enable,
			display->clamp_enabled);

	if (rc == DSI_PHY_ULPS_ERROR) {
		DSI_ERR("Ulps PHY state change(%d) failed\n", enable);
		return -EINVAL;
	}

	else if (rc == DSI_PHY_ULPS_HANDLED) {
		display_for_each_ctrl(i, display) {
			ctrl = &display->ctrl[i];
			if (!ctrl->ctrl || (ctrl == m_ctrl))
				continue;

			rc = dsi_phy_set_ulps(ctrl->phy, &display->config,
					enable, display->clamp_enabled);
			if (rc == DSI_PHY_ULPS_ERROR) {
				DSI_ERR("Ulps PHY state change(%d) failed\n",
						enable);
				return -EINVAL;
			}
		}
	}

	else if (rc == DSI_PHY_ULPS_NOT_HANDLED) {
		rc = dsi_ctrl_set_ulps(m_ctrl->ctrl, enable);
		if (rc) {
			DSI_ERR("Ulps controller state change(%d) failed\n",
					enable);
			return rc;
		}
		display_for_each_ctrl(i, display) {
			ctrl = &display->ctrl[i];
			if (!ctrl->ctrl || (ctrl == m_ctrl))
				continue;

			rc = dsi_ctrl_set_ulps(ctrl->ctrl, enable);
			if (rc) {
				DSI_ERR("Ulps controller state change(%d) failed\n",
						enable);
				return rc;
			}
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
		DSI_ERR("Invalid params\n");
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
		DSI_ERR("DSI ctrl clamp state change(%d) failed\n", enable);
		return rc;
	}

	rc = dsi_phy_set_clamp_state(m_ctrl->phy, enable);
	if (rc) {
		DSI_ERR("DSI phy clamp state change(%d) failed\n", enable);
		return rc;
	}

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_clamp_state(ctrl->ctrl, enable, ulps_enabled);
		if (rc) {
			DSI_ERR("DSI Clamp state change(%d) failed\n", enable);
			return rc;
		}

		rc = dsi_phy_set_clamp_state(ctrl->phy, enable);
		if (rc) {
			DSI_ERR("DSI phy clamp state change(%d) failed\n",
				enable);
			return rc;
		}

		DSI_DEBUG("Clamps %s for ctrl%d\n",
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
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	rc = dsi_ctrl_setup(m_ctrl->ctrl);
	if (rc) {
		DSI_ERR("DSI controller setup failed\n");
		return rc;
	}

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_setup(ctrl->ctrl);
		if (rc) {
			DSI_ERR("DSI controller setup failed\n");
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
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (mmss_clamp && !display->phy_idle_power_off) {
		dsi_display_phy_enable(display);
		return 0;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	rc = dsi_phy_idle_ctrl(m_ctrl->phy, true);
	if (rc) {
		DSI_ERR("DSI controller setup failed\n");
		return rc;
	}

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_phy_idle_ctrl(ctrl->phy, true);
		if (rc) {
			DSI_ERR("DSI controller setup failed\n");
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
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	display_for_each_ctrl(i, display) {
		struct msm_dsi_phy *phy = display->ctrl[i].phy;

		if (!phy)
			continue;

		if (!phy->allow_phy_power_off) {
			DSI_DEBUG("phy doesn't support this feature\n");
			return 0;
		}
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	rc = dsi_phy_idle_ctrl(m_ctrl->phy, false);
	if (rc) {
		DSI_ERR("[%s] failed to enable cmd engine, rc=%d\n",
		       display->name, rc);
		return rc;
	}

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_phy_idle_ctrl(ctrl->phy, false);
		if (rc) {
			DSI_ERR("DSI controller setup failed\n");
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
		DSI_ERR("invalid display\n");
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
			display_for_each_ctrl(i, display)
				display->ctrl[i].ctrl->recovery_cb =
							*event_info;
		}
		break;
	case SDE_CONN_EVENT_PANEL_ID:
		if (event_info)
			display_for_each_ctrl(i, display)
				display->ctrl[i].ctrl->panel_id_cb
				    = *event_info;
		dsi_display_panel_id_notification(display);
		break;
	default:
		/* nothing to do */
		DSI_DEBUG("[%s] unhandled event %d\n", display->name, event_idx);
		return;
	}

	if (enable) {
		display_for_each_ctrl(i, display)
			dsi_ctrl_enable_status_interrupt(
					display->ctrl[i].ctrl, irq_status_idx,
					event_info);
	} else {
		display_for_each_ctrl(i, display)
			dsi_ctrl_disable_status_interrupt(
					display->ctrl[i].ctrl, irq_status_idx);
	}
}

static int dsi_display_ctrl_power_on(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	/* Sequence does not matter for split dsi usecases */
	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;

		rc = dsi_ctrl_set_power_state(ctrl->ctrl,
					      DSI_CTRL_POWER_VREG_ON);
		if (rc) {
			DSI_ERR("[%s] Failed to set power state, rc=%d\n",
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
	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;

		rc = dsi_ctrl_set_power_state(ctrl->ctrl,
			DSI_CTRL_POWER_VREG_OFF);
		if (rc) {
			DSI_ERR("[%s] Failed to power off, rc=%d\n",
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
	unsigned long cmdline_topology = NO_OVERRIDE;
	unsigned long cmdline_timing = NO_OVERRIDE;

	if (display_type >= MAX_DSI_ACTIVE_DISPLAY) {
		DSI_ERR("display_type=%d not supported\n", display_type);
		goto end;
	}

	if (display_type == DSI_PRIMARY)
		boot_str = dsi_display_primary;
	else
		boot_str = dsi_display_secondary;

	sw_te = strnstr(boot_str, ":sim-swte", strlen(boot_str));
	if (sw_te)
		display->sw_te_using_wd = true;

	str = strnstr(boot_str, ":config", strlen(boot_str));
	if (str) {
		if (sscanf(str, ":config%lu", &cmdline_topology) != 1) {
			DSI_ERR("invalid config index override: %s\n",
				boot_str);
			goto end;
		}
	}

	str = strnstr(boot_str, ":timing", strlen(boot_str));
	if (str) {
		if (sscanf(str, ":timing%lu", &cmdline_timing) != 1) {
			DSI_ERR("invalid timing index override: %s\n",
				boot_str);
			cmdline_topology = NO_OVERRIDE;
			goto end;
		}
	}
	DSI_DEBUG("successfully parsed command line topology and timing\n");
end:
	display->cmdline_topology = cmdline_topology;
	display->cmdline_timing = cmdline_timing;
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

		pos = strnstr(disp_buf, ":", strlen(disp_buf));

		/* Use ':' as a delimiter to retrieve the display name */
		if (!pos) {
			DSI_DEBUG("display name[%s]is not valid\n", disp_buf);
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
	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;

		rc = dsi_phy_set_power_state(ctrl->phy, true);
		if (rc) {
			DSI_ERR("[%s] Failed to set power state, rc=%d\n",
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
	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->phy)
			continue;

		rc = dsi_phy_set_power_state(ctrl->phy, false);
		if (rc) {
			DSI_ERR("[%s] Failed to power off, rc=%d\n",
			       ctrl->ctrl->name, rc);
			goto error;
		}
	}
error:
	return rc;
}

static int dsi_display_set_clk_src(struct dsi_display *display, bool set_xo)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	struct dsi_ctrl_clk_info *info;

	if (display->trusted_vm_env)
		return 0;
	/*
	 * In case of split DSI usecases, the clock for master controller should
	 * be enabled before the other controller. Master controller in the
	 * clock context refers to the controller that sources the clock. While turning off the
	 * clocks, the source is set to xo.
	 */
	m_ctrl = &display->ctrl[display->clk_master_idx];
	info = &m_ctrl->ctrl->clk_info;

	if (!set_xo)
		rc = dsi_ctrl_set_clock_source(m_ctrl->ctrl, &display->clock_info.pll_clks);
	else if ((info->xo_clk.byte_clk) && (info->xo_clk.pixel_clk))
		rc = dsi_ctrl_set_clock_source(m_ctrl->ctrl, &info->xo_clk);
	if (rc) {
		DSI_ERR("[%s] failed to set source clocks for master, rc=%d\n", display->name, rc);
		return rc;
	}

	/* Set source for the rest of the controllers */
	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		info = &ctrl->ctrl->clk_info;
		if (!set_xo)
			rc = dsi_ctrl_set_clock_source(ctrl->ctrl, &display->clock_info.pll_clks);
		else if ((info->xo_clk.byte_clk) && (info->xo_clk.pixel_clk))
			rc = dsi_ctrl_set_clock_source(ctrl->ctrl, &info->xo_clk);
		if (rc) {
			DSI_ERR("[%s] failed to set source clocks, rc=%d\n", display->name, rc);
			return rc;
		}
	}
	return 0;
}

int dsi_display_phy_pll_toggle(void *priv, bool prepare)
{
	int rc = 0;
	struct dsi_display *display = priv;
	struct dsi_display_ctrl *m_ctrl;

	if (!display) {
		DSI_ERR("invalid arguments\n");
		return -EINVAL;
	}

	if (is_skip_op_required(display))
		return 0;

	rc = dsi_display_set_clk_src(display, !prepare);

	m_ctrl = &display->ctrl[display->clk_master_idx];
	if (!m_ctrl->phy) {
		DSI_ERR("[%s] PHY not found\n", display->name);
		return -EINVAL;
	}

	rc = dsi_phy_pll_toggle(m_ctrl->phy, prepare);

	return rc;
}

int dsi_display_phy_configure(void *priv, bool commit)
{
	int rc = 0;
	struct dsi_display *display = priv;
	struct dsi_display_ctrl *m_ctrl;
	struct dsi_pll_resource *pll_res;
	struct dsi_ctrl *ctrl;

	if (!display) {
		DSI_ERR("invalid arguments\n");
		return -EINVAL;
	}

	if (is_skip_op_required(display))
		return 0;

	m_ctrl = &display->ctrl[display->clk_master_idx];
	if ((!m_ctrl->phy) || (!m_ctrl->ctrl)) {
		DSI_ERR("[%s] PHY not found\n", display->name);
		return -EINVAL;
	}

	pll_res = m_ctrl->phy->pll;
	if (!pll_res) {
		DSI_ERR("[%s] PLL res not found\n", display->name);
		return -EINVAL;
	}

	ctrl = m_ctrl->ctrl;
	pll_res->byteclk_rate = ctrl->clk_freq.byte_clk_rate;
	pll_res->pclk_rate = ctrl->clk_freq.pix_clk_rate;

	rc = dsi_phy_configure(m_ctrl->phy, commit);

	return rc;
}

static int dsi_display_phy_reset_config(struct dsi_display *display,
		bool enable)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_phy_reset_config(ctrl->ctrl, enable);
		if (rc) {
			DSI_ERR("[%s] failed to %s phy reset, rc=%d\n",
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

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		dsi_phy_toggle_resync_fifo(ctrl->phy);
	}

	/*
	 * After retime buffer synchronization we need to turn of clk_en_sel
	 * bit on each phy. Avoid this for Cphy.
	 */

	if (dsi_is_type_cphy(&display->panel->host_config))
		return;

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		dsi_phy_reset_clk_en_sel(ctrl->phy);
	}

}

static int dsi_display_ctrl_update(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_host_timing_update(ctrl->ctrl);
		if (rc) {
			DSI_ERR("[%s] failed to update host_%d, rc=%d\n",
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
	bool skip_op = is_skip_op_required(display);

	/* when ULPS suspend feature is enabled, we will keep the lanes in
	 * ULPS during suspend state and clamp DSI phy. Hence while resuming
	 * we will programe DSI controller as part of core clock enable.
	 * After that we should not re-configure DSI controller again here for
	 * usecases where we are resuming from ulps suspend as it might put
	 * the HW in bad state.
	 */
	if (!display->panel->ulps_suspend_enabled || !display->ulps_enabled) {
		display_for_each_ctrl(i, display) {
			ctrl = &display->ctrl[i];
			rc = dsi_ctrl_host_init(ctrl->ctrl, skip_op);
			if (rc) {
				DSI_ERR(
				"[%s] failed to init host_%d, skip_op=%d, rc=%d\n",
				       display->name, i, skip_op, rc);
				goto error_host_deinit;
			}
		}
	} else {
		display_for_each_ctrl(i, display) {
			ctrl = &display->ctrl[i];
			rc = dsi_ctrl_update_host_state(ctrl->ctrl,
							DSI_CTRL_OP_HOST_INIT,
							true);
			if (rc)
				DSI_DEBUG("host init update failed rc=%d\n",
						rc);
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

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_host_deinit(ctrl->ctrl);
		if (rc) {
			DSI_ERR("[%s] failed to deinit host_%d, rc=%d\n",
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
	bool skip_op = is_skip_op_required(display);

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	rc = dsi_ctrl_set_host_engine_state(m_ctrl->ctrl,
			DSI_CTRL_ENGINE_ON, skip_op);
	if (rc) {
		DSI_ERR("[%s]enable host engine failed, skip_op:%d rc:%d\n",
		       display->name, skip_op, rc);
		goto error;
	}

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_host_engine_state(ctrl->ctrl,
						DSI_CTRL_ENGINE_ON, skip_op);
		if (rc) {
			DSI_ERR(
			"[%s] enable host engine failed, skip_op:%d rc:%d\n",
			       display->name, skip_op, rc);
			goto error_disable_master;
		}
	}

	return rc;
error_disable_master:
	(void)dsi_ctrl_set_host_engine_state(m_ctrl->ctrl,
					DSI_CTRL_ENGINE_OFF, skip_op);
error:
	return rc;
}

static int dsi_display_ctrl_host_disable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	bool skip_op = is_skip_op_required(display);

	/*
	 * This is a defensive check. In reality as this is called after panel OFF commands, which
	 * can never be ASYNC, the controller post_tx_queued flag will never be set when this API
	 * is called.
	 */
	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || !(ctrl->ctrl->post_tx_queued))
			continue;
		flush_workqueue(display->post_cmd_tx_workq);
		cancel_work_sync(&ctrl->ctrl->post_cmd_tx_work);
		ctrl->ctrl->post_tx_queued = false;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	/*
	 * For platforms where ULPS is controlled by DSI controller block,
	 * do not disable dsi controller block if lanes are to be
	 * kept in ULPS during suspend. So just update the SW state
	 * and return early.
	 */
	if (display->panel->ulps_suspend_enabled &&
			!m_ctrl->phy->hw.ops.ulps_ops.ulps_request) {
		display_for_each_ctrl(i, display) {
			ctrl = &display->ctrl[i];
			rc = dsi_ctrl_update_host_state(ctrl->ctrl,
					DSI_CTRL_OP_HOST_ENGINE,
					false);
			if (rc)
				DSI_DEBUG("host state update failed %d\n", rc);
		}
		return rc;
	}

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_host_engine_state(ctrl->ctrl,
					DSI_CTRL_ENGINE_OFF, skip_op);
		if (rc)
			DSI_ERR(
			"[%s] disable host engine failed, skip_op:%d rc:%d\n",
			       display->name, skip_op, rc);
	}

	rc = dsi_ctrl_set_host_engine_state(m_ctrl->ctrl,
				DSI_CTRL_ENGINE_OFF, skip_op);
	if (rc) {
		DSI_ERR("[%s] disable mhost engine failed, skip_op:%d rc:%d\n",
		       display->name, skip_op, rc);
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
	bool skip_op = is_skip_op_required(display);

	m_ctrl = &display->ctrl[display->video_master_idx];

	rc = dsi_ctrl_set_vid_engine_state(m_ctrl->ctrl,
			DSI_CTRL_ENGINE_ON, skip_op);
	if (rc) {
		DSI_ERR("[%s] enable mvid engine failed, skip_op:%d rc:%d\n",
				display->name, skip_op, rc);
		goto error;
	}

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_vid_engine_state(ctrl->ctrl,
					DSI_CTRL_ENGINE_ON, skip_op);
		if (rc) {
			DSI_ERR(
			    "[%s] enable vid engine failed, skip_op:%d rc:%d\n",
				display->name, skip_op, rc);
			goto error_disable_master;
		}
	}

	return rc;
error_disable_master:
	(void)dsi_ctrl_set_vid_engine_state(m_ctrl->ctrl,
				DSI_CTRL_ENGINE_OFF, skip_op);
error:
	return rc;
}

static int dsi_display_vid_engine_disable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	bool skip_op = is_skip_op_required(display);

	m_ctrl = &display->ctrl[display->video_master_idx];

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_set_vid_engine_state(ctrl->ctrl,
					DSI_CTRL_ENGINE_OFF, skip_op);
		if (rc)
			DSI_ERR(
			   "[%s] disable vid engine failed, skip_op:%d rc:%d\n",
				display->name, skip_op, rc);
	}

	rc = dsi_ctrl_set_vid_engine_state(m_ctrl->ctrl,
				DSI_CTRL_ENGINE_OFF, skip_op);
	if (rc)
		DSI_ERR("[%s] disable mvid engine failed, skip_op:%d rc:%d\n",
				display->name, skip_op, rc);

	return rc;
}

static int dsi_display_phy_enable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	enum dsi_phy_pll_source m_src = DSI_PLL_SOURCE_STANDALONE;
	bool skip_op = is_skip_op_required(display);

	m_ctrl = &display->ctrl[display->clk_master_idx];
	if (display->ctrl_count > 1)
		m_src = DSI_PLL_SOURCE_NATIVE;

	rc = dsi_phy_enable(m_ctrl->phy, &display->config,
			m_src, true, skip_op);
	if (rc) {
		DSI_ERR("[%s] failed to enable DSI PHY, skip_op=%d rc=%d\n",
		       display->name, skip_op, rc);
		goto error;
	}

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_phy_enable(ctrl->phy, &display->config,
				DSI_PLL_SOURCE_NON_NATIVE, true, skip_op);
		if (rc) {
			DSI_ERR(
				"[%s] failed to enable DSI PHY, skip_op: %d rc=%d\n",
				display->name, skip_op, rc);
			goto error_disable_master;
		}
	}

	return rc;

error_disable_master:
	(void)dsi_phy_disable(m_ctrl->phy, skip_op);
error:
	return rc;
}

static int dsi_display_phy_disable(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	bool skip_op = is_skip_op_required(display);

	m_ctrl = &display->ctrl[display->clk_master_idx];

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_phy_disable(ctrl->phy, skip_op);
		if (rc)
			DSI_ERR(
				"[%s] failed to disable DSI PHY, skip_op=%d rc=%d\n",
				display->name, skip_op,  rc);
	}

	rc = dsi_phy_disable(m_ctrl->phy, skip_op);
	if (rc)
		DSI_ERR("[%s] failed to disable DSI PHY, skip_op=%d rc=%d\n",
		       display->name, skip_op, rc);

	return rc;
}

static int dsi_display_wake_up(struct dsi_display *display)
{
	return 0;
}

static int dsi_display_broadcast_cmd(struct dsi_display *display, struct dsi_cmd_desc *cmd)
{
	int rc = 0;
	struct dsi_display_ctrl *ctrl, *m_ctrl;
	int i;
	u32 flags = 0;

	/*
	 * 1. Setup commands in FIFO
	 * 2. Trigger commands
	 */
	m_ctrl = &display->ctrl[display->cmd_master_idx];

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		flags = cmd->ctrl_flags;
		if (ctrl == m_ctrl)
			flags |= DSI_CTRL_CMD_BROADCAST_MASTER;
		rc = dsi_ctrl_transfer_prepare(ctrl->ctrl, flags);
		if (rc) {
			DSI_ERR("[%s] prepare for cmd transfer failed,rc=%d\n",
				   display->name, rc);
			if (ctrl != m_ctrl)
				dsi_ctrl_transfer_unprepare(m_ctrl->ctrl, flags |
						DSI_CTRL_CMD_BROADCAST_MASTER);
			return rc;
		}
	}

	cmd->ctrl_flags |= DSI_CTRL_CMD_BROADCAST_MASTER;
	rc = dsi_ctrl_cmd_transfer(m_ctrl->ctrl, cmd);
	if (rc) {
		DSI_ERR("[%s] cmd transfer failed on master,rc=%d\n",
		       display->name, rc);
		goto error;
	}

	cmd->ctrl_flags &= ~DSI_CTRL_CMD_BROADCAST_MASTER;
	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (ctrl == m_ctrl)
			continue;

		rc = dsi_ctrl_cmd_transfer(ctrl->ctrl, cmd);
		if (rc) {
			DSI_ERR("[%s] cmd transfer failed, rc=%d\n",
			       display->name, rc);
			goto error;
		}

		rc = dsi_ctrl_cmd_tx_trigger(ctrl->ctrl, cmd->ctrl_flags);
		if (rc) {
			DSI_ERR("[%s] cmd trigger failed, rc=%d\n",
			       display->name, rc);
			goto error;
		}
	}

	rc = dsi_ctrl_cmd_tx_trigger(m_ctrl->ctrl, cmd->ctrl_flags | DSI_CTRL_CMD_BROADCAST_MASTER);
	if (rc) {
		DSI_ERR("[%s] cmd trigger failed for master, rc=%d\n",
		       display->name, rc);
		goto error;
	}

error:
	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		flags = cmd->ctrl_flags;
		if (ctrl == m_ctrl)
			flags |= DSI_CTRL_CMD_BROADCAST_MASTER;
		dsi_ctrl_transfer_unprepare(ctrl->ctrl, flags);
	}

	return rc;
}

static int dsi_display_phy_sw_reset(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;

	/*
	 * For continuous splash and trusted vm environment,
	 * ctrl states are updated separately and hence we do
	 * an early return
	 */
	if (is_skip_op_required(display)) {
		DSI_DEBUG(
			"cont splash/trusted vm use case, phy sw reset not required\n");
		return 0;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	rc = dsi_ctrl_phy_sw_reset(m_ctrl->ctrl);
	if (rc) {
		DSI_ERR("[%s] failed to reset phy, rc=%d\n", display->name, rc);
		goto error;
	}

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_phy_sw_reset(ctrl->ctrl);
		if (rc) {
			DSI_ERR("[%s] failed to reset phy, rc=%d\n",
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

int dsi_host_transfer_sub(struct mipi_dsi_host *host, struct dsi_cmd_desc *cmd)
{
	struct dsi_display *display;
	int rc = 0;

	if (!host || !cmd) {
		DSI_ERR("Invalid params\n");
		return 0;
	}

	display = to_dsi_display(host);

	/* Avoid sending DCS commands when ESD recovery is pending */
	if (atomic_read(&display->panel->esd_recovery_pending)) {
		DSI_DEBUG("ESD recovery pending\n");
		return 0;
	}

	rc = dsi_display_wake_up(display);
	if (rc) {
		DSI_ERR("[%s] failed to wake up display, rc=%d\n", display->name, rc);
		goto error;
	}

	if (display->tx_cmd_buf == NULL) {
		rc = dsi_host_alloc_cmd_tx_buffer(display);
		if (rc) {
			DSI_ERR("failed to allocate cmd tx buffer memory\n");
			goto error;
		}
	}

	dsi_display_set_cmd_tx_ctrl_flags(display, cmd);

	if (cmd->msg.type == MIPI_DSI_DCS_READ)
		cmd->ctrl_flags |= DSI_CTRL_CMD_READ;

	mi_dsi_print_51_backlight_log(display->panel, cmd);

	if (cmd->ctrl_flags & DSI_CTRL_CMD_BROADCAST) {
		rc = dsi_display_broadcast_cmd(display, cmd);
		if (rc) {
			DSI_ERR("[%s] cmd broadcast failed, rc=%d\n", display->name, rc);
			goto error;
		}
	} else {
		int idx = cmd->ctrl;

		rc = dsi_ctrl_transfer_prepare(display->ctrl[idx].ctrl, cmd->ctrl_flags);
		if (rc) {
			DSI_ERR("failed to prepare for command transfer: %d\n", rc);
			goto error;
		}

		rc = dsi_ctrl_cmd_transfer(display->ctrl[idx].ctrl, cmd);
		if ((cmd->msg.type == MIPI_DSI_DCS_READ && rc <= 0) ||
			(cmd->msg.type != MIPI_DSI_DCS_READ && rc))
			DSI_ERR("[%s] cmd transfer failed, rc=%d\n", display->name, rc);

		dsi_ctrl_transfer_unprepare(display->ctrl[idx].ctrl, cmd->ctrl_flags);
	}

error:
	return rc;
}

static ssize_t dsi_host_transfer(struct mipi_dsi_host *host, const struct mipi_dsi_msg *msg)
{
	int rc = 0;
	struct dsi_cmd_desc cmd;

	if (!msg) {
		DSI_ERR("Invalid params\n");
		return 0;
	}

	memcpy(&cmd.msg, msg, sizeof(*msg));
	cmd.ctrl = 0;
	cmd.post_wait_ms = 0;
	cmd.ctrl_flags = 0;

	rc = dsi_host_transfer_sub(host, &cmd);

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
		DSI_ERR("[%s] failed to register mipi dsi host, rc=%d\n",
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

static bool dsi_display_check_prefix(const char *clk_prefix,
					const char *clk_name)
{
	return !!strnstr(clk_name, clk_prefix, strlen(clk_name));
}

static int dsi_display_get_clocks_count(struct dsi_display *display,
						char *dsi_clk_name)
{
	if (display->fw)
		return dsi_parser_count_strings(display->parser_node,
			dsi_clk_name);
	else
		return of_property_count_strings(display->panel_node,
			dsi_clk_name);
}

static void dsi_display_get_clock_name(struct dsi_display *display,
					char *dsi_clk_name, int index,
					const char **clk_name)
{
	if (display->fw)
		dsi_parser_read_string_index(display->parser_node,
			dsi_clk_name, index, clk_name);
	else
		of_property_read_string_index(display->panel_node,
			dsi_clk_name, index, clk_name);
}

static int dsi_display_clocks_init(struct dsi_display *display)
{
	int i, rc = 0, num_clk = 0;
	const char *clk_name;
	const char *pll_byte = "pll_byte", *pll_dsi = "pll_dsi";
	struct clk *dsi_clk;
	struct dsi_clk_link_set *pll = &display->clock_info.pll_clks;
	char *dsi_clock_name;

	if (!strcmp(display->display_type, "primary"))
		dsi_clock_name = "qcom,dsi-select-clocks";
	else
		dsi_clock_name = "qcom,dsi-select-sec-clocks";

	num_clk = dsi_display_get_clocks_count(display, dsi_clock_name);

	for (i = 0; i < num_clk; i++) {
		dsi_display_get_clock_name(display, dsi_clock_name, i,
						&clk_name);

		DSI_DEBUG("clock name:%s\n", clk_name);

		dsi_clk = devm_clk_get(&display->pdev->dev, clk_name);
		if (IS_ERR_OR_NULL(dsi_clk)) {
			rc = PTR_ERR(dsi_clk);

			DSI_ERR("failed to get %s, rc=%d\n", clk_name, rc);

			if (dsi_display_check_prefix(pll_byte, clk_name)) {
				pll->byte_clk = NULL;
				goto error;
			}

			if (dsi_display_check_prefix(pll_dsi, clk_name)) {
				pll->pixel_clk = NULL;
				goto error;
			}
		}

		if (dsi_display_check_prefix(pll_byte, clk_name)) {
			pll->byte_clk = dsi_clk;
			continue;
		}
		if (dsi_display_check_prefix(pll_dsi, clk_name)) {
			pll->pixel_clk = dsi_clk;
			continue;
		}

	}

	return 0;
error:
	return rc;
}

static int dsi_display_clk_ctrl_cb(void *priv,
	struct dsi_clk_ctrl_info clk_state_info)
{
	int rc = 0;
	struct dsi_display *display = NULL;
	void *clk_handle = NULL;

	if (!priv) {
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	display = priv;

	if (clk_state_info.client == DSI_CLK_REQ_MDP_CLIENT) {
		clk_handle = display->mdp_clk_handle;
	} else if (clk_state_info.client == DSI_CLK_REQ_DSI_CLIENT) {
		clk_handle = display->dsi_clk_handle;
	} else {
		DSI_ERR("invalid clk handle, return error\n");
		return -EINVAL;
	}

	/*
	 * TODO: Wait for CMD_MDP_DONE interrupt if MDP client tries
	 * to turn off DSI clocks.
	 */
	rc = dsi_display_clk_ctrl(clk_handle,
		clk_state_info.clk_type, clk_state_info.clk_state);
	if (rc) {
		DSI_ERR("[%s] failed to %d DSI %d clocks, rc=%d\n",
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

	display_for_each_ctrl(i, display) {
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
		(l_type & DSI_LINK_LP_CLK)) {
		/*
		 * If continuous clock is enabled then disable it
		 * before entering into ULPS Mode.
		 */
		if (display->panel->host_config.force_hs_clk_lane)
			_dsi_display_continuous_clk_ctrl(display, false);
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
			DSI_ERR("%s: failed enable ulps, rc = %d\n",
			       __func__, rc);
	}

	if ((clk & DSI_LINK_CLK) && (new_state == DSI_CLK_OFF) &&
		(l_type & DSI_LINK_HS_CLK)) {
		/*
		 * PHY clock gating should be disabled before the PLL and the
		 * branch clocks are turned off. Otherwise, it is possible that
		 * the clock RCGs may not be turned off correctly resulting
		 * in clock warnings.
		 */
		rc = dsi_display_config_clk_gating(display, false);
		if (rc)
			DSI_ERR("[%s] failed to disable clk gating, rc=%d\n",
					display->name, rc);
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
				DSI_ERR("%s: Failed to enable dsi clamps. rc=%d\n",
					__func__, rc);

			rc = dsi_display_phy_reset_config(display, false);
			if (rc)
				DSI_ERR("%s: Failed to reset phy, rc=%d\n",
						__func__, rc);
		} else {
			/* Make sure that controller is not in ULPS state when
			 * the DSI link is not active.
			 */
			rc = dsi_display_set_ulps(display, false);
			if (rc)
				DSI_ERR("%s: failed to disable ulps. rc=%d\n",
					__func__, rc);
		}
		/* dsi will not be able to serve irqs from here on */
		dsi_display_ctrl_irq_update(display, false);

		/* cache the MISR values */
		display_for_each_ctrl(i, display) {
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
				DSI_ERR("%s: Failed to enter ULPS. rc=%d\n",
					__func__, rc);
				goto error;
			}
		}

		rc = dsi_display_phy_reset_config(display, true);
		if (rc) {
			DSI_ERR("%s: Failed to reset phy, rc=%d\n",
						__func__, rc);
			goto error;
		}

		rc = dsi_display_set_clamp(display, false);
		if (rc) {
			DSI_ERR("%s: Failed to disable dsi clamps. rc=%d\n",
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
				DSI_ERR("%s: failed to disable ulps, rc= %d\n",
				       __func__, rc);
				goto error;
			}
		}

		if (display->panel->host_config.force_hs_clk_lane)
			_dsi_display_continuous_clk_ctrl(display, true);

		rc = dsi_display_config_clk_gating(display, true);
		if (rc) {
			DSI_ERR("[%s] failed to enable clk gating %d\n",
					display->name, rc);
			goto error;
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
		DSI_ERR("%s: Invalid arg\n", __func__);
		return -EINVAL;
	}

	if ((clk_type & DSI_CORE_CLK) &&
	    (curr_state == DSI_CLK_OFF)) {
		rc = dsi_display_phy_power_off(display);
		if (rc)
			DSI_ERR("[%s] failed to power off PHY, rc=%d\n",
				   display->name, rc);

		rc = dsi_display_ctrl_power_off(display);
		if (rc)
			DSI_ERR("[%s] failed to power DSI vregs, rc=%d\n",
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
		DSI_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if ((clk_type & DSI_CORE_CLK) && (new_state == DSI_CLK_ON)) {
		/*
		 * Enable DSI core power
		 * 1.> PANEL_PM are controlled as part of
		 *     panel_power_ctrl. Needed not be handled here.
		 * 2.> CTRL_PM need to be enabled/disabled
		 *     only during unblank/blank. Their state should
		 *     not be changed during static screen.
		 */

		DSI_DEBUG("updating power states for ctrl and phy\n");
		rc = dsi_display_ctrl_power_on(display);
		if (rc) {
			DSI_ERR("[%s] failed to power on dsi controllers, rc=%d\n",
				   display->name, rc);
			return rc;
		}

		rc = dsi_display_phy_power_on(display);
		if (rc) {
			DSI_ERR("[%s] failed to power on dsi phy, rc = %d\n",
				   display->name, rc);
			return rc;
		}

		DSI_DEBUG("%s: Enable DSI core power\n", __func__);
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
		DSI_ERR("invalid params\n");
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
		DSI_DEBUG("Incorrect mapping, configure default\n");
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
		DSI_WARN("%s: invalid lane map %s specified. defaulting to lane_map0123\n",
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
	struct device_node *disp_node = display->panel_node;
	u32 *val = NULL;
	int rc = 0;

	val = kcalloc(count, sizeof(*val), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(val)) {
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

	DSI_DEBUG("%s index=%d\n", propname, rc);
end:
	kfree(val);
	return rc;
}

static bool dsi_display_validate_res(struct dsi_display *display)
{
	struct device_node *of_node = display->pdev->dev.of_node;
	struct of_phandle_iterator it;
	bool ctrl_avail = false;
	bool phy_avail = false;

	/*
	 * At least if one of the controller or PHY is present or has been probed, the
	 * dsi_display_dev_probe can pass this check.  Exact ctrl and PHY match will be
	 * done after the DT is parsed.
	 */
	of_phandle_iterator_init(&it, of_node, "qcom,dsi-ctrl", NULL, 0);
	while (of_phandle_iterator_next(&it) == 0)
		ctrl_avail |= dsi_ctrl_check_resource(it.node);

	of_phandle_iterator_init(&it, of_node, "qcom,dsi-phy", NULL, 0);
	while (of_phandle_iterator_next(&it) == 0)
		phy_avail |= dsi_phy_check_resource(it.node);

	return (ctrl_avail & phy_avail);
}

static int dsi_display_get_phandle_count(struct dsi_display *display,
			const char *propname)
{
	if (display->fw)
		return dsi_parser_count_u32_elems(display->parser_node,
				propname);
	else
		return of_property_count_u32_elems(display->panel_node,
				propname);
}

static int dsi_display_parse_dt(struct dsi_display *display)
{
	int i, rc = 0;
	u32 phy_count = 0;
	struct device_node *of_node = display->pdev->dev.of_node;
	char *dsi_ctrl_name, *dsi_phy_name;

	if (!strcmp(display->display_type, "primary")) {
		dsi_ctrl_name = "qcom,dsi-ctrl-num";
		dsi_phy_name = "qcom,dsi-phy-num";
	} else {
		dsi_ctrl_name = "qcom,dsi-sec-ctrl-num";
		dsi_phy_name = "qcom,dsi-sec-phy-num";
	}

	display->ctrl_count = dsi_display_get_phandle_count(display,
					dsi_ctrl_name);
	phy_count = dsi_display_get_phandle_count(display, dsi_phy_name);

	DSI_DEBUG("ctrl count=%d, phy count=%d\n",
			display->ctrl_count, phy_count);

	if (!phy_count || !display->ctrl_count) {
		DSI_ERR("no ctrl/phys found\n");
		rc = -ENODEV;
		goto error;
	}

	if (phy_count != display->ctrl_count) {
		DSI_ERR("different ctrl and phy counts\n");
		rc = -ENODEV;
		goto error;
	}

	display_for_each_ctrl(i, display) {
		struct dsi_display_ctrl *ctrl = &display->ctrl[i];
		int index;
		index = dsi_display_get_phandle_index(display, dsi_ctrl_name,
			display->ctrl_count, i);
		ctrl->ctrl_of_node = of_parse_phandle(of_node,
				"qcom,dsi-ctrl", index);
		of_node_put(ctrl->ctrl_of_node);

		index = dsi_display_get_phandle_index(display, dsi_phy_name,
			display->ctrl_count, i);
		ctrl->phy_of_node = of_parse_phandle(of_node,
				"qcom,dsi-phy", index);
		of_node_put(ctrl->phy_of_node);
	}

	/* Parse TE data */
	dsi_display_parse_te_data(display);

	/* Parse all external bridges from port 0 */
	display_for_each_ctrl(i, display) {
		display->ext_bridge[i].node_of =
			of_graph_get_remote_node(of_node, 0, i);
		if (display->ext_bridge[i].node_of)
			display->ext_bridge_cnt++;
		else
			break;
	}

	/* Parse Demura data */
	dsi_display_parse_demura_data(display);

	DSI_DEBUG("success\n");
error:
	return rc;
}

static bool dsi_display_validate_panel_resources(struct dsi_display *display)
{
	if (!is_sim_panel(display)) {
		if (!gpio_is_valid(display->panel->reset_config.reset_gpio)) {
			DSI_ERR("invalid reset gpio for the panel\n");
			return false;
		}
	}

	return true;
}

static int dsi_display_res_init(struct dsi_display *display)
{
	int rc = 0;
	int i;
	struct dsi_display_ctrl *ctrl;

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		ctrl->ctrl = dsi_ctrl_get(ctrl->ctrl_of_node);
		if (IS_ERR_OR_NULL(ctrl->ctrl)) {
			rc = PTR_ERR(ctrl->ctrl);
			DSI_ERR("failed to get dsi controller, rc=%d\n", rc);
			ctrl->ctrl = NULL;
			goto error_ctrl_put;
		}

		ctrl->phy = dsi_phy_get(ctrl->phy_of_node);
		if (IS_ERR_OR_NULL(ctrl->phy)) {
			rc = PTR_ERR(ctrl->phy);
			DSI_ERR("failed to get phy controller, rc=%d\n", rc);
			dsi_ctrl_put(ctrl->ctrl);
			ctrl->phy = NULL;
			goto error_ctrl_put;
		}
	}

	display->panel = dsi_panel_get(&display->pdev->dev,
				display->panel_node,
				display->parser_node,
				display->display_type,
				display->cmdline_topology,
				display->trusted_vm_env);
	if (IS_ERR_OR_NULL(display->panel)) {
		rc = PTR_ERR(display->panel);
		DSI_ERR("failed to get panel, rc=%d\n", rc);
		display->panel = NULL;
		goto error_ctrl_put;
	}

	display->panel->te_using_watchdog_timer |= display->sw_te_using_wd;

	if (!dsi_display_validate_panel_resources(display)) {
		rc = -EINVAL;
		goto error_panel_put;
	}

	display_for_each_ctrl(i, display) {
		struct msm_dsi_phy *phy = display->ctrl[i].phy;
		struct dsi_host_common_cfg *host = &display->panel->host_config;

		phy->cfg.force_clk_lane_hs =
			display->panel->host_config.force_hs_clk_lane;
		phy->cfg.phy_type =
			display->panel->host_config.phy_type;

		/*
		 * Parse the dynamic clock trim codes for PLL, for video mode panels that have
		 * dynamic clock property set.
		 */
		if ((display->panel->dyn_clk_caps.dyn_clk_support) &&
				(display->panel->panel_mode == DSI_OP_VIDEO_MODE))
			dsi_phy_pll_parse_dfps_data(phy);

		phy->cfg.split_link.enabled = host->split_link.enabled;
		phy->cfg.split_link.num_sublinks = host->split_link.num_sublinks;
		phy->cfg.split_link.lanes_per_sublink = host->split_link.lanes_per_sublink;
	}

	rc = dsi_display_parse_lane_map(display);
	if (rc) {
		DSI_ERR("Lane map not found, rc=%d\n", rc);
		goto error_panel_put;
	}

	rc = dsi_display_clocks_init(display);
	if (rc) {
		DSI_ERR("Failed to parse clock data, rc=%d\n", rc);
		goto error_panel_put;
	}

	/**
	 * In trusted vm, the connectors will not be enabled
	 * until the HW resources are assigned and accepted.
	 */
	if (display->trusted_vm_env) {
		display->is_active = false;
		display->hw_ownership = false;
	} else {
		display->is_active = true;
		display->hw_ownership = true;
	}

	return 0;
error_panel_put:
	dsi_panel_put(display->panel);
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

	display_for_each_ctrl(i, display) {
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

	display_for_each_ctrl(i, display) {
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
		DSI_ERR("Invalid params\n");
		return false;
	}

	cur = display->panel->cur_mode;

	if (cur->timing.h_active != tgt->timing.h_active) {
		DSI_DEBUG("timing.h_active differs %d %d\n",
				cur->timing.h_active, tgt->timing.h_active);
		return false;
	}

	if (cur->timing.h_back_porch != tgt->timing.h_back_porch) {
		DSI_DEBUG("timing.h_back_porch differs %d %d\n",
				cur->timing.h_back_porch,
				tgt->timing.h_back_porch);
		return false;
	}

	if (cur->timing.h_sync_width != tgt->timing.h_sync_width) {
		DSI_DEBUG("timing.h_sync_width differs %d %d\n",
				cur->timing.h_sync_width,
				tgt->timing.h_sync_width);
		return false;
	}

	if (cur->timing.h_front_porch != tgt->timing.h_front_porch) {
		DSI_DEBUG("timing.h_front_porch differs %d %d\n",
				cur->timing.h_front_porch,
				tgt->timing.h_front_porch);
		if (dfps_type != DSI_DFPS_IMMEDIATE_HFP)
			return false;
	}

	if (cur->timing.h_skew != tgt->timing.h_skew) {
		DSI_DEBUG("timing.h_skew differs %d %d\n",
				cur->timing.h_skew,
				tgt->timing.h_skew);
		return false;
	}

	/* skip polarity comparison */

	if (cur->timing.v_active != tgt->timing.v_active) {
		DSI_DEBUG("timing.v_active differs %d %d\n",
				cur->timing.v_active,
				tgt->timing.v_active);
		return false;
	}

	if (cur->timing.v_back_porch != tgt->timing.v_back_porch) {
		DSI_DEBUG("timing.v_back_porch differs %d %d\n",
				cur->timing.v_back_porch,
				tgt->timing.v_back_porch);
		return false;
	}

	if (cur->timing.v_sync_width != tgt->timing.v_sync_width) {
		DSI_DEBUG("timing.v_sync_width differs %d %d\n",
				cur->timing.v_sync_width,
				tgt->timing.v_sync_width);
		return false;
	}

	if (cur->timing.v_front_porch != tgt->timing.v_front_porch) {
		DSI_DEBUG("timing.v_front_porch differs %d %d\n",
				cur->timing.v_front_porch,
				tgt->timing.v_front_porch);
		if (dfps_type != DSI_DFPS_IMMEDIATE_VFP)
			return false;
	}

	/* skip polarity comparison */

	if (cur->timing.refresh_rate == tgt->timing.refresh_rate)
		DSI_DEBUG("timing.refresh_rate identical %d %d\n",
				cur->timing.refresh_rate,
				tgt->timing.refresh_rate);

	if (cur->pixel_clk_khz != tgt->pixel_clk_khz)
		DSI_DEBUG("pixel_clk_khz differs %d %d\n",
				cur->pixel_clk_khz, tgt->pixel_clk_khz);

	if (cur->dsi_mode_flags != tgt->dsi_mode_flags)
		DSI_DEBUG("flags differs %d %d\n",
				cur->dsi_mode_flags, tgt->dsi_mode_flags);

	return true;
}

void dsi_display_update_byte_intf_div(struct dsi_display *display)
{
	struct dsi_host_common_cfg *config;
	struct dsi_display_ctrl *m_ctrl;
	int phy_ver;

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	config = &display->panel->host_config;

	phy_ver = dsi_phy_get_version(m_ctrl->phy);
	config->byte_intf_clk_div = 2;
}

static int dsi_display_update_dsi_bitrate(struct dsi_display *display,
					  u32 bit_clk_rate)
{
	int rc = 0;
	int i;

	DSI_DEBUG("%s:bit rate:%d\n", __func__, bit_clk_rate);
	if (!display->panel) {
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (bit_clk_rate == 0) {
		DSI_ERR("Invalid bit clock rate\n");
		return -EINVAL;
	}

	display->config.bit_clk_rate_hz = bit_clk_rate;

	display_for_each_ctrl(i, display) {
		struct dsi_display_ctrl *dsi_disp_ctrl = &display->ctrl[i];
		struct dsi_ctrl *ctrl = dsi_disp_ctrl->ctrl;
		u32 num_of_lanes = 0, bpp, byte_intf_clk_div;
		u64 bit_rate, pclk_rate, bit_rate_per_lane, byte_clk_rate,
				byte_intf_clk_rate;
		u32 bits_per_symbol = 16, num_of_symbols = 7; /* For Cphy */
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
			DSI_ERR("Invalid lane count\n");
			rc = -EINVAL;
			goto error;
		}

		bpp = dsi_pixel_format_to_bpp(host_cfg->dst_format);

		bit_rate = display->config.bit_clk_rate_hz * num_of_lanes;
		bit_rate_per_lane = bit_rate;
		do_div(bit_rate_per_lane, num_of_lanes);
		pclk_rate = bit_rate;
		do_div(pclk_rate, bpp);
		if (host_cfg->phy_type == DSI_PHY_TYPE_DPHY) {
			bit_rate_per_lane = bit_rate;
			do_div(bit_rate_per_lane, num_of_lanes);
			byte_clk_rate = bit_rate_per_lane;
			do_div(byte_clk_rate, 8);
			byte_intf_clk_rate = byte_clk_rate;
			byte_intf_clk_div = host_cfg->byte_intf_clk_div;
			do_div(byte_intf_clk_rate, byte_intf_clk_div);
		} else {
			bit_rate_per_lane = bit_clk_rate;
			pclk_rate *= bits_per_symbol;
			do_div(pclk_rate, num_of_symbols);
			byte_clk_rate = bit_clk_rate;
			do_div(byte_clk_rate, num_of_symbols);

			/* For CPHY, byte_intf_clk is same as byte_clk */
			byte_intf_clk_rate = byte_clk_rate;
		}

		DSI_DEBUG("bit_clk_rate = %llu, bit_clk_rate_per_lane = %llu\n",
			 bit_rate, bit_rate_per_lane);
		DSI_DEBUG("byte_clk_rate = %llu, byte_intf_clk_rate = %llu\n",
			  byte_clk_rate, byte_intf_clk_rate);
		DSI_DEBUG("pclk_rate = %llu\n", pclk_rate);
		SDE_EVT32(i, bit_rate, byte_clk_rate, pclk_rate);

		ctrl->clk_freq.byte_clk_rate = byte_clk_rate;
		ctrl->clk_freq.byte_intf_clk_rate = byte_intf_clk_rate;
		ctrl->clk_freq.pix_clk_rate = pclk_rate;
		rc = dsi_clk_set_link_frequencies(display->dsi_clk_handle,
			ctrl->clk_freq, ctrl->cell_index);
		if (rc) {
			DSI_ERR("Failed to update link frequencies\n");
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

static void _dsi_display_calc_pipe_delay(struct dsi_display *display,
				    struct dsi_dyn_clk_delay *delay,
				    struct dsi_display_mode *mode)
{
	u32 esc_clk_rate_hz;
	u32 pclk_to_esc_ratio, byte_to_esc_ratio, hr_bit_to_esc_ratio;
	u32 hsync_period = 0;
	struct dsi_display_ctrl *m_ctrl;
	struct dsi_ctrl *dsi_ctrl;
	struct dsi_phy_cfg *cfg;
	int phy_ver;

	m_ctrl = &display->ctrl[display->clk_master_idx];
	dsi_ctrl = m_ctrl->ctrl;

	cfg = &(m_ctrl->phy->cfg);

	esc_clk_rate_hz = dsi_ctrl->clk_freq.esc_clk_rate;
	pclk_to_esc_ratio = (dsi_ctrl->clk_freq.pix_clk_rate /
			     esc_clk_rate_hz);
	byte_to_esc_ratio = (dsi_ctrl->clk_freq.byte_clk_rate /
			     esc_clk_rate_hz);
	hr_bit_to_esc_ratio = ((dsi_ctrl->clk_freq.byte_clk_rate * 4) /
					esc_clk_rate_hz);

	hsync_period = dsi_h_total_dce(&mode->timing);
	delay->pipe_delay = (hsync_period + 1) / pclk_to_esc_ratio;
	if (!display->panel->video_config.eof_bllp_lp11_en)
		delay->pipe_delay += (17 / pclk_to_esc_ratio) +
			((21 + (display->config.common_config.t_clk_pre + 1) +
			  (display->config.common_config.t_clk_post + 1)) /
			 byte_to_esc_ratio) +
			((((cfg->timing.lane_v3[8] >> 1) + 1) +
			((cfg->timing.lane_v3[6] >> 1) + 1) +
			((cfg->timing.lane_v3[3] * 4) +
			 (cfg->timing.lane_v3[5] >> 1) + 1) +
			((cfg->timing.lane_v3[7] >> 1) + 1) +
			((cfg->timing.lane_v3[1] >> 1) + 1) +
			((cfg->timing.lane_v3[4] >> 1) + 1)) /
			 hr_bit_to_esc_ratio);

	delay->pipe_delay2 = 0;
	if (display->panel->host_config.force_hs_clk_lane)
		delay->pipe_delay2 = (6 / byte_to_esc_ratio) +
			((((cfg->timing.lane_v3[1] >> 1) + 1) +
			  ((cfg->timing.lane_v3[4] >> 1) + 1)) /
			 hr_bit_to_esc_ratio);

	/*
	 * 100us pll delay recommended for phy ver 2.0 and 3.0
	 * 25us pll delay recommended for phy ver 4.0
	 */
	phy_ver = dsi_phy_get_version(m_ctrl->phy);
	if (phy_ver <= DSI_PHY_VERSION_3_0)
		delay->pll_delay = 100;
	else
		delay->pll_delay = 25;

	delay->pll_delay = ((delay->pll_delay * esc_clk_rate_hz) / 1000000);
}

static int _dsi_display_dyn_update_clks(struct dsi_display *display,
					struct link_clk_freq *bkp_freq)
{
	int rc = 0, i;
	u8 ctrl_version;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	struct dsi_dyn_clk_caps *dyn_clk_caps;
	struct dsi_clk_link_set *enable_clk;

	m_ctrl = &display->ctrl[display->clk_master_idx];
	dyn_clk_caps = &(display->panel->dyn_clk_caps);
	ctrl_version = m_ctrl->ctrl->version;

	enable_clk = &display->clock_info.pll_clks;

	dsi_clk_prepare_enable(enable_clk);

	dsi_display_phy_configure(display, false);

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;
		rc = dsi_clk_set_byte_clk_rate(display->dsi_clk_handle,
				ctrl->ctrl->clk_freq.byte_clk_rate,
				ctrl->ctrl->clk_freq.byte_intf_clk_rate, i);
		if (rc) {
			DSI_ERR("failed to set byte rate for index:%d\n", i);
			goto recover_byte_clk;
		}
		rc = dsi_clk_set_pixel_clk_rate(display->dsi_clk_handle,
				   ctrl->ctrl->clk_freq.pix_clk_rate, i);
		if (rc) {
			DSI_ERR("failed to set pix rate for index:%d\n", i);
			goto recover_pix_clk;
		}
	}

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (ctrl == m_ctrl)
			continue;
		dsi_phy_dynamic_refresh_trigger(ctrl->phy, false);
	}
	dsi_phy_dynamic_refresh_trigger(m_ctrl->phy, true);

	/*
	 * Don't wait for dynamic refresh done for dsi ctrl greater than 2.5
	 * and with constant fps, as dynamic refresh will applied with
	 * next mdp intf ctrl flush.
	 */
	if ((ctrl_version >= DSI_CTRL_VERSION_2_5) &&
			(dyn_clk_caps->maintain_const_fps))
		return 0;

	/* wait for dynamic refresh done */
	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_wait4dynamic_refresh_done(ctrl->ctrl);
		if (rc) {
			DSI_ERR("wait4dynamic refresh failed for dsi:%d\n", i);
			goto recover_pix_clk;
		} else {
			DSI_INFO("dynamic refresh done on dsi: %s\n",
				i ? "slave" : "master");
		}
	}

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		dsi_phy_dynamic_refresh_clear(ctrl->phy);
	}

	if (rc)
		DSI_ERR("could not switch back to src clks %d\n", rc);

	dsi_clk_disable_unprepare(enable_clk);

	return rc;

recover_pix_clk:
	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;
		dsi_clk_set_pixel_clk_rate(display->dsi_clk_handle,
					   bkp_freq->pix_clk_rate, i);
	}

recover_byte_clk:
	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;
		dsi_clk_set_byte_clk_rate(display->dsi_clk_handle,
					bkp_freq->byte_clk_rate,
					bkp_freq->byte_intf_clk_rate, i);
	}

	return rc;
}

static int dsi_display_dynamic_clk_switch_vid(struct dsi_display *display,
					  struct dsi_display_mode *mode)
{
	int rc = 0, mask, i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	struct dsi_dyn_clk_delay delay;
	struct link_clk_freq bkp_freq;

	dsi_panel_acquire_panel_lock(display->panel);

	m_ctrl = &display->ctrl[display->clk_master_idx];

	dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_ON);

	/* mask PLL unlock, FIFO overflow and underflow errors */
	mask = BIT(DSI_PLL_UNLOCK_ERR) | BIT(DSI_FIFO_UNDERFLOW) |
		BIT(DSI_FIFO_OVERFLOW);
	dsi_display_mask_ctrl_error_interrupts(display, mask, true);

	/* update the phy timings based on new mode */
	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		dsi_phy_update_phy_timings(ctrl->phy, &display->config);
	}

	/* back up existing rates to handle failure case */
	bkp_freq.byte_clk_rate = m_ctrl->ctrl->clk_freq.byte_clk_rate;
	bkp_freq.byte_intf_clk_rate = m_ctrl->ctrl->clk_freq.byte_intf_clk_rate;
	bkp_freq.pix_clk_rate = m_ctrl->ctrl->clk_freq.pix_clk_rate;
	bkp_freq.esc_clk_rate = m_ctrl->ctrl->clk_freq.esc_clk_rate;

	rc = dsi_display_update_dsi_bitrate(display, mode->timing.clk_rate_hz);
	if (rc) {
		DSI_ERR("failed set link frequencies %d\n", rc);
		goto exit;
	}

	/* calculate pipe delays */
	_dsi_display_calc_pipe_delay(display, &delay, mode);

	/* configure dynamic refresh ctrl registers */
	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->phy)
			continue;
		if (ctrl == m_ctrl)
			dsi_phy_config_dynamic_refresh(ctrl->phy, &delay, true);
		else
			dsi_phy_config_dynamic_refresh(ctrl->phy, &delay,
						       false);
	}

	rc = _dsi_display_dyn_update_clks(display, &bkp_freq);

exit:
	dsi_display_mask_ctrl_error_interrupts(display, mask, false);

	dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS,
			     DSI_CLK_OFF);

	/* store newly calculated phy timings in mode private info */
	dsi_phy_dyn_refresh_cache_phy_timings(m_ctrl->phy,
					      mode->priv_info->phy_timing_val,
					      mode->priv_info->phy_timing_len);

	dsi_panel_release_panel_lock(display->panel);

	return rc;
}

static int dsi_display_dynamic_clk_configure_cmd(struct dsi_display *display,
		int clk_rate)
{
	int rc = 0;

	if (clk_rate <= 0) {
		DSI_ERR("%s: bitrate should be greater than 0\n", __func__);
		return -EINVAL;
	}

	if (clk_rate == display->cached_clk_rate) {
		DSI_INFO("%s: ignore duplicated DSI clk setting\n", __func__);
		return rc;
	}

	display->cached_clk_rate = clk_rate;

	rc = dsi_display_update_dsi_bitrate(display, clk_rate);
	if (!rc) {
		DSI_DEBUG("%s: bit clk is ready to be configured to '%d'\n",
				__func__, clk_rate);
		atomic_set(&display->clkrate_change_pending, 1);
	} else {
		DSI_ERR("%s: Failed to prepare to configure '%d'. rc = %d\n",
				__func__, clk_rate, rc);
		/* Caching clock failed, so don't go on doing so. */
		atomic_set(&display->clkrate_change_pending, 0);
		display->cached_clk_rate = 0;
	}

	return rc;
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
	struct dsi_dyn_clk_caps *dyn_clk_caps;

	if (!display || !dsi_mode || !display->panel) {
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}
	timing = &dsi_mode->timing;

	dsi_panel_get_dfps_caps(display->panel, &dfps_caps);
	dyn_clk_caps = &(display->panel->dyn_clk_caps);
	if (!dfps_caps.dfps_support && !dyn_clk_caps->maintain_const_fps) {
		DSI_ERR("dfps or constant fps not supported\n");
		return -ENOTSUPP;
	}

	if (dfps_caps.type == DSI_DFPS_IMMEDIATE_CLK) {
		DSI_ERR("dfps clock method not supported\n");
		return -ENOTSUPP;
	}

	/* For split DSI, update the clock master first */

	DSI_DEBUG("configuring seamless dynamic fps\n\n");
	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY);

	m_ctrl = &display->ctrl[display->clk_master_idx];
	rc = dsi_ctrl_async_timing_update(m_ctrl->ctrl, timing);
	if (rc) {
		DSI_ERR("[%s] failed to dfps update host_%d, rc=%d\n",
				display->name, i, rc);
		goto error;
	}

	/* Update the rest of the controllers */
	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || (ctrl == m_ctrl))
			continue;

		rc = dsi_ctrl_async_timing_update(ctrl->ctrl, timing);
		if (rc) {
			DSI_ERR("[%s] failed to dfps update host_%d, rc=%d\n",
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
	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT);
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
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (!a_total || !new_fps) {
		DSI_ERR("Invalid pixel total or new fps in mode request\n");
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

	DSI_DEBUG("fps %u a %u b %u b_fp %u new_fp %d\n",
			new_fps, a_total, b_total, b_fp, b_fp_new);

	if (b_fp_new < 0) {
		DSI_ERR("Invalid new_hfp calcluated%d\n", b_fp_new);
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
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}
	m_ctrl = display->ctrl[display->clk_master_idx].ctrl;

	dsi_panel_get_dfps_caps(display->panel, &dfps_caps);
	if (!dfps_caps.dfps_support) {
		DSI_ERR("dfps not supported by panel\n");
		return -EINVAL;
	}

	per_ctrl_mode = *adj_mode;
	adjust_timing_by_ctrl_count(display, &per_ctrl_mode);

	if (!curr_refresh_rate) {
		if (!dsi_display_is_seamless_dfps_possible(display,
				&per_ctrl_mode, dfps_caps.type)) {
			DSI_ERR("seamless dynamic fps not supported for mode\n");
			return -EINVAL;
		}
		if (display->panel->cur_mode) {
			curr_refresh_rate =
				display->panel->cur_mode->timing.refresh_rate;
		} else {
			DSI_ERR("cur_mode is not initialized\n");
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
				dsi_h_total_dce(timing),
				DSI_V_TOTAL(timing),
				timing->v_front_porch,
				&adj_mode->timing.v_front_porch);
		SDE_EVT32(SDE_EVTLOG_FUNC_CASE1, DSI_DFPS_IMMEDIATE_VFP,
			curr_refresh_rate, timing->refresh_rate,
			timing->v_front_porch, adj_mode->timing.v_front_porch);
		break;

	case DSI_DFPS_IMMEDIATE_HFP:
		rc = dsi_display_dfps_calc_front_porch(
				curr_refresh_rate,
				timing->refresh_rate,
				DSI_V_TOTAL(timing),
				dsi_h_total_dce(timing),
				timing->h_front_porch,
				&adj_mode->timing.h_front_porch);
		SDE_EVT32(SDE_EVTLOG_FUNC_CASE2, DSI_DFPS_IMMEDIATE_HFP,
			curr_refresh_rate, timing->refresh_rate,
			timing->h_front_porch, adj_mode->timing.h_front_porch);
		if (!rc)
			adj_mode->timing.h_front_porch *= display->ctrl_count;
		break;

	default:
		DSI_ERR("Unsupported DFPS mode %d\n", dfps_caps.type);
		rc = -ENOTSUPP;
	}

	return rc;
}

static bool dsi_display_validate_mode_seamless(struct dsi_display *display,
		struct dsi_display_mode *adj_mode)
{
	int rc = 0;

	if (!display || !adj_mode) {
		DSI_ERR("Invalid params\n");
		return false;
	}

	/* Currently the only seamless transition is dynamic fps */
	rc = dsi_display_get_dfps_timing(display, adj_mode, 0);
	if (rc) {
		DSI_DEBUG("Dynamic FPS not supported for seamless\n");
	} else {
		DSI_DEBUG("Mode switch is seamless Dynamic FPS\n");
		adj_mode->dsi_mode_flags |= DSI_MODE_FLAG_DFPS |
				DSI_MODE_FLAG_VBLANK_PRE_MODESET;
	}

	return rc;
}

static void dsi_display_validate_dms_fps(struct dsi_display_mode *cur_mode,
		struct dsi_display_mode *to_mode)
{
	u32 cur_fps, to_fps;
	u32 cur_h_active, to_h_active;
	u32 cur_v_active, to_v_active;

	cur_fps = cur_mode->timing.refresh_rate;
	to_fps = to_mode->timing.refresh_rate;
	cur_h_active = cur_mode->timing.h_active;
	cur_v_active = cur_mode->timing.v_active;
	to_h_active = to_mode->timing.h_active;
	to_v_active = to_mode->timing.v_active;

	if ((cur_h_active == to_h_active) && (cur_v_active == to_v_active) &&
			(cur_fps != to_fps)) {
		to_mode->dsi_mode_flags |= DSI_MODE_FLAG_DMS_FPS;
		DSI_DEBUG("DMS Modeset with FPS change\n");
	} else {
		to_mode->dsi_mode_flags &= ~DSI_MODE_FLAG_DMS_FPS;
	}
}


static int dsi_display_set_mode_sub(struct dsi_display *display,
				    struct dsi_display_mode *mode,
				    u32 flags)
{
	int rc = 0, clk_rate = 0;
	int i;
	struct dsi_display_ctrl *ctrl;
	struct dsi_display_ctrl *mctrl;
	struct dsi_display_mode_priv_info *priv_info;
	bool commit_phy_timing = false;
	struct dsi_dyn_clk_caps *dyn_clk_caps;

	priv_info = mode->priv_info;
	if (!priv_info) {
		DSI_ERR("[%s] failed to get private info of the display mode\n",
			display->name);
		return -EINVAL;
	}

	SDE_EVT32(mode->dsi_mode_flags, display->panel->panel_mode);

	if (mode->dsi_mode_flags & DSI_MODE_FLAG_POMS_TO_VID)
		display->panel->panel_mode = DSI_OP_VIDEO_MODE;
	else if (mode->dsi_mode_flags & DSI_MODE_FLAG_POMS_TO_CMD)
		display->panel->panel_mode = DSI_OP_CMD_MODE;

	rc = dsi_panel_get_host_cfg_for_mode(display->panel,
					     mode,
					     &display->config);
	if (rc) {
		DSI_ERR("[%s] failed to get host config for mode, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	memcpy(&display->config.lane_map, &display->lane_map,
	       sizeof(display->lane_map));

	mctrl = &display->ctrl[display->clk_master_idx];
	dyn_clk_caps = &(display->panel->dyn_clk_caps);

	if (mode->dsi_mode_flags &
			(DSI_MODE_FLAG_DFPS | DSI_MODE_FLAG_VRR)) {
		display_for_each_ctrl(i, display) {
			ctrl = &display->ctrl[i];

			if (!ctrl->ctrl || (ctrl != mctrl))
				continue;

			ctrl->ctrl->hw.ops.set_timing_db(&ctrl->ctrl->hw,
					true);
			dsi_phy_dynamic_refresh_clear(ctrl->phy);

			if ((ctrl->ctrl->version >= DSI_CTRL_VERSION_2_5) &&
					(dyn_clk_caps->maintain_const_fps)) {
				dsi_phy_dynamic_refresh_trigger_sel(ctrl->phy,
						true);
			}
		}
		rc = dsi_display_dfps_update(display, mode);
		if (rc) {
			DSI_ERR("[%s]DSI dfps update failed, rc=%d\n",
					display->name, rc);
			goto error;
		}
		display_for_each_ctrl(i, display) {
			ctrl = &display->ctrl[i];
			rc = dsi_ctrl_update_host_config(ctrl->ctrl,
				&display->config, mode, mode->dsi_mode_flags,
					display->dsi_clk_handle);
			if (rc) {
				DSI_ERR("failed to update ctrl config\n");
				goto error;
			}
		}
		if (priv_info->phy_timing_len) {
			display_for_each_ctrl(i, display) {
				ctrl = &display->ctrl[i];
				rc = dsi_phy_set_timing_params(ctrl->phy,
						priv_info->phy_timing_val,
						priv_info->phy_timing_len,
						commit_phy_timing);
				if (rc)
					DSI_ERR("Fail to add timing params\n");
			}
		}
		if (!(mode->dsi_mode_flags & DSI_MODE_FLAG_DYN_CLK))
			return rc;
	}

	if (mode->dsi_mode_flags & DSI_MODE_FLAG_DYN_CLK) {
		if (display->panel->panel_mode == DSI_OP_VIDEO_MODE) {
			rc = dsi_display_dynamic_clk_switch_vid(display, mode);
			if (rc)
				DSI_ERR("dynamic clk change failed %d\n", rc);
			/*
			 * skip rest of the opearations since
			 * dsi_display_dynamic_clk_switch_vid() already takes
			 * care of them.
			 */
			return rc;
		} else if (display->panel->panel_mode == DSI_OP_CMD_MODE) {
			clk_rate = mode->timing.clk_rate_hz;
			rc = dsi_display_dynamic_clk_configure_cmd(display,
					clk_rate);
			if (rc) {
				DSI_ERR("Failed to configure dynamic clk\n");
				return rc;
			}
		}
	}

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_update_host_config(ctrl->ctrl, &display->config,
				mode, mode->dsi_mode_flags,
				display->dsi_clk_handle);
		if (rc) {
			DSI_ERR("[%s] failed to update ctrl config, rc=%d\n",
			       display->name, rc);
			goto error;
		}
	}

	if ((mode->dsi_mode_flags & DSI_MODE_FLAG_DMS) &&
			(display->panel->panel_mode == DSI_OP_CMD_MODE)) {
		u64 cur_bitclk = display->panel->cur_mode->timing.clk_rate_hz;
		u64 to_bitclk = mode->timing.clk_rate_hz;
		commit_phy_timing = true;

		/* No need to set clkrate pending flag if clocks are same */
		if ((!cur_bitclk && !to_bitclk) || (cur_bitclk != to_bitclk))
			atomic_set(&display->clkrate_change_pending, 1);

		dsi_display_validate_dms_fps(display->panel->cur_mode, mode);
	}

	if (priv_info->phy_timing_len) {
		display_for_each_ctrl(i, display) {
			ctrl = &display->ctrl[i];
			 rc = dsi_phy_set_timing_params(ctrl->phy,
				priv_info->phy_timing_val,
				priv_info->phy_timing_len,
				commit_phy_timing);
			if (rc)
				DSI_ERR("failed to add DSI PHY timing params\n");
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
		DSI_ERR("invalid display\n");
		return -EINVAL;
	}

	if (!display->panel_node && !display->fw)
		return 0;

	mutex_lock(&display->display_lock);

	display->parser = dsi_parser_get(&display->pdev->dev);
	if (display->fw && display->parser)
		display->parser_node = dsi_parser_get_head_node(
				display->parser, display->fw->data,
				display->fw->size);

	rc = dsi_display_parse_dt(display);
	if (rc) {
		DSI_ERR("[%s] failed to parse dt, rc=%d\n", display->name, rc);
		goto error;
	}

	rc = dsi_display_res_init(display);
	if (rc) {
		DSI_ERR("[%s] failed to initialize resources, rc=%d\n",
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
		DSI_ERR("invalid display\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = dsi_display_res_deinit(display);
	if (rc)
		DSI_ERR("[%s] failed to deinitialize resource, rc=%d\n",
		       display->name, rc);

	mutex_unlock(&display->display_lock);

	return rc;
}

/**
 * dsi_display_cont_splash_res_disable() - Disable resource votes added in probe
 * @dsi_display:    Pointer to dsi display
 * Returns:     Zero on success
 */
int dsi_display_cont_splash_res_disable(void *dsi_display)
{
	struct dsi_display *display = dsi_display;
	int rc = 0;

	/* Remove the panel vote that was added during dsi display probe */
	rc = dsi_pwr_enable_regulator(&display->panel->power_info, false);
	if (rc)
		DSI_ERR("[%s] failed to disable vregs, rc=%d\n",
				display->panel->name, rc);

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
		DSI_ERR("invalid input display param\n");
		return -EINVAL;
	}

	rc = pm_runtime_get_sync(display->drm_dev->dev);
	if (rc < 0) {
		DSI_ERR("failed to vote gdsc for continuous splash, rc=%d\n",
							rc);
		return rc;
	}

	mutex_lock(&display->display_lock);

	display->is_cont_splash_enabled = true;

	/* Update splash status for clock manager */
	dsi_display_clk_mngr_update_splash_status(display->clk_mngr,
				display->is_cont_splash_enabled);
	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY, display->is_cont_splash_enabled);

	/* Set up ctrl isr before enabling core clk */
	dsi_display_ctrl_isr_configure(display, true);

	/* Vote for Core clk and link clk. Votes on ctrl and phy
	 * regulator are inplicit from  pre clk on callback
	 */
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);
	if (rc) {
		DSI_ERR("[%s] failed to enable DSI link clocks, rc=%d\n",
		       display->name, rc);
		goto clk_manager_update;
	}

	mutex_unlock(&display->display_lock);

	/* Set the current brightness level */
	dsi_panel_bl_handoff(display->panel);

	return rc;

clk_manager_update:
	dsi_display_ctrl_isr_configure(display, false);
	/* Update splash status for clock manager */
	dsi_display_clk_mngr_update_splash_status(display->clk_mngr,
				false);
	pm_runtime_put_sync(display->drm_dev->dev);
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
		DSI_ERR("[%s] failed to disable DSI link clocks, rc=%d\n",
		       display->name, rc);

	pm_runtime_put_sync(display->drm_dev->dev);

	display->is_cont_splash_enabled = false;
	/* Update splash status for clock manager */
	dsi_display_clk_mngr_update_splash_status(display->clk_mngr,
				display->is_cont_splash_enabled);

	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT, display->is_cont_splash_enabled);
	return rc;
}

static int dsi_display_force_update_dsi_clk(struct dsi_display *display)
{
	int rc = 0, i = 0;
	struct dsi_display_ctrl *ctrl;

	/*
	 * The force update dsi clock, is the only clock update function that toggles the state of
	 * DSI clocks without any ref count protection. With the addition of ASYNC command wait,
	 * there is a need for adding a check for any queued waits before updating these clocks.
	 */
	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl || !(ctrl->ctrl->post_tx_queued))
			continue;
		flush_workqueue(display->post_cmd_tx_workq);
		cancel_work_sync(&ctrl->ctrl->post_cmd_tx_work);
		ctrl->ctrl->post_tx_queued = false;
	}

	rc = dsi_display_link_clk_force_update_ctrl(display->dsi_clk_handle);

	if (!rc) {
		DSI_DEBUG("dsi bit clk has been configured to %d\n",
			display->cached_clk_rate);

		atomic_set(&display->clkrate_change_pending, 0);
	} else {
		DSI_ERR("Failed to configure dsi bit clock '%d'. rc = %d\n",
			display->cached_clk_rate, rc);
	}

	return rc;
}

static int dsi_display_validate_split_link(struct dsi_display *display)
{
	int i, rc = 0;
	struct dsi_display_ctrl *ctrl;
	struct dsi_host_common_cfg *host = &display->panel->host_config;

	if (!host->split_link.enabled)
		return 0;

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl->split_link_supported) {
			DSI_ERR("[%s] split link is not supported by hw\n",
				display->name);
			rc = -ENOTSUPP;
			goto error;
		}

		set_bit(DSI_PHY_SPLIT_LINK, ctrl->phy->hw.feature_map);
		host->split_link.panel_mode = display->panel->panel_mode;
	}

	DSI_DEBUG("Split link is enabled\n");
	return 0;

error:
	host->split_link.enabled = false;
	return rc;
}

static int dsi_display_get_io_resources(struct msm_io_res *io_res, void *data)
{
	int rc = 0;
	struct dsi_display *display;
	struct platform_device *pdev;
	int te_gpio, avdd_gpio;

	if (!data)
		return -EINVAL;

	display = (struct dsi_display *)data;

	pdev = display->pdev;
	if (!pdev)
		return -EINVAL;

	rc = dsi_ctrl_get_io_resources(io_res);
	if (rc)
		return rc;

	rc = dsi_phy_get_io_resources(io_res);
	if (rc)
		return rc;

	rc = dsi_panel_get_io_resources(display->panel, io_res);
	if (rc)
		return rc;

	te_gpio = of_get_named_gpio(pdev->dev.of_node, "qcom,platform-te-gpio", 0);
	if (gpio_is_valid(te_gpio)) {
		rc = msm_dss_get_gpio_io_mem(te_gpio, &io_res->mem);
		if (rc) {
			DSI_ERR("[%s] failed to retrieve the te gpio address\n",
					display->panel->name);
			return rc;
		}
	}

	avdd_gpio = of_get_named_gpio(pdev->dev.of_node,
			"qcom,avdd-regulator-gpio", 0);
	if (gpio_is_valid(avdd_gpio)) {
		rc = msm_dss_get_gpio_io_mem(avdd_gpio, &io_res->mem);
		if (rc)
			DSI_ERR("[%s] failed to retrieve the avdd gpio address\n",
					display->panel->name);
	}

	return rc;
}

static int dsi_display_pre_release(void *data)
{
	struct dsi_display *display;
	int i;

	if (!data)
		return -EINVAL;

	display = (struct dsi_display *)data;
	mutex_lock(&display->display_lock);
	display->hw_ownership = false;
	mutex_unlock(&display->display_lock);

	/* flush work queues */
	display_for_each_ctrl(i, display) {
		struct dsi_display_ctrl *ctrl = &display->ctrl[i];

		if (!ctrl->ctrl || !(ctrl->ctrl->post_tx_queued))
			continue;
		flush_workqueue(display->post_cmd_tx_workq);
		cancel_work_sync(&ctrl->ctrl->post_cmd_tx_work);
		ctrl->ctrl->post_tx_queued = false;
	}

	dsi_display_ctrl_irq_update(display, false);

	return 0;
}

static int dsi_display_pre_acquire(void *data)
{
	struct dsi_display *display;

	if (!data)
		return -EINVAL;

	display = (struct dsi_display *)data;
	mutex_lock(&display->display_lock);
	display->hw_ownership = true;
	mutex_unlock(&display->display_lock);

	dsi_display_ctrl_irq_update((struct dsi_display *)data, true);

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
	void *handle = NULL;
	struct platform_device *pdev = to_platform_device(dev);
	char *client1 = "dsi_clk_client";
	char *client2 = "mdp_event_client";
	struct msm_vm_ops vm_event_ops = {
		.vm_get_io_resources = dsi_display_get_io_resources,
		.vm_pre_hw_release = dsi_display_pre_release,
		.vm_post_hw_acquire = dsi_display_pre_acquire,
	};
	int i, rc = 0;

	if (!dev || !pdev || !master) {
		DSI_ERR("invalid param(s), dev %pK, pdev %pK, master %pK\n",
				dev, pdev, master);
		return -EINVAL;
	}

	drm = dev_get_drvdata(master);
	display = platform_get_drvdata(pdev);
	if (!drm || !display) {
		DSI_ERR("invalid param(s), drm %pK, display %pK\n",
				drm, display);
		return -EINVAL;
	}
	if (!display->panel_node && !display->fw)
		return 0;

	if (!display->fw)
		display->name = display->panel_node->name;

	/* defer bind if ext bridge driver is not loaded */
	if (display->panel && display->panel->host_config.ext_bridge_mode) {
		for (i = 0; i < display->ext_bridge_cnt; i++) {
			if (!of_drm_find_bridge(
					display->ext_bridge[i].node_of)) {
				DSI_DEBUG("defer for bridge[%d] %s\n", i,
				  display->ext_bridge[i].node_of->full_name);
				return -EPROBE_DEFER;
			}
		}
	}

	mutex_lock(&display->display_lock);

	rc = dsi_display_validate_split_link(display);
	if (rc) {
		DSI_ERR("[%s] split link validation failed, rc=%d\n",
						 display->name, rc);
		goto error;
	}

	rc = dsi_display_debugfs_init(display);
	if (rc) {
		DSI_ERR("[%s] debugfs init failed, rc=%d\n", display->name, rc);
		goto error;
	}

	atomic_set(&display->clkrate_change_pending, 0);
	display->cached_clk_rate = 0;

	memset(&info, 0x0, sizeof(info));

	display_for_each_ctrl(i, display) {
		display_ctrl = &display->ctrl[i];
		rc = dsi_ctrl_drv_init(display_ctrl->ctrl, display->root);
		if (rc) {
			DSI_ERR("[%s] failed to initialize ctrl[%d], rc=%d\n",
			       display->name, i, rc);
			goto error_ctrl_deinit;
		}
		display_ctrl->ctrl->horiz_index = i;

		rc = dsi_phy_drv_init(display_ctrl->phy);
		if (rc) {
			DSI_ERR("[%s] Failed to initialize phy[%d], rc=%d\n",
				display->name, i, rc);
			(void)dsi_ctrl_drv_deinit(display_ctrl->ctrl);
			goto error_ctrl_deinit;
		}

		display_ctrl->ctrl->post_cmd_tx_workq = display->post_cmd_tx_workq;
		memcpy(&info.c_clks[i],
				(&display_ctrl->ctrl->clk_info.core_clks),
				sizeof(struct dsi_core_clk_info));
		memcpy(&info.l_hs_clks[i],
				(&display_ctrl->ctrl->clk_info.hs_link_clks),
				sizeof(struct dsi_link_hs_clk_info));
		memcpy(&info.l_lp_clks[i],
				(&display_ctrl->ctrl->clk_info.lp_link_clks),
				sizeof(struct dsi_link_lp_clk_info));

		info.c_clks[i].drm = drm;
		info.ctrl_index[i] = display_ctrl->ctrl->cell_index;
	}

	info.pre_clkoff_cb = dsi_pre_clkoff_cb;
	info.pre_clkon_cb = dsi_pre_clkon_cb;
	info.post_clkoff_cb = dsi_post_clkoff_cb;
	info.post_clkon_cb = dsi_post_clkon_cb;
	info.phy_config_cb = dsi_display_phy_configure;
	info.phy_pll_toggle_cb = dsi_display_phy_pll_toggle;
	info.priv_data = display;
	info.master_ndx = display->clk_master_idx;
	info.dsi_ctrl_count = display->ctrl_count;
	snprintf(info.name, MAX_STRING_LEN,
			"DSI_MNGR-%s", display->name);

	display->clk_mngr = dsi_display_clk_mngr_register(&info);
	if (IS_ERR_OR_NULL(display->clk_mngr)) {
		rc = PTR_ERR(display->clk_mngr);
		display->clk_mngr = NULL;
		DSI_ERR("dsi clock registration failed, rc = %d\n", rc);
		goto error_ctrl_deinit;
	}

	handle = dsi_register_clk_handle(display->clk_mngr, client1);
	if (IS_ERR_OR_NULL(handle)) {
		rc = PTR_ERR(handle);
		DSI_ERR("failed to register %s client, rc = %d\n",
		       client1, rc);
		goto error_clk_deinit;
	} else {
		display->dsi_clk_handle = handle;
	}

	handle = dsi_register_clk_handle(display->clk_mngr, client2);
	if (IS_ERR_OR_NULL(handle)) {
		rc = PTR_ERR(handle);
		DSI_ERR("failed to register %s client, rc = %d\n",
		       client2, rc);
		goto error_clk_client_deinit;
	} else {
		display->mdp_clk_handle = handle;
	}

	clk_cb.priv = display;
	clk_cb.dsi_clk_cb = dsi_display_clk_ctrl_cb;

	display_for_each_ctrl(i, display) {
		display_ctrl = &display->ctrl[i];

		rc = dsi_ctrl_clk_cb_register(display_ctrl->ctrl, &clk_cb);
		if (rc) {
			DSI_ERR("[%s] failed to register ctrl clk_cb[%d], rc=%d\n",
			       display->name, i, rc);
			goto error_ctrl_deinit;
		}

		rc = dsi_phy_clk_cb_register(display_ctrl->phy, &clk_cb);
		if (rc) {
			DSI_ERR("[%s] failed to register phy clk_cb[%d], rc=%d\n",
			       display->name, i, rc);
			goto error_ctrl_deinit;
		}
	}

	dsi_display_update_byte_intf_div(display);
	rc = dsi_display_mipi_host_init(display);
	if (rc) {
		DSI_ERR("[%s] failed to initialize mipi host, rc=%d\n",
		       display->name, rc);
		goto error_ctrl_deinit;
	}

	rc = dsi_panel_drv_init(display->panel, &display->host);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			DSI_ERR("[%s] failed to initialize panel driver, rc=%d\n",
			       display->name, rc);
		goto error_host_deinit;
	}

	DSI_INFO("Successfully bind display panel '%s %s'\n", display->name,
			display->panel->te_using_watchdog_timer ? "as sim panel" : "");
	display->drm_dev = drm;

	display_for_each_ctrl(i, display) {
		display_ctrl = &display->ctrl[i];
		if (!display_ctrl->phy || !display_ctrl->ctrl)
			continue;

		display_ctrl->ctrl->drm_dev = drm;
		rc = dsi_phy_set_clk_freq(display_ctrl->phy,
				&display_ctrl->ctrl->clk_freq);
		if (rc) {
			DSI_ERR("[%s] failed to set phy clk freq, rc=%d\n",
					display->name, rc);
			goto error;
		}
	}


	msm_register_vm_event(master, dev, &vm_event_ops, (void *)display);

	rc = mi_disp_feature_attach_display(display,
			mi_get_disp_id(display->display_type), MI_INTF_DSI);
	if (rc) {
		DISP_ERROR("failed to attach %s display(%s intf)\n",
			get_disp_id_name(mi_get_disp_id(display->display_type)),
			get_disp_intf_type_name(MI_INTF_DSI));
	}

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
		dsi_ctrl_put(display_ctrl->ctrl);
		dsi_phy_put(display_ctrl->phy);
	}
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

	if (!dev || !pdev || !master) {
		DSI_ERR("invalid param(s)\n");
		return;
	}

	display = platform_get_drvdata(pdev);
	if (!display || !display->panel_node) {
		DSI_ERR("invalid display\n");
		return;
	}

	mutex_lock(&display->display_lock);

	rc = mi_disp_feature_detach_display(display,
			mi_get_disp_id(display->display_type), MI_INTF_DSI);
	if (rc) {
		DISP_ERROR("failed to detach %s display(%s intf)\n",
			get_disp_id_name(mi_get_disp_id(display->display_type)),
			get_disp_intf_type_name(MI_INTF_DSI));
	}

	rc = dsi_display_mipi_host_deinit(display);
	if (rc)
		DSI_ERR("[%s] failed to deinit mipi hosts, rc=%d\n",
		       display->name,
		       rc);

	display_for_each_ctrl(i, display) {
		display_ctrl = &display->ctrl[i];

		rc = dsi_phy_drv_deinit(display_ctrl->phy);
		if (rc)
			DSI_ERR("[%s] failed to deinit phy%d driver, rc=%d\n",
			       display->name, i, rc);

		display->ctrl->ctrl->post_cmd_tx_workq = NULL;
		rc = dsi_ctrl_drv_deinit(display_ctrl->ctrl);
		if (rc)
			DSI_ERR("[%s] failed to deinit ctrl%d driver, rc=%d\n",
			       display->name, i, rc);
	}

	atomic_set(&display->clkrate_change_pending, 0);
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
		DSI_ERR("device init failed, rc=%d\n", rc);
		goto end;
	}

	/*
	 * Vote on panel regulator is added to make sure panel regulators
	 * are ON for cont-splash enabled usecase.
	 * This panel regulator vote will be removed only in:
	 *	1) device suspend when cont-splash is enabled.
	 *	2) cont_splash_res_disable() when cont-splash is disabled.
	 * For GKI, adding this vote will make sure that sync_state
	 * kernel driver doesn't disable the panel regulators after
	 * dsi probe is complete.
	 */
	if (display->panel) {
		rc = dsi_pwr_enable_regulator(&display->panel->power_info,
								true);
		if (rc) {
			DSI_ERR("[%s] failed to enable vregs, rc=%d\n",
					display->panel->name, rc);
			return rc;
		}
	}

	rc = component_add(&pdev->dev, &dsi_display_comp_ops);
	if (rc)
		DSI_ERR("component add failed, rc=%d\n", rc);

	DSI_DEBUG("component add success: %s\n", display->name);
end:
	return rc;
}

static void dsi_display_firmware_display(const struct firmware *fw,
				void *context)
{
	struct dsi_display *display = context;

	if (fw) {
		DSI_INFO("reading data from firmware, size=%zd\n",
			fw->size);

		display->fw = fw;

		if (!strcmp(display->display_type, "primary"))
			display->name = "dsi_firmware_display";

		else if (!strcmp(display->display_type, "secondary"))
			display->name = "dsi_firmware_display_secondary";

	} else {
		DSI_INFO("no firmware available, fallback to device node\n");
	}

	if (dsi_display_init(display))
		return;

	DSI_DEBUG("success\n");
}

int dsi_display_dev_probe(struct platform_device *pdev)
{
	struct dsi_display *display = NULL;
	struct device_node *node = NULL, *panel_node = NULL, *mdp_node = NULL;
	int rc = 0, index = DSI_PRIMARY;
	bool firm_req = false;
	struct dsi_display_boot_param *boot_disp;

	if (!pdev || !pdev->dev.of_node) {
		DSI_ERR("pdev not found\n");
		rc = -ENODEV;
		goto end;
	}

	display = devm_kzalloc(&pdev->dev, sizeof(*display), GFP_KERNEL);
	if (!display) {
		rc = -ENOMEM;
		goto end;
	}

	display->post_cmd_tx_workq = create_singlethread_workqueue(
			"dsi_post_cmd_tx_workq");
	if (!display->post_cmd_tx_workq)  {
		DSI_ERR("failed to create work queue\n");
		rc =  -EINVAL;
		goto end;
	}

	mdp_node = of_parse_phandle(pdev->dev.of_node, "qcom,mdp", 0);
	if (!mdp_node) {
		DSI_ERR("mdp_node not found\n");
		rc = -ENODEV;
		goto end;
	}

	display->trusted_vm_env = of_property_read_bool(mdp_node,
						"qcom,sde-trusted-vm-env");
	if (display->trusted_vm_env)
		DSI_INFO("Display enabled with trusted vm path\n");

	/* initialize panel id to UINT64_MAX */
	display->panel_id = ~0x0;

	display->display_type = of_get_property(pdev->dev.of_node,
				"label", NULL);
	if (!display->display_type)
		display->display_type = "primary";

	if (!strcmp(display->display_type, "secondary"))
		index = DSI_SECONDARY;

	boot_disp = &boot_displays[index];
	node = pdev->dev.of_node;
	if (boot_disp->boot_disp_en) {
		/* The panel name should be same as UEFI name index */
		panel_node = of_find_node_by_name(mdp_node, boot_disp->name);
		if (!panel_node)
			DSI_WARN("%s panel_node %s not found\n", display->display_type,
					boot_disp->name);
	} else {
		panel_node = of_parse_phandle(node,
				"qcom,dsi-default-panel", 0);
		if (!panel_node)
			DSI_WARN("%s default panel not found\n", display->display_type);
	}

	boot_disp->node = pdev->dev.of_node;
	boot_disp->disp = display;

	display->panel_node = panel_node;
	display->pdev = pdev;
	display->boot_disp = boot_disp;

	dsi_display_parse_cmdline_topology(display, index);

	platform_set_drvdata(pdev, display);

	if (!dsi_display_validate_res(display)) {
		rc = -EPROBE_DEFER;
		DSI_ERR("resources required for display probe not present: \
				rc=%d,panel-type=%s\n", rc, display->display_type);
		goto end;
	}

	/* initialize display in firmware callback */
	if (!(boot_displays[DSI_PRIMARY].boot_disp_en ||
			boot_displays[DSI_SECONDARY].boot_disp_en) &&
			IS_ENABLED(CONFIG_DSI_PARSER)) {
		if (!strcmp(display->display_type, "primary"))
			firm_req = !request_firmware_nowait(
				THIS_MODULE, 1, "dsi_prop",
				&pdev->dev, GFP_KERNEL, display,
				dsi_display_firmware_display);

		else if (!strcmp(display->display_type, "secondary"))
			firm_req = !request_firmware_nowait(
				THIS_MODULE, 1, "dsi_prop_sec",
				&pdev->dev, GFP_KERNEL, display,
				dsi_display_firmware_display);
	}

	if (!firm_req) {
		rc = dsi_display_init(display);
		if (rc)
			goto end;
	}

	return 0;
end:
	if (display->post_cmd_tx_workq) {
		destroy_workqueue(display->post_cmd_tx_workq);
	}

	if (display)
		devm_kfree(&pdev->dev, display);

	return rc;
}

int dsi_display_dev_remove(struct platform_device *pdev)
{
	int rc = 0, i = 0;
	struct dsi_display *display;
	struct dsi_display_ctrl *ctrl;

	if (!pdev) {
		DSI_ERR("Invalid device\n");
		return -EINVAL;
	}

	display = platform_get_drvdata(pdev);

	/* decrement ref count */
	of_node_put(display->panel_node);

	if (display->post_cmd_tx_workq) {
		flush_workqueue(display->post_cmd_tx_workq);
		destroy_workqueue(display->post_cmd_tx_workq);
		display->post_cmd_tx_workq = NULL;
		display_for_each_ctrl(i, display) {
			ctrl = &display->ctrl[i];
			if (!ctrl->ctrl)
				continue;
			ctrl->ctrl->post_cmd_tx_workq = NULL;
		}
	}

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

		if ((display && display->panel_node) ||
					(display && display->fw))
			count++;
	}

	return count;
}

int dsi_display_get_active_displays(void **display_array, u32 max_display_count)
{
	int index = 0, count = 0;

	if (!display_array || !max_display_count) {
		DSI_ERR("invalid params\n");
		return 0;
	}

	for (index = 0; index < MAX_DSI_ACTIVE_DISPLAY; index++) {
		struct dsi_display *display = boot_displays[index].disp;

		if ((display && display->panel_node) ||
					(display && display->fw))
			display_array[count++] = display;
	}

	return count;
}

void dsi_display_set_active_state(struct dsi_display *display, bool is_active)
{
	if (!display)
		return;

	mutex_lock(&display->display_lock);
	display->is_active = is_active;
	mutex_unlock(&display->display_lock);
}

int dsi_display_drm_bridge_init(struct dsi_display *display,
		struct drm_encoder *enc)
{
	int rc = 0;
	struct dsi_bridge *bridge;
	struct msm_drm_private *priv = NULL;

	if (!display || !display->drm_dev || !enc) {
		DSI_ERR("invalid param(s)\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	priv = display->drm_dev->dev_private;

	if (!priv) {
		DSI_ERR("Private data is not present\n");
		rc = -EINVAL;
		goto error;
	}

	if (display->bridge) {
		DSI_ERR("display is already initialize\n");
		goto error;
	}

	bridge = dsi_drm_bridge_init(display, display->drm_dev, enc);
	if (IS_ERR_OR_NULL(bridge)) {
		rc = PTR_ERR(bridge);
		DSI_ERR("[%s] brige init failed, %d\n", display->name, rc);
		goto error;
	}

	display->bridge = bridge;
	priv->bridges[priv->num_bridges++] = &bridge->base;

	if (display->tx_cmd_buf == NULL) {
		rc = dsi_host_alloc_cmd_tx_buffer(display);
		if (rc)
			DSI_ERR("failed to allocate cmd tx buffer memory\n");
	}

error:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_drm_bridge_deinit(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		DSI_ERR("Invalid params\n");
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
		struct drm_connector *connector, void *disp,
		const struct msm_resource_caps_info *avail_res)
{
	struct dsi_display *display = disp;
	struct drm_display_mode *pmode, *pt;
	int count;

	/* if there are modes defined in panel, ignore external modes */
	if (display->panel->num_timing_nodes)
		return dsi_connector_get_modes(connector, disp, avail_res);

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
		void *disp, const struct msm_resource_caps_info *avail_res)
{
	struct dsi_display *display = disp;
	enum drm_mode_status status;

	/* always do internal mode_valid check */
	status = dsi_conn_mode_valid(connector, mode, disp, avail_res);
	if (status != MODE_OK)
		return status;

	return display->ext_conn->helper_private->mode_valid(
			display->ext_conn, mode);
}

static int dsi_display_drm_ext_atomic_check(struct drm_connector *connector,
		void *disp,
		struct drm_atomic_state *state)
{
	struct dsi_display *display = disp;
	struct drm_connector_state *c_state;

	c_state = drm_atomic_get_new_connector_state(state, connector);

	return display->ext_conn->helper_private->atomic_check(
			display->ext_conn, state);
}

static int dsi_display_ext_get_info(struct drm_connector *connector,
	struct msm_display_info *info, void *disp)
{
	struct dsi_display *display;
	int i;

	if (!info || !disp) {
		DSI_ERR("invalid params\n");
		return -EINVAL;
	}

	display = disp;
	if (!display->panel) {
		DSI_ERR("invalid display panel\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	memset(info, 0, sizeof(struct msm_display_info));

	info->intf_type = DRM_MODE_CONNECTOR_DSI;
	info->num_of_h_tiles = display->ctrl_count;
	for (i = 0; i < info->num_of_h_tiles; i++)
		info->h_tile_instance[i] = display->ctrl[i].ctrl->cell_index;

	info->is_connected = connector->status != connector_status_disconnected;

	if (!strcmp(display->display_type, "primary"))
		info->display_type = SDE_CONNECTOR_PRIMARY;
	else if (!strcmp(display->display_type, "secondary"))
		info->display_type = SDE_CONNECTOR_SECONDARY;

	info->capabilities |= (MSM_DISPLAY_CAP_VID_MODE |
			MSM_DISPLAY_CAP_EDID | MSM_DISPLAY_CAP_HOT_PLUG);
	info->curr_panel_mode = MSM_DISPLAY_VIDEO_MODE;

	mutex_unlock(&display->display_lock);
	return 0;
}

static int dsi_display_ext_get_mode_info(struct drm_connector *connector,
	const struct drm_display_mode *drm_mode, struct msm_sub_mode *sub_mode,
	struct msm_mode_info *mode_info,
	void *display, const struct msm_resource_caps_info *avail_res)
{
	struct msm_display_topology *topology;

	if (!drm_mode || !mode_info ||
			!avail_res || !avail_res->max_mixer_width)
		return -EINVAL;

	memset(mode_info, 0, sizeof(*mode_info));
	mode_info->frame_rate = drm_mode_vrefresh(drm_mode);
	mode_info->vtotal = drm_mode->vtotal;

	topology = &mode_info->topology;
	topology->num_lm = (avail_res->max_mixer_width
			<= drm_mode->hdisplay) ? 2 : 1;
	topology->num_enc = 0;
	topology->num_intf = topology->num_lm;

	mode_info->comp_info.comp_type = MSM_DISPLAY_COMPRESSION_NONE;

	return 0;
}

static struct dsi_display_ext_bridge *dsi_display_ext_get_bridge(
		struct drm_bridge *bridge)
{
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct drm_connector *conn;
	struct drm_connector_list_iter conn_iter;
	struct sde_connector *sde_conn;
	struct dsi_display *display;
	struct dsi_display_ext_bridge *dsi_bridge = NULL;
	int i;

	if (!bridge || !bridge->encoder) {
		SDE_ERROR("invalid argument\n");
		return NULL;
	}

	priv = bridge->dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);

	drm_connector_list_iter_begin(sde_kms->dev, &conn_iter);
	drm_for_each_connector_iter(conn, &conn_iter) {
		sde_conn = to_sde_connector(conn);
		if (sde_conn->encoder == bridge->encoder) {
			display = sde_conn->display;
			display_for_each_ctrl(i, display) {
				if (display->ext_bridge[i].bridge == bridge) {
					dsi_bridge = &display->ext_bridge[i];
					break;
				}
			}
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	return dsi_bridge;
}

static void dsi_display_drm_ext_adjust_timing(
		const struct dsi_display *display,
		struct drm_display_mode *mode)
{
	mode->hdisplay /= display->ctrl_count;
	mode->hsync_start /= display->ctrl_count;
	mode->hsync_end /= display->ctrl_count;
	mode->htotal /= display->ctrl_count;
	mode->hskew /= display->ctrl_count;
	mode->clock /= display->ctrl_count;
}

static enum drm_mode_status dsi_display_drm_ext_bridge_mode_valid(
		struct drm_bridge *bridge,
		const struct drm_display_info *info,
		const struct drm_display_mode *mode)
{
	struct dsi_display_ext_bridge *ext_bridge;
	struct drm_display_mode tmp;

	ext_bridge = dsi_display_ext_get_bridge(bridge);
	if (!ext_bridge)
		return MODE_ERROR;

	tmp = *mode;
	dsi_display_drm_ext_adjust_timing(ext_bridge->display, &tmp);
	return ext_bridge->orig_funcs->mode_valid(bridge, info, &tmp);
}

static bool dsi_display_drm_ext_bridge_mode_fixup(
		struct drm_bridge *bridge,
		const struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	struct dsi_display_ext_bridge *ext_bridge;
	struct drm_display_mode tmp;

	ext_bridge = dsi_display_ext_get_bridge(bridge);
	if (!ext_bridge)
		return false;

	tmp = *mode;
	dsi_display_drm_ext_adjust_timing(ext_bridge->display, &tmp);
	return ext_bridge->orig_funcs->mode_fixup(bridge, &tmp, &tmp);
}

static void dsi_display_drm_ext_bridge_mode_set(
		struct drm_bridge *bridge,
		const struct drm_display_mode *mode,
		const struct drm_display_mode *adjusted_mode)
{
	struct dsi_display_ext_bridge *ext_bridge;
	struct drm_display_mode tmp;

	ext_bridge = dsi_display_ext_get_bridge(bridge);
	if (!ext_bridge)
		return;

	tmp = *mode;
	dsi_display_drm_ext_adjust_timing(ext_bridge->display, &tmp);
	ext_bridge->orig_funcs->mode_set(bridge, &tmp, &tmp);
}

static int dsi_host_ext_attach(struct mipi_dsi_host *host,
			   struct mipi_dsi_device *dsi)
{
	struct dsi_display *display = to_dsi_display(host);
	struct dsi_panel *panel;

	if (!host || !dsi || !display->panel) {
		DSI_ERR("Invalid param\n");
		return -EINVAL;
	}

	DSI_DEBUG("DSI[%s]: channel=%d, lanes=%d, format=%d, mode_flags=%lx\n",
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
	} else {
		panel->panel_mode = DSI_OP_CMD_MODE;
		DSI_ERR("command mode not supported by ext bridge\n");
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

struct drm_panel *dsi_display_get_drm_panel(struct dsi_display *display)
{
	if (!display || !display->panel) {
		pr_err("invalid param(s)\n");
		return NULL;
	}

	return &display->panel->drm_panel;
}

bool dsi_display_has_dsc_switch_support(struct dsi_display *display)
{
	if (!display || !display->panel) {
		pr_err("invalid param(s)\n");
		return false;
	}

	return display->panel->dsc_switch_supported;
}

int dsi_display_drm_ext_bridge_init(struct dsi_display *display,
		struct drm_encoder *encoder, struct drm_connector *connector)
{
	struct drm_device *drm;
	struct drm_bridge *bridge;
	struct drm_bridge *ext_bridge;
	struct drm_connector *ext_conn;
	struct sde_connector *sde_conn;
	struct drm_bridge *prev_bridge;
	int rc = 0, i;

	if (!display || !encoder || !connector)
		return -EINVAL;

	drm = encoder->dev;
	bridge = drm_bridge_chain_get_first_bridge(encoder);
	sde_conn = to_sde_connector(connector);
	prev_bridge = bridge;

	if (display->panel && !display->panel->host_config.ext_bridge_mode)
		return 0;

	if (!bridge)
		return -EINVAL;

	for (i = 0; i < display->ext_bridge_cnt; i++) {
		struct dsi_display_ext_bridge *ext_bridge_info =
				&display->ext_bridge[i];
		struct drm_encoder *c_encoder;

		/* return if ext bridge is already initialized */
		if (ext_bridge_info->bridge)
			return 0;

		ext_bridge = of_drm_find_bridge(ext_bridge_info->node_of);
		if (IS_ERR_OR_NULL(ext_bridge)) {
			rc = PTR_ERR(ext_bridge);
			DSI_ERR("failed to find ext bridge\n");
			goto error;
		}

		/* override functions for mode adjustment */
		if (display->ext_bridge_cnt > 1) {
			ext_bridge_info->bridge_funcs = *ext_bridge->funcs;
			if (ext_bridge->funcs->mode_fixup)
				ext_bridge_info->bridge_funcs.mode_fixup =
					dsi_display_drm_ext_bridge_mode_fixup;
			if (ext_bridge->funcs->mode_valid)
				ext_bridge_info->bridge_funcs.mode_valid =
					dsi_display_drm_ext_bridge_mode_valid;
			if (ext_bridge->funcs->mode_set)
				ext_bridge_info->bridge_funcs.mode_set =
					dsi_display_drm_ext_bridge_mode_set;
			ext_bridge_info->orig_funcs = ext_bridge->funcs;
			ext_bridge->funcs = &ext_bridge_info->bridge_funcs;
		}

		rc = drm_bridge_attach(encoder, ext_bridge, prev_bridge, 0);
		if (rc) {
			DSI_ERR("[%s] ext brige attach failed, %d\n",
				display->name, rc);
			goto error;
		}

		ext_bridge_info->display = display;
		ext_bridge_info->bridge = ext_bridge;
		prev_bridge = ext_bridge;

		/* ext bridge will init its own connector during attach,
		 * we need to extract it out of the connector list
		 */
		spin_lock_irq(&drm->mode_config.connector_list_lock);
		ext_conn = list_last_entry(&drm->mode_config.connector_list,
			struct drm_connector, head);

		if (!ext_conn) {
			DSI_ERR("failed to get external connector\n");
			rc = PTR_ERR(ext_conn);

			spin_unlock_irq(&drm->mode_config.connector_list_lock);
			goto error;
		}

		drm_connector_for_each_possible_encoder(ext_conn, c_encoder)
			break;

		if (!c_encoder) {
			DSI_ERR("failed to get encoder\n");
			rc = PTR_ERR(c_encoder);

			spin_unlock_irq(&drm->mode_config.connector_list_lock);
			goto error;
		}

		if (ext_conn && ext_conn != connector &&
			c_encoder->base.id == bridge->encoder->base.id) {
			list_del_init(&ext_conn->head);
			display->ext_conn = ext_conn;
		}
		spin_unlock_irq(&drm->mode_config.connector_list_lock);

		/* if there is no valid external connector created, or in split
		 * mode, default setting is used from panel defined in DT file.
		 */
		if (!display->ext_conn ||
		    !display->ext_conn->funcs ||
		    !display->ext_conn->helper_private ||
		    display->ext_bridge_cnt > 1) {
			display->ext_conn = NULL;
			continue;
		}

		/* otherwise, hook up the functions to use external connector */
		if (display->ext_conn->funcs->detect)
			sde_conn->ops.detect = dsi_display_drm_ext_detect;

		if (display->ext_conn->helper_private->get_modes)
			sde_conn->ops.get_modes =
				dsi_display_drm_ext_get_modes;

		if (display->ext_conn->helper_private->mode_valid)
			sde_conn->ops.mode_valid =
				dsi_display_drm_ext_mode_valid;

		if (display->ext_conn->helper_private->atomic_check)
			sde_conn->ops.atomic_check =
				dsi_display_drm_ext_atomic_check;

		sde_conn->ops.get_info =
				dsi_display_ext_get_info;
		sde_conn->ops.get_mode_info =
				dsi_display_ext_get_mode_info;

		/* add support to attach/detach */
		display->host.ops = &dsi_host_ext_ops;
	}

	return 0;
error:
	return rc;
}

int dsi_display_get_info(struct drm_connector *connector,
		struct msm_display_info *info, void *disp)
{
	struct dsi_display *display;
	struct dsi_panel_phy_props phy_props;
	struct dsi_host_common_cfg *host;
	int i, rc;

	if (!info || !disp) {
		DSI_ERR("invalid params\n");
		return -EINVAL;
	}

	display = disp;
	if (!display->panel) {
		DSI_ERR("invalid display panel\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	rc = dsi_panel_get_phy_props(display->panel, &phy_props);
	if (rc) {
		DSI_ERR("[%s] failed to get panel phy props, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	memset(info, 0, sizeof(struct msm_display_info));
	info->intf_type = DRM_MODE_CONNECTOR_DSI;
	info->num_of_h_tiles = display->ctrl_count;
	for (i = 0; i < info->num_of_h_tiles; i++)
		info->h_tile_instance[i] = display->ctrl[i].ctrl->cell_index;

	info->is_connected = display->is_active;

	if (!strcmp(display->display_type, "primary"))
		info->display_type = SDE_CONNECTOR_PRIMARY;
	else if (!strcmp(display->display_type, "secondary"))
		info->display_type = SDE_CONNECTOR_SECONDARY;

	info->width_mm = phy_props.panel_width_mm;
	info->height_mm = phy_props.panel_height_mm;
	info->max_width = 1920;
	info->max_height = 1080;
	info->qsync_min_fps = display->panel->qsync_caps.qsync_min_fps;
	info->has_qsync_min_fps_list = (display->panel->qsync_caps.qsync_min_fps_list_len > 0);
	info->has_avr_step_req = (display->panel->avr_caps.avr_step_fps_list_len > 0);
	info->poms_align_vsync = display->panel->poms_align_vsync;

	switch (display->panel->panel_mode) {
	case DSI_OP_VIDEO_MODE:
		info->curr_panel_mode = MSM_DISPLAY_VIDEO_MODE;
		info->capabilities |= MSM_DISPLAY_CAP_VID_MODE;
		if (display->panel->panel_mode_switch_enabled)
			info->capabilities |= MSM_DISPLAY_CAP_CMD_MODE;
		break;
	case DSI_OP_CMD_MODE:
		info->curr_panel_mode = MSM_DISPLAY_CMD_MODE;
		info->capabilities |= MSM_DISPLAY_CAP_CMD_MODE;
		if (display->panel->panel_mode_switch_enabled)
			info->capabilities |= MSM_DISPLAY_CAP_VID_MODE;
		info->is_te_using_watchdog_timer = is_sim_panel(display);
		break;
	default:
		DSI_ERR("unknwown dsi panel mode %d\n",
				display->panel->panel_mode);
		break;
	}

	if (display->panel->esd_config.esd_enabled && !is_sim_panel(display))
		info->capabilities |= MSM_DISPLAY_ESD_ENABLED;

	info->te_source = display->te_source;

	host = &display->panel->host_config;
	if (host->split_link.enabled)
		info->capabilities |= MSM_DISPLAY_SPLIT_LINK;

	info->dsc_count = display->panel->dsc_count;
	info->lm_count = display->panel->lm_count;
	info->switch_vsync_delay = display->panel->switch_vsync_delay;
error:
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_get_mode_count(struct dsi_display *display,
			u32 *count)
{
	if (!display || !display->panel) {
		DSI_ERR("invalid display:%d panel:%d\n", display != NULL,
			display ? display->panel != NULL : 0);
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	*count = display->panel->num_display_modes;
	mutex_unlock(&display->display_lock);

	return 0;
}

void dsi_display_adjust_mode_timing(struct dsi_display *display,
			struct dsi_display_mode *dsi_mode,
			int lanes, int bpp)
{
	u64 new_htotal, new_vtotal, htotal, vtotal, old_htotal, div;
	struct dsi_dyn_clk_caps *dyn_clk_caps;
	u32 bits_per_symbol = 16, num_of_symbols = 7; /* For Cphy */

	dyn_clk_caps = &(display->panel->dyn_clk_caps);

	/* Constant FPS is not supported on command mode */
	if (!(dsi_mode->panel_mode_caps & DSI_OP_VIDEO_MODE))
		return;

	if (!dyn_clk_caps->maintain_const_fps)
		return;
	/*
	 * When there is a dynamic clock switch, there is small change
	 * in FPS. To compensate for this difference in FPS, hfp or vfp
	 * is adjusted. It has been assumed that the refined porch values
	 * are supported by the panel. This logic can be enhanced further
	 * in future by taking min/max porches supported by the panel.
	 */
	switch (dyn_clk_caps->type) {
	case DSI_DYN_CLK_TYPE_CONST_FPS_ADJUST_HFP:
		vtotal = DSI_V_TOTAL(&dsi_mode->timing);
		old_htotal = dsi_h_total_dce(&dsi_mode->timing);
		do_div(old_htotal, display->ctrl_count);
		new_htotal = dsi_mode->timing.clk_rate_hz * lanes;
		div = bpp * vtotal * dsi_mode->timing.refresh_rate;
		if (dsi_is_type_cphy(&display->panel->host_config)) {
			new_htotal = new_htotal * bits_per_symbol;
			div = div * num_of_symbols;
		}
		do_div(new_htotal, div);
		if (old_htotal > new_htotal)
			dsi_mode->timing.h_front_porch -=
			((old_htotal - new_htotal) * display->ctrl_count);
		else
			dsi_mode->timing.h_front_porch +=
			((new_htotal - old_htotal) * display->ctrl_count);
		break;

	case DSI_DYN_CLK_TYPE_CONST_FPS_ADJUST_VFP:
		htotal = dsi_h_total_dce(&dsi_mode->timing);
		do_div(htotal, display->ctrl_count);
		new_vtotal = dsi_mode->timing.clk_rate_hz * lanes;
		div = bpp * htotal * dsi_mode->timing.refresh_rate;
		if (dsi_is_type_cphy(&display->panel->host_config)) {
			new_vtotal = new_vtotal * bits_per_symbol;
			div = div * num_of_symbols;
		}
		do_div(new_vtotal, div);
		dsi_mode->timing.v_front_porch = new_vtotal -
				dsi_mode->timing.v_back_porch -
				dsi_mode->timing.v_sync_width -
				dsi_mode->timing.v_active;
		break;

	default:
		break;
	}

	dsi_mode->pixel_clk_khz = div_u64(dsi_mode->timing.clk_rate_hz * lanes, bpp);
	do_div(dsi_mode->pixel_clk_khz, 1000);
	dsi_mode->pixel_clk_khz *= display->ctrl_count;
}

static void _dsi_display_populate_bit_clks(struct dsi_display *display, int start, int end)
{
	struct dsi_dyn_clk_caps *dyn_clk_caps;
	struct dsi_display_mode *src, dst;
	struct dsi_host_common_cfg *cfg;
	int i, j, bpp, lanes = 0;

	if (!display)
		return;

	dyn_clk_caps = &(display->panel->dyn_clk_caps);
	if (!dyn_clk_caps->dyn_clk_support)
		return;

	cfg = &(display->panel->host_config);
	bpp = dsi_pixel_format_to_bpp(cfg->dst_format);

	if (cfg->data_lanes & DSI_DATA_LANE_0)
		lanes++;
	if (cfg->data_lanes & DSI_DATA_LANE_1)
		lanes++;
	if (cfg->data_lanes & DSI_DATA_LANE_2)
		lanes++;
	if (cfg->data_lanes & DSI_DATA_LANE_3)
		lanes++;

	for (i = start; i < end; i++) {
		src = &display->modes[i];
		if (!src)
			return;

		if (!src->priv_info->bit_clk_list.count)
			continue;

		src->timing.clk_rate_hz = src->priv_info->bit_clk_list.rates[0];

		dsi_display_adjust_mode_timing(display, src, lanes, bpp);

		/* populate mode adjusted values */
		for (j = 0; j < src->priv_info->bit_clk_list.count; j++) {
			memcpy(&dst, src, sizeof(struct dsi_display_mode));
			memcpy(&dst.timing, &src->timing, sizeof(struct dsi_mode_info));
			dst.timing.clk_rate_hz = src->priv_info->bit_clk_list.rates[j];

			dsi_display_adjust_mode_timing(display, &dst, lanes, bpp);

			/* store the list of RFI matching porches */
			switch (dyn_clk_caps->type) {
			case DSI_DYN_CLK_TYPE_CONST_FPS_ADJUST_HFP:
				src->priv_info->bit_clk_list.front_porches[j] =
						dst.timing.h_front_porch;
				break;

			case DSI_DYN_CLK_TYPE_CONST_FPS_ADJUST_VFP:
				src->priv_info->bit_clk_list.front_porches[j] =
						dst.timing.v_front_porch;
				break;

			default:
				break;
			}

			/* store the list of RFI matching pixel clocks */
			src->priv_info->bit_clk_list.pixel_clks_khz[j] = dst.pixel_clk_khz;
		}
	}
}

int dsi_display_restore_bit_clk(struct dsi_display *display, struct dsi_display_mode *mode)
{
	int i;
	u32 clk_rate_hz = 0;

	if (!display || !mode || !mode->priv_info) {
		DSI_ERR("invalid arguments\n");
		return -EINVAL;
	}

	clk_rate_hz = display->cached_clk_rate;

	if (mode->priv_info->bit_clk_list.count) {
		/* use first entry as the default bit clk rate */
		clk_rate_hz = mode->priv_info->bit_clk_list.rates[0];

		for (i = 0; i < mode->priv_info->bit_clk_list.count; i++) {
			if (display->dyn_bit_clk == mode->priv_info->bit_clk_list.rates[i])
				clk_rate_hz = display->dyn_bit_clk;
		}
	}

	mode->timing.clk_rate_hz = clk_rate_hz;
	mode->priv_info->clk_rate_hz = clk_rate_hz;

	SDE_EVT32(clk_rate_hz, display->cached_clk_rate, display->dyn_bit_clk);
	DSI_DEBUG("clk_rate_hz:%u, cached_clk_rate:%u, dyn_bit_clk:%u\n",
			clk_rate_hz, display->cached_clk_rate, display->dyn_bit_clk);
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
	struct dsi_display_ctrl *ctrl;
	struct dsi_host_common_cfg *host = &display->panel->host_config;
	bool is_split_link, support_cmd_mode, support_video_mode;
	u32 num_dfps_rates, timing_mode_count, display_mode_count;
	u32 sublinks_count, mode_idx, array_idx = 0;
	struct dsi_dyn_clk_caps *dyn_clk_caps;
	int i, start, end, rc = -EINVAL;
	int dsc_modes = 0, nondsc_modes = 0;
	struct dsi_qsync_capabilities *qsync_caps;

	if (!display || !out_modes) {
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	*out_modes = NULL;
	ctrl = &display->ctrl[0];

	mutex_lock(&display->display_lock);

	if (display->modes)
		goto exit;

	display_mode_count = display->panel->num_display_modes;

	display->modes = kcalloc(display_mode_count, sizeof(*display->modes),
			GFP_KERNEL);
	if (!display->modes) {
		rc = -ENOMEM;
		goto error;
	}

	rc = dsi_panel_get_dfps_caps(display->panel, &dfps_caps);
	if (rc) {
		DSI_ERR("[%s] failed to get dfps caps from panel\n",
				display->name);
		goto error;
	}

	qsync_caps = &(display->panel->qsync_caps);
	dyn_clk_caps = &(display->panel->dyn_clk_caps);

	timing_mode_count = display->panel->num_timing_nodes;

	/* Validate command line timing */
	if ((display->cmdline_timing != NO_OVERRIDE) &&
		(display->cmdline_timing >= timing_mode_count))
		display->cmdline_timing = NO_OVERRIDE;

	for (mode_idx = 0; mode_idx < timing_mode_count; mode_idx++) {
		struct dsi_display_mode display_mode;
		int topology_override = NO_OVERRIDE;
		bool is_preferred = false;
		u32 frame_threshold_us = ctrl->ctrl->frame_threshold_time_us;

		memset(&display_mode, 0, sizeof(display_mode));

		rc = dsi_panel_get_mode(display->panel, mode_idx,
						&display_mode,
						topology_override);
		if (rc) {
			DSI_ERR("[%s] failed to get mode idx %d from panel\n",
				   display->name, mode_idx);
			goto error;
		}

		if (display->cmdline_timing == display_mode.mode_idx) {
			topology_override = display->cmdline_topology;
			is_preferred = true;
		}

		support_cmd_mode = display_mode.panel_mode_caps & DSI_OP_CMD_MODE;
		support_video_mode = display_mode.panel_mode_caps & DSI_OP_VIDEO_MODE;

		if (display_mode.priv_info->dsc_enabled)
			dsc_modes++;
		else
			nondsc_modes++;

		/* Setup widebus support */
		display_mode.priv_info->widebus_support =
				ctrl->ctrl->hw.widebus_support;
		num_dfps_rates = ((!dfps_caps.dfps_support ||
			!support_video_mode) ? 1 : dfps_caps.dfps_list_len);

		/* Calculate dsi frame transfer time */
		if (support_cmd_mode) {
			dsi_panel_calc_dsi_transfer_time(
					&display->panel->host_config,
					&display_mode, frame_threshold_us);
			display_mode.priv_info->dsi_transfer_time_us =
				display_mode.timing.dsi_transfer_time_us;
			display_mode.priv_info->min_dsi_clk_hz =
				display_mode.timing.min_dsi_clk_hz;

			display_mode.priv_info->mdp_transfer_time_us =
				display_mode.timing.mdp_transfer_time_us;
		}

		is_split_link = host->split_link.enabled;
		sublinks_count = host->split_link.num_sublinks;
		if (is_split_link && sublinks_count > 1) {
			display_mode.timing.h_active *= sublinks_count;
			display_mode.timing.h_front_porch *= sublinks_count;
			display_mode.timing.h_sync_width *= sublinks_count;
			display_mode.timing.h_back_porch *= sublinks_count;
			display_mode.timing.h_skew *= sublinks_count;
			display_mode.pixel_clk_khz *= sublinks_count;
		} else {
			display_mode.timing.h_active *= display->ctrl_count;
			display_mode.timing.h_front_porch *=
						display->ctrl_count;
			display_mode.timing.h_sync_width *=
						display->ctrl_count;
			display_mode.timing.h_back_porch *=
						display->ctrl_count;
			display_mode.timing.h_skew *= display->ctrl_count;
			display_mode.pixel_clk_khz *= display->ctrl_count;
		}

		start = array_idx;
		for (i = 0; i < num_dfps_rates; i++) {
			struct dsi_display_mode *sub_mode =
					&display->modes[array_idx];
			u32 curr_refresh_rate;

			if (!sub_mode) {
				DSI_ERR("invalid mode data\n");
				rc = -EFAULT;
				goto error;
			}

			memcpy(sub_mode, &display_mode, sizeof(display_mode));
			array_idx++;

			/*
			 * Populate mode qsync min fps from panel min qsync fps dt property
			 * in video mode & in command mode where per mode qsync min fps is
			 * not defined.
			 */
			if (!sub_mode->timing.qsync_min_fps && qsync_caps->qsync_min_fps)
				sub_mode->timing.qsync_min_fps = qsync_caps->qsync_min_fps;

			/*
			 * Qsync min fps for the mode will be populated in the timing info
			 * in dsi_panel_get_mode function.
			 */
			sub_mode->priv_info->qsync_min_fps = sub_mode->timing.qsync_min_fps;
			if (!dfps_caps.dfps_support || !support_video_mode)
				continue;

			sub_mode->mode_idx += (array_idx - 1);
			curr_refresh_rate = sub_mode->timing.refresh_rate;
			sub_mode->timing.refresh_rate = dfps_caps.dfps_list[i];

			/* Override with qsync min fps list in dfps usecases */
			if (qsync_caps->qsync_min_fps && qsync_caps->qsync_min_fps_list_len) {
				sub_mode->timing.qsync_min_fps = qsync_caps->qsync_min_fps_list[i];
				sub_mode->priv_info->qsync_min_fps = sub_mode->timing.qsync_min_fps;
			}

			dsi_display_get_dfps_timing(display, sub_mode,
					curr_refresh_rate);
			sub_mode->panel_mode_caps = DSI_OP_VIDEO_MODE;
		}
		end = array_idx;

		_dsi_display_populate_bit_clks(display, start, end);

		if (is_preferred) {
			/* Set first timing sub mode as preferred mode */
			display->modes[start].is_preferred = true;
		}
	}

	if (dsc_modes && nondsc_modes)
		display->panel->dsc_switch_supported = true;

exit:
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
	struct dsi_host_common_cfg *host;

	if (!display || !display->panel)
		return -EINVAL;

	mutex_lock(&display->display_lock);

	count = display->panel->num_display_modes;

	if (display->panel->cur_mode)
		refresh_rate = display->panel->cur_mode->timing.refresh_rate;

	dsi_panel_get_dfps_caps(display->panel, &dfps_caps);
	if (dfps_caps.dfps_support)
		refresh_rate = dfps_caps.max_refresh_rate;

	if (!refresh_rate) {
		mutex_unlock(&display->display_lock);
		DSI_ERR("Null Refresh Rate\n");
		return -EINVAL;
	}

	host = &display->panel->host_config;
	if (host->split_link.enabled)
		h_active *= host->split_link.num_sublinks;
	else
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

int dsi_display_get_default_lms(void *dsi_display, u32 *num_lm)
{
	struct dsi_display *display = (struct dsi_display *)dsi_display;
	u32 count, i;
	int rc = 0;

	*num_lm = 0;

	mutex_lock(&display->display_lock);
	count = display->panel->num_display_modes;
	mutex_unlock(&display->display_lock);

	if (!display->modes) {
		struct dsi_display_mode *m;

		rc = dsi_display_get_modes(display, &m);
		if (rc)
			return rc;
	}

	mutex_lock(&display->display_lock);
	for (i = 0; i < count; i++) {
		struct dsi_display_mode *m = &display->modes[i];

		*num_lm = max(m->priv_info->topology.num_lm, *num_lm);
	}
	mutex_unlock(&display->display_lock);

	return rc;
}

int dsi_display_get_avr_step_req_fps(void *display_dsi, u32 mode_fps)
{
	struct dsi_display *display = (struct dsi_display *)display_dsi;
	struct dsi_panel *panel;
	u32 i, step = 0;

	if (!display || !display->panel)
		return -EINVAL;

	panel = display->panel;

	/* support a single fixed rate, or rate corresponding to dfps list entry */
	if (panel->avr_caps.avr_step_fps_list_len == 1) {
		step = panel->avr_caps.avr_step_fps_list[0];
	} else if (panel->avr_caps.avr_step_fps_list_len > 1) {
		for (i = 0; i < panel->dfps_caps.dfps_list_len; i++) {
			if (panel->dfps_caps.dfps_list[i] == mode_fps)
				step = panel->avr_caps.avr_step_fps_list[i];
		}
	}

	DSI_DEBUG("mode_fps %u, avr_step fps %u\n", mode_fps, step);
	return step;
}

static bool dsi_display_match_timings(const struct dsi_display_mode *mode1,
		struct dsi_display_mode *mode2, unsigned int match_flags)
{
	bool is_matching = false;

	if (match_flags & DSI_MODE_MATCH_ACTIVE_TIMINGS) {
		is_matching = mode1->timing.h_active == mode2->timing.h_active &&
				mode1->timing.v_active == mode2->timing.v_active &&
				mode1->timing.refresh_rate == mode2->timing.refresh_rate;
		if (!is_matching)
			goto end;
	}

	if (match_flags & DSI_MODE_MATCH_PORCH_TIMINGS)
		is_matching = mode1->timing.h_back_porch == mode2->timing.h_back_porch &&
				mode1->timing.h_front_porch == mode2->timing.h_front_porch &&
				mode1->timing.h_sync_width == mode2->timing.h_sync_width &&
				mode1->timing.h_skew == mode2->timing.h_skew &&
				mode1->timing.v_back_porch == mode2->timing.v_back_porch &&
				mode1->timing.v_front_porch == mode2->timing.v_front_porch &&
				mode1->timing.v_sync_width == mode2->timing.v_sync_width;

end:
	return is_matching;
}

bool dsi_display_mode_match(const struct dsi_display_mode *mode1,
		struct dsi_display_mode *mode2, unsigned int match_flags)
{
	if (!mode1 && !mode2)
		return true;

	if (!mode1 || !mode2)
		return false;

	if ((match_flags & DSI_MODE_MATCH_FULL_TIMINGS) &&
			!dsi_display_match_timings(mode1, mode2, match_flags))
		return false;

	if ((match_flags & DSI_MODE_MATCH_DSC_CONFIG) &&
			mode1->priv_info->dsc_enabled != mode2->priv_info->dsc_enabled)
		return false;

	return true;
}

int dsi_display_find_mode(struct dsi_display *display,
		struct dsi_display_mode *cmp,
		struct msm_sub_mode *sub_mode,
		struct dsi_display_mode **out_mode)
{
	u32 count, i;
	int rc;
	struct dsi_display_mode *m;
	struct dsi_dyn_clk_caps *dyn_clk_caps;
	unsigned int match_flags = DSI_MODE_MATCH_FULL_TIMINGS;
	struct dsi_display_mode_priv_info *priv_info;

	if (!display || !out_mode)
		return -EINVAL;

	*out_mode = NULL;

	mutex_lock(&display->display_lock);
	count = display->panel->num_display_modes;
	mutex_unlock(&display->display_lock);

	if (!display->modes) {
		rc = dsi_display_get_modes(display, &m);
		if (rc)
			return rc;
	}

	priv_info = kvzalloc(sizeof(struct dsi_display_mode_priv_info),
			GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(priv_info))
		return -ENOMEM;

	mutex_lock(&display->display_lock);
	dyn_clk_caps = &(display->panel->dyn_clk_caps);
	for (i = 0; i < count; i++) {
		m = &display->modes[i];

		/**
		 * When dynamic bit clock is enabled with contants FPS,
		 * the adjusted mode porches value may not match the panel
		 * default mode porches and panel mode lookup will fail.
		 * In that case we omit porches in mode matching function.
		 */
		if (dyn_clk_caps->maintain_const_fps)
			match_flags = DSI_MODE_MATCH_ACTIVE_TIMINGS;

		if (sub_mode && sub_mode->dsc_mode) {
			match_flags |= DSI_MODE_MATCH_DSC_CONFIG;
			cmp->priv_info = priv_info;
			cmp->priv_info->dsc_enabled = (sub_mode->dsc_mode ==
				MSM_DISPLAY_DSC_MODE_ENABLED) ? true : false;
		}

		if (dsi_display_mode_match(cmp, m, match_flags)) {
			*out_mode = m;
			rc = 0;
			break;
		}
	}

	cmp->priv_info = NULL;

	mutex_unlock(&display->display_lock);
	kvfree(priv_info);

	if (!*out_mode) {
		DSI_ERR("[%s] failed to find mode for v_active %u h_active %u fps %u pclk %u\n",
				display->name, cmp->timing.v_active,
				cmp->timing.h_active, cmp->timing.refresh_rate,
				cmp->pixel_clk_khz);
		rc = -ENOENT;
	}

	return rc;
}

static inline bool dsi_display_mode_switch_dfps(struct dsi_display_mode *cur,
						struct dsi_display_mode *adj)
{
	/*
	 * If there is a change in the hfp or vfp of the current and adjoining
	 * mode,then either it is a dfps mode switch or dynamic clk change with
	 * constant fps.
	 */
	if ((cur->timing.h_front_porch != adj->timing.h_front_porch) ||
		(cur->timing.v_front_porch != adj->timing.v_front_porch))
		return true;
	else
		return false;
}

/**
 * dsi_display_validate_mode_change() - Validate mode change case.
 * @display:     DSI display handle.
 * @cur_mode:    Current mode.
 * @adj_mode:    Mode to be set.
 *               MSM_MODE_FLAG_SEAMLESS_VRR flag is set if there
 *               is change in hfp or vfp but vactive and hactive are same.
 *               DSI_MODE_FLAG_DYN_CLK flag is set if there
 *               is change in clk but vactive and hactive are same.
 * Return: error code.
 */
int dsi_display_validate_mode_change(struct dsi_display *display,
			struct dsi_display_mode *cur_mode,
			struct dsi_display_mode *adj_mode)
{
	int rc = 0;
	struct dsi_dfps_capabilities dfps_caps;
	struct dsi_dyn_clk_caps *dyn_clk_caps;
	struct sde_connector *sde_conn;

	if (!display || !adj_mode || !display->drm_conn) {
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (!display->panel || !display->panel->cur_mode) {
		DSI_DEBUG("Current panel mode not set\n");
		return rc;
	}

	if ((cur_mode->timing.v_active != adj_mode->timing.v_active) ||
		(cur_mode->timing.h_active != adj_mode->timing.h_active)) {
		DSI_DEBUG("Avoid VRR and POMS when resolution is changed\n");
		return rc;
	}

	sde_conn = to_sde_connector(display->drm_conn);

	mutex_lock(&display->display_lock);

	if (sde_conn->expected_panel_mode == MSM_DISPLAY_VIDEO_MODE &&
		display->config.panel_mode == DSI_OP_CMD_MODE) {
		adj_mode->dsi_mode_flags |= DSI_MODE_FLAG_POMS_TO_VID;
		SDE_EVT32(SDE_EVTLOG_FUNC_CASE1, sde_conn->expected_panel_mode,
			display->config.panel_mode);
		DSI_DEBUG("Panel operating mode change to video detected\n");
	} else if (sde_conn->expected_panel_mode == MSM_DISPLAY_CMD_MODE &&
		display->config.panel_mode == DSI_OP_VIDEO_MODE) {
		adj_mode->dsi_mode_flags |= DSI_MODE_FLAG_POMS_TO_CMD;
		SDE_EVT32(SDE_EVTLOG_FUNC_CASE2, sde_conn->expected_panel_mode,
			display->config.panel_mode);
		DSI_DEBUG("Panel operating mode change to command detected\n");
	} else if (cur_mode->timing.dsc_enabled != adj_mode->timing.dsc_enabled) {
		adj_mode->dsi_mode_flags |= DSI_MODE_FLAG_DMS;
		SDE_EVT32(SDE_EVTLOG_FUNC_CASE3, cur_mode->timing.dsc_enabled,
				adj_mode->timing.dsc_enabled);
		DSI_DEBUG("DSC mode change detected\n");
	} else {
		dyn_clk_caps = &(display->panel->dyn_clk_caps);
		/* dfps and dynamic clock with const fps use case */
		if (dsi_display_mode_switch_dfps(cur_mode, adj_mode)) {
			dsi_panel_get_dfps_caps(display->panel, &dfps_caps);
			if (dfps_caps.dfps_support ||
				dyn_clk_caps->maintain_const_fps) {
				DSI_DEBUG("Mode switch is seamless variable refresh\n");
				adj_mode->dsi_mode_flags |= DSI_MODE_FLAG_VRR;
				SDE_EVT32(SDE_EVTLOG_FUNC_CASE4,
					cur_mode->timing.refresh_rate,
					adj_mode->timing.refresh_rate,
					cur_mode->timing.h_front_porch,
					adj_mode->timing.h_front_porch,
					cur_mode->timing.v_front_porch,
					adj_mode->timing.v_front_porch);
			}
		}

		/* dynamic clk change use case */
		if (display->dyn_bit_clk_pending) {
			if (dyn_clk_caps->dyn_clk_support) {
				DSI_DEBUG("dynamic clk change detected\n");
				if ((adj_mode->dsi_mode_flags &
					DSI_MODE_FLAG_VRR) &&
					(!dyn_clk_caps->maintain_const_fps)) {
					DSI_ERR("dfps and dyn clk not supported in same commit\n");
					rc = -ENOTSUPP;
					goto error;
				}

				/**
				 * Set VRR flag whenever there is a dynamic clock
				 * change on video mode panel as dynamic refresh is
				 * always required when fps compensation is enabled.
				 */
				if ((display->config.panel_mode == DSI_OP_VIDEO_MODE) &&
						dyn_clk_caps->maintain_const_fps)
					adj_mode->dsi_mode_flags |= DSI_MODE_FLAG_VRR;

				adj_mode->dsi_mode_flags |=
						DSI_MODE_FLAG_DYN_CLK;
				SDE_EVT32(SDE_EVTLOG_FUNC_CASE5,
					cur_mode->pixel_clk_khz,
					adj_mode->pixel_clk_khz);
			}
			display->dyn_bit_clk_pending = false;
		}
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
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	adj_mode = *mode;
	adjust_timing_by_ctrl_count(display, &adj_mode);

	rc = dsi_panel_validate_mode(display->panel, &adj_mode);
	if (rc) {
		DSI_ERR("[%s] panel mode validation failed, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_validate_timing(ctrl->ctrl, &adj_mode.timing);
		if (rc) {
			DSI_ERR("[%s] ctrl mode validation failed, rc=%d\n",
			       display->name, rc);
			goto error;
		}

		rc = dsi_phy_validate_mode(ctrl->phy, &adj_mode.timing);
		if (rc) {
			DSI_ERR("[%s] phy mode validation failed, rc=%d\n",
			       display->name, rc);
			goto error;
		}
	}

	if ((flags & DSI_VALIDATE_FLAG_ALLOW_ADJUST) &&
			(mode->dsi_mode_flags & DSI_MODE_FLAG_SEAMLESS)) {
		rc = dsi_display_validate_mode_seamless(display, mode);
		if (rc) {
			DSI_ERR("[%s] seamless not possible rc=%d\n",
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
	struct dsi_mode_info timing;

	if (!display || !mode || !display->panel) {
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	adj_mode = *mode;
	timing = adj_mode.timing;
	adjust_timing_by_ctrl_count(display, &adj_mode);

	if (!display->panel->cur_mode) {
		display->panel->cur_mode =
			kzalloc(sizeof(struct dsi_display_mode), GFP_KERNEL);
		if (!display->panel->cur_mode) {
			rc = -ENOMEM;
			goto error;
		}
	}

	rc = dsi_display_restore_bit_clk(display, &adj_mode);
	if (rc) {
		DSI_ERR("[%s] bit clk rate cannot be restored\n", display->name);
		goto error;
	}

	rc = dsi_display_validate_mode_set(display, &adj_mode, flags);
	if (rc) {
		DSI_ERR("[%s] mode cannot be set\n", display->name);
		goto error;
	}

	rc = dsi_display_set_mode_sub(display, &adj_mode, flags);
	if (rc) {
		DSI_ERR("[%s] failed to set mode\n", display->name);
		goto error;
	}

	DSI_INFO("mdp_transfer_time=%d, hactive=%d, vactive=%d, fps=%d, clk_rate=%llu\n",
			adj_mode.priv_info->mdp_transfer_time_us,
			timing.h_active, timing.v_active, timing.refresh_rate,
			adj_mode.priv_info->clk_rate_hz);
	SDE_EVT32(adj_mode.priv_info->mdp_transfer_time_us,
			timing.h_active, timing.v_active, timing.refresh_rate,
			adj_mode.priv_info->clk_rate_hz);

	if (timing.h_skew) {
		DISP_UTC_INFO("-------------- ddic %d fps\n", timing.h_skew);
		mi_disp_feature_event_notify_by_type(mi_get_disp_id(display->display_type),
			MI_DISP_EVENT_FPS, sizeof(timing.h_skew), timing.h_skew);

		if (display->panel->cur_mode->timing.refresh_rate != timing.refresh_rate) {
			mi_disp_feature_sysfs_notify(mi_get_disp_id(display->display_type),
				MI_SYSFS_DYNAMIC_FPS);
		}
    } else {
		mi_disp_feature_event_notify_by_type(mi_get_disp_id(display->display_type),
			MI_DISP_EVENT_FPS, sizeof(timing.refresh_rate), timing.refresh_rate);

		if (display->panel->cur_mode->timing.refresh_rate != timing.refresh_rate) {
			mi_disp_feature_sysfs_notify(mi_get_disp_id(display->display_type),
				MI_SYSFS_DYNAMIC_FPS);
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
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		rc = dsi_ctrl_set_tpg_state(ctrl->ctrl, enable);
		if (rc) {
			DSI_ERR("[%s] failed to set tpg state for host_%d\n",
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
		DSI_ERR("[%s] failed to enable DSI core clocks, rc=%d\n",
		       display->name, rc);
		goto error;
	}

	rc = dsi_display_ctrl_update(display);
	if (rc) {
		DSI_ERR("[%s] failed to update DSI controller, rc=%d\n",
			   display->name, rc);
		goto error_ctrl_clk_off;
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_LINK_CLK, DSI_CLK_ON);
	if (rc) {
		DSI_ERR("[%s] failed to enable DSI link clocks, rc=%d\n",
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

static bool _dsi_display_validate_host_state(struct dsi_display *display)
{
	int i;
	struct dsi_display_ctrl *ctrl;

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (!ctrl->ctrl)
			continue;
		if (!dsi_ctrl_validate_host_state(ctrl->ctrl))
			return false;
	}

	return true;
}

static void dsi_display_handle_fifo_underflow(struct work_struct *work)
{
	struct dsi_display *display = NULL;

	display = container_of(work, struct dsi_display, fifo_underflow_work);
	if (!display || !display->panel ||
	    atomic_read(&display->panel->esd_recovery_pending)) {
		DSI_DEBUG("Invalid recovery use case\n");
		return;
	}

	mutex_lock(&display->display_lock);

	if (!_dsi_display_validate_host_state(display)) {
		mutex_unlock(&display->display_lock);
		return;
	}

	DSI_INFO("handle DSI FIFO underflow error\n");
	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY);

	dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);
	dsi_display_soft_reset(display);
	dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);
	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT);

	mutex_unlock(&display->display_lock);
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

	display = container_of(work, struct dsi_display, fifo_overflow_work);
	if (!display || !display->panel ||
	    (display->panel->panel_mode != DSI_OP_VIDEO_MODE) ||
	    atomic_read(&display->panel->esd_recovery_pending)) {
		DSI_DEBUG("Invalid recovery use case\n");
		return;
	}

	mutex_lock(&display->display_lock);

	if (!_dsi_display_validate_host_state(display)) {
		mutex_unlock(&display->display_lock);
		return;
	}

	DSI_INFO("handle DSI FIFO overflow error\n");
	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY);

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
	display_for_each_ctrl(i, display) {
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
			DSI_DEBUG("sde callback failed\n");
			goto end;
		}
	}

	/* Enable Video mode for DSI controller */
	display_for_each_ctrl(i, display) {
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
	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT);

	mutex_unlock(&display->display_lock);
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

	display = container_of(work, struct dsi_display, lp_rx_timeout_work);
	if (!display || !display->panel ||
	    (display->panel->panel_mode != DSI_OP_VIDEO_MODE) ||
	    atomic_read(&display->panel->esd_recovery_pending)) {
		DSI_DEBUG("Invalid recovery use case\n");
		return;
	}

	mutex_lock(&display->display_lock);

	if (!_dsi_display_validate_host_state(display)) {
		mutex_unlock(&display->display_lock);
		return;
	}

	DSI_INFO("handle DSI LP RX Timeout error\n");
	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY);

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
	display_for_each_ctrl(i, display) {
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
			DSI_DEBUG("Target is in suspend/shutdown\n");
			goto end;
		}
	}

	/* Enable Video mode for DSI controller */
	display_for_each_ctrl(i, display) {
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
	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT);

	mutex_unlock(&display->display_lock);
}

static int dsi_display_cb_error_handler(void *data,
		uint32_t event_idx, uint32_t instance_idx,
		uint32_t data0, uint32_t data1,
		uint32_t data2, uint32_t data3)
{
	struct dsi_display *display =  data;

	if (!display || !(display->err_workq))
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
		DSI_WARN("unhandled error interrupt: %d\n", event_idx);
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
		DSI_ERR("failed to create dsi workq!\n");
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

	display_for_each_ctrl(i, display) {
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

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		memset(&ctrl->ctrl->irq_info.irq_err_cb,
		       0, sizeof(struct dsi_event_cb_info));
	}

	if (display->err_workq) {
		destroy_workqueue(display->err_workq);
		display->err_workq = NULL;
	}
}

int dsi_display_prepare(struct dsi_display *display)
{
	int rc = 0;
	struct dsi_display_mode *mode;

	if (!display) {
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (!display->panel->cur_mode) {
		DSI_ERR("no valid mode set for the display\n");
		return -EINVAL;
	}

	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY);
	mutex_lock(&display->display_lock);

	display->hw_ownership = true;
	mode = display->panel->cur_mode;

	dsi_display_set_ctrl_esd_check_flag(display, false);

	/* Set up ctrl isr before enabling core clk */
	if (!display->trusted_vm_env)
		dsi_display_ctrl_isr_configure(display, true);

	if (mode->dsi_mode_flags & DSI_MODE_FLAG_DMS) {
		if (display->is_cont_splash_enabled &&
		    display->config.panel_mode == DSI_OP_VIDEO_MODE) {
			DSI_ERR("DMS not supported on first frame\n");
			rc = -EINVAL;
			goto error;
		}

		if (!is_skip_op_required(display)) {
			/* update dsi ctrl for new mode */
			rc = dsi_display_pre_switch(display);
			if (rc)
				DSI_ERR("[%s] panel pre-switch failed, rc=%d\n",
					display->name, rc);
			goto error;
		}
	}

	if (!display->poms_pending &&
		(!is_skip_op_required(display))) {
		/*
		 * For continuous splash/trusted vm, we skip panel
		 * pre prepare since the regulator vote is already
		 * taken care in splash resource init
		 */
		rc = dsi_panel_pre_prepare(display->panel);
		if (rc) {
			DSI_ERR("[%s] panel pre-prepare failed, rc=%d\n",
					display->name, rc);
			goto error;
		}
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	if (rc) {
		DSI_ERR("[%s] failed to enable DSI core clocks, rc=%d\n",
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
			DSI_ERR("[%s] failed to reset phy, rc=%d\n",
				display->name, rc);
			goto error_ctrl_clk_off;
		}

		rc = dsi_display_phy_enable(display);
		if (rc) {
			DSI_ERR("[%s] failed to enable DSI PHY, rc=%d\n",
			       display->name, rc);
			goto error_ctrl_clk_off;
		}
	}

	rc = dsi_display_ctrl_init(display);
	if (rc) {
		DSI_ERR("[%s] failed to setup DSI controller, rc=%d\n",
		       display->name, rc);
		goto error_phy_disable;
	}
	/* Set up DSI ERROR event callback */
	dsi_display_register_error_handler(display);

	rc = dsi_display_ctrl_host_enable(display);
	if (rc) {
		DSI_ERR("[%s] failed to enable DSI host, rc=%d\n",
		       display->name, rc);
		goto error_ctrl_deinit;
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_LINK_CLK, DSI_CLK_ON);
	if (rc) {
		DSI_ERR("[%s] failed to enable DSI link clocks, rc=%d\n",
		       display->name, rc);
		goto error_host_engine_off;
	}

	if (!is_skip_op_required(display)) {
		/*
		 * For continuous splash/trusted vm, skip panel prepare and
		 * ctl reset since the pnael and ctrl is already in active
		 * state and panel on commands are not needed
		 */
		rc = dsi_display_soft_reset(display);
		if (rc) {
			DSI_ERR("[%s] failed soft reset, rc=%d\n",
					display->name, rc);
			goto error_ctrl_link_off;
		}

		if (!display->poms_pending) {
			rc = dsi_panel_prepare(display->panel);
			if (rc) {
				DSI_ERR("[%s] panel prepare failed, rc=%d\n",
						display->name, rc);
				goto error_ctrl_link_off;
			}
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
	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT);
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
		DSI_ERR("request for %d rois greater than max %d\n",
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

	DSI_DEBUG("ctrl%d:%d: req (%d,%d,%d,%d) bnd (%d,%d,%d,%d) out (%d,%d,%d,%d)\n",
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

	mutex_lock(&display->display_lock);

	display_for_each_ctrl(i, display) {
		if (enable) {
			/* send the commands to enable qsync */
			rc = dsi_panel_send_qsync_on_dcs(display->panel, i);
			if (rc) {
				DSI_ERR("fail qsync ON cmds rc:%d\n", rc);
				goto exit;
			}
		} else {
			/* send the commands to enable qsync */
			rc = dsi_panel_send_qsync_off_dcs(display->panel, i);
			if (rc) {
				DSI_ERR("fail qsync OFF cmds rc:%d\n", rc);
				goto exit;
			}
		}

		dsi_ctrl_setup_avr(display->ctrl[i].ctrl, enable);
	}

exit:
	SDE_EVT32(enable, display->panel->qsync_caps.qsync_min_fps, rc);
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

	display_for_each_ctrl(i, display) {
		struct dsi_display_ctrl *ctrl = &display->ctrl[i];
		struct dsi_rect ctrl_roi;
		bool changed = false;

		rc = dsi_display_calc_ctrl_roi(display, ctrl, rois, &ctrl_roi);
		if (rc) {
			DSI_ERR("dsi_display_calc_ctrl_roi failed rc %d\n", rc);
			return rc;
		}

		rc = dsi_ctrl_set_roi(ctrl->ctrl, &ctrl_roi, &changed);
		if (rc) {
			DSI_ERR("dsi_ctrl_set_roi failed rc %d\n", rc);
			return rc;
		}

		if (!changed)
			continue;

		/* re-program the ctrl with the timing based on the new roi */
		rc = dsi_ctrl_timing_setup(ctrl->ctrl);
		if (rc) {
			DSI_ERR("dsi_ctrl_setup failed rc %d\n", rc);
			return rc;
		}

		/* send the new roi to the panel via dcs commands */
		rc = dsi_panel_send_roi_dcs(display->panel, i, &ctrl_roi);
		if (rc) {
			DSI_ERR("dsi_panel_set_roi failed rc %d\n", rc);
			return rc;
		}
	}

	return rc;
}

int dsi_display_pre_kickoff(struct drm_connector *connector,
		struct dsi_display *display,
		struct msm_display_kickoff_params *params,
		bool force_update_dsi_clocks)
{
	int rc = 0, ret = 0;
	int i;

	/* check and setup MISR */
	if (display->misr_enable)
		_dsi_display_setup_misr(display);

	/* dynamic DSI clock setting */
	if (atomic_read(&display->clkrate_change_pending) && force_update_dsi_clocks) {
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
		display_for_each_ctrl(i, display) {
			struct dsi_ctrl *ctrl = display->ctrl[i].ctrl;

			ret = dsi_ctrl_wait_for_cmd_mode_mdp_idle(ctrl);
			if (ret)
				goto wait_failure;
		}

		/*
		 * Don't check the return value so as not to impact DRM commit
		 * when error occurs.
		 */
		SDE_EVT32(SDE_EVTLOG_FUNC_CASE1);
		(void)dsi_display_force_update_dsi_clk(display);
wait_failure:
		/* release panel_lock */
		dsi_panel_release_panel_lock(display->panel);
		mutex_unlock(&display->display_lock);
	}

	if (!ret)
		rc = dsi_display_set_roi(display, params->rois);

	return rc;
}

int dsi_display_config_ctrl_for_cont_splash(struct dsi_display *display)
{
	int rc = 0;

	if (!display || !display->panel) {
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (!display->panel->cur_mode) {
		DSI_ERR("no valid mode set for the display\n");
		return -EINVAL;
	}

	if (display->config.panel_mode == DSI_OP_VIDEO_MODE) {
		rc = dsi_display_vid_engine_enable(display);
		if (rc) {
			DSI_ERR("[%s]failed to enable DSI video engine, rc=%d\n",
			       display->name, rc);
			goto error_out;
		}
	} else if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_cmd_engine_enable(display);
		if (rc) {
			DSI_ERR("[%s]failed to enable DSI cmd engine, rc=%d\n",
			       display->name, rc);
			goto error_out;
		}
	} else {
		DSI_ERR("[%s] Invalid configuration\n", display->name);
		rc = -EINVAL;
	}

error_out:
	return rc;
}

int dsi_display_pre_commit(void *display,
		struct msm_display_conn_params *params)
{
	bool enable = false;
	int rc = 0;

	if (!display || !params) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (params->qsync_update) {
		enable = (params->qsync_mode > 0) ? true : false;
		rc = dsi_display_qsync(display, enable);
		if (rc)
			pr_err("%s failed to send qsync commands\n",
				__func__);
		SDE_EVT32(params->qsync_mode, rc);
	}

	return rc;
}

static void dsi_display_panel_id_notification(struct dsi_display *display)
{
	if (display->panel_id != ~0x0 &&
		display->ctrl[0].ctrl->panel_id_cb.event_cb) {
		display->ctrl[0].ctrl->panel_id_cb.event_cb(
			display->ctrl[0].ctrl->panel_id_cb.event_usr_ptr,
			display->ctrl[0].ctrl->panel_id_cb.event_idx,
			0, ((display->panel_id & 0xffffffff00000000) >> 32),
			(display->panel_id & 0xffffffff), 0, 0);
	}
}

int dsi_display_enable(struct dsi_display *display)
{
	int rc = 0;
	struct dsi_display_mode *mode;
	char trace_buf[64];

	if (!display || !display->panel) {
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (!display->panel->cur_mode) {
		DSI_ERR("no valid mode set for the display\n");
		return -EINVAL;
	}
	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY);

	/*
	 * Engine states and panel states are populated during splash
	 * resource/trusted vm and hence we return early
	 */
	if (is_skip_op_required(display)) {

		dsi_display_config_ctrl_for_cont_splash(display);

		rc = dsi_display_splash_res_cleanup(display);
		if (rc) {
			DSI_ERR("Continuous splash res cleanup failed, rc=%d\n",
				rc);
			return -EINVAL;
		}

		display->panel->panel_initialized = true;
		DSI_DEBUG("cont splash enabled, display enable not required\n");
		dsi_display_panel_id_notification(display);

		rc = mi_dsi_panel_read_and_update_dc_param(display->panel);
		if (rc) {
			DSI_ERR("[%s] failed to read DC param, rc=%d\n",
				display->name, rc);
		}

		rc = mi_dsi_panel_read_and_update_flat_param(display->panel);
		if (rc) {
			DSI_ERR("[%s] failed to read flat param, rc=%d\n",
				display->name, rc);
		}

		if (mi_get_panel_id(display->panel->mi_cfg.mi_panel_id) == L3_PANEL_PA) {
			if (!display->panel->mi_cfg.uefi_read_lhbm_success) {
				DSI_INFO("uefi_read_lhbm_success is false, read in kernel.\n");
				mi_dsi_panel_read_lhbm_white_param(display->panel);
			}
		}

		return 0;
	}

	mutex_lock(&display->display_lock);

	mode = display->panel->cur_mode;

	if (mode->dsi_mode_flags & DSI_MODE_FLAG_DMS) {
		snprintf(trace_buf, sizeof(trace_buf), "dsi_panel_post_switch:%dx%dx%d:%d",
			mode->timing.h_active, mode->timing.v_active,
			mode->timing.refresh_rate, mode->timing.h_skew);
		SDE_ATRACE_BEGIN(trace_buf);
		rc = dsi_panel_post_switch(display->panel);
		SDE_ATRACE_END(trace_buf);
		if (rc) {
			DSI_ERR("[%s] failed to switch DSI panel mode, rc=%d\n",
				   display->name, rc);
			goto error;
		}
	} else if (!display->poms_pending) {
		rc = dsi_panel_enable(display->panel);
		if (rc) {
			DSI_ERR("[%s] failed to enable DSI panel, rc=%d\n",
			       display->name, rc);
			goto error;
		}
	}
	dsi_display_panel_id_notification(display);
	/* Block sending pps command if modeset is due to fps difference */
	if ((mode->priv_info->dsc_enabled ||
			mode->priv_info->vdc_enabled) &&
		!(mode->dsi_mode_flags & DSI_MODE_FLAG_DMS_FPS)) {
		rc = dsi_panel_update_pps(display->panel);
		if (rc) {
			DSI_ERR("[%s] panel pps cmd update failed, rc=%d\n",
				display->name, rc);
			goto error;
		}
	}

	if (mode->dsi_mode_flags & DSI_MODE_FLAG_DMS) {
		snprintf(trace_buf, sizeof(trace_buf), "dsi_panel_switch:%dx%dx%d:%d",
			mode->timing.h_active, mode->timing.v_active,
			mode->timing.refresh_rate, mode->timing.h_skew);
		SDE_ATRACE_BEGIN(trace_buf);
		rc = dsi_panel_switch(display->panel);
		SDE_ATRACE_END(trace_buf);
		if (rc)
			DSI_ERR("[%s] failed to switch DSI panel mode, rc=%d\n",
				   display->name, rc);

		goto error;
	}

	if (display->config.panel_mode == DSI_OP_VIDEO_MODE) {
		DSI_DEBUG("%s:enable video timing eng\n", __func__);
		rc = dsi_display_vid_engine_enable(display);
		if (rc) {
			DSI_ERR("[%s]failed to enable DSI video engine, rc=%d\n",
			       display->name, rc);
			goto error_disable_panel;
		}
	} else if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		DSI_DEBUG("%s:enable command timing eng\n", __func__);
		rc = dsi_display_cmd_engine_enable(display);
		if (rc) {
			DSI_ERR("[%s]failed to enable DSI cmd engine, rc=%d\n",
			       display->name, rc);
			goto error_disable_panel;
		}
	} else {
		DSI_ERR("[%s] Invalid configuration\n", display->name);
		rc = -EINVAL;
		goto error_disable_panel;
	}

	goto error;

error_disable_panel:
	(void)dsi_panel_disable(display->panel);
error:
	mutex_unlock(&display->display_lock);
	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT);
	return rc;
}

int dsi_display_post_enable(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	if (display->panel->cur_mode->dsi_mode_flags &
			DSI_MODE_FLAG_POMS_TO_CMD) {
		dsi_panel_switch_cmd_mode_in(display->panel);
	} else if (display->panel->cur_mode->dsi_mode_flags &
			DSI_MODE_FLAG_POMS_TO_VID)
		dsi_panel_switch_video_mode_in(display->panel);
	else {
		rc = dsi_panel_post_enable(display->panel);
		if (rc)
			DSI_ERR("[%s] panel post-enable failed, rc=%d\n",
				display->name, rc);
	}

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
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE)
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);
	if (display->poms_pending) {
		if (display->config.panel_mode == DSI_OP_CMD_MODE)
			dsi_panel_switch_cmd_mode_out(display->panel);

		if (display->config.panel_mode == DSI_OP_VIDEO_MODE)
			dsi_panel_switch_video_mode_out(display->panel);
	} else {
		rc = dsi_panel_pre_disable(display->panel);
		if (rc)
			DSI_ERR("[%s] panel pre-disable failed, rc=%d\n",
				display->name, rc);
	}

	mutex_unlock(&display->display_lock);
	return rc;
}

static void dsi_display_handle_poms_te(struct work_struct *work)
{
	struct dsi_display *display = NULL;
	struct delayed_work *dw = to_delayed_work(work);
	struct mipi_dsi_device *dsi = NULL;
	struct dsi_panel *panel = NULL;
	int rc = 0;

	display = container_of(dw, struct dsi_display, poms_te_work);
	if (!display || !display->panel) {
		DSI_ERR("Invalid params\n");
		return;
	}

	panel = display->panel;
	mutex_lock(&panel->panel_lock);
	if (!dsi_panel_initialized(panel)) {
		rc = -EINVAL;
		goto error;
	}

	dsi = &panel->mipi_device;
	rc = mipi_dsi_dcs_set_tear_off(dsi);

error:
	mutex_unlock(&panel->panel_lock);
	if (rc < 0)
		DSI_ERR("failed to set tear off\n");
}

int dsi_display_disable(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY);
	mutex_lock(&display->display_lock);

	/* cancel delayed work */
	if (display->poms_pending &&
			display->panel->poms_align_vsync)
		cancel_delayed_work_sync(&display->poms_te_work);

	rc = dsi_display_wake_up(display);
	if (rc)
		DSI_ERR("[%s] display wake up failed, rc=%d\n",
		       display->name, rc);

	if (display->config.panel_mode == DSI_OP_VIDEO_MODE) {
		rc = dsi_display_vid_engine_disable(display);
		if (rc)
			DSI_ERR("[%s]failed to disable DSI vid engine, rc=%d\n",
			       display->name, rc);
	} else if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		/**
		 * On POMS request , disable panel TE through
		 * delayed work queue.
		 */
		if (display->poms_pending &&
				display->panel->poms_align_vsync) {
			INIT_DELAYED_WORK(&display->poms_te_work,
					dsi_display_handle_poms_te);
			queue_delayed_work(system_wq,
					&display->poms_te_work,
					msecs_to_jiffies(100));
		}
		rc = dsi_display_cmd_engine_disable(display);
		if (rc)
			DSI_ERR("[%s]failed to disable DSI cmd engine, rc=%d\n",
			       display->name, rc);
	} else {
		DSI_ERR("[%s] Invalid configuration\n", display->name);
		rc = -EINVAL;
	}

	if (!display->poms_pending && !is_skip_op_required(display)) {
		rc = dsi_panel_disable(display->panel);
		if (rc)
			DSI_ERR("[%s] failed to disable DSI panel, rc=%d\n",
				display->name, rc);
	}

	if (is_skip_op_required(display)) {
		/* applicable only for trusted vm */
		display->panel->panel_initialized = false;
		display->panel->power_mode = SDE_MODE_DPMS_OFF;
	}
	mutex_unlock(&display->display_lock);
	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT);
	return rc;
}

int dsi_display_update_pps(char *pps_cmd, void *disp)
{
	struct dsi_display *display;

	if (pps_cmd == NULL || disp == NULL) {
		DSI_ERR("Invalid parameter\n");
		return -EINVAL;
	}

	display = disp;
	mutex_lock(&display->display_lock);
	memcpy(display->panel->dce_pps_cmd, pps_cmd, DSI_CMD_PPS_SIZE);
	mutex_unlock(&display->display_lock);

	return 0;
}

int dsi_display_update_dyn_bit_clk(struct dsi_display *display,
			struct dsi_display_mode *mode)
{
	struct dsi_dyn_clk_caps *dyn_clk_caps;
	struct dsi_host_common_cfg *host_cfg;
	int bpp, lanes = 0;

	if (!display || !mode) {
		DSI_ERR("invalid arguments\n");
		return -EINVAL;
	}

	dyn_clk_caps = &(display->panel->dyn_clk_caps);
	if (!dyn_clk_caps->dyn_clk_support) {
		DSI_DEBUG("dynamic bit clock support not enabled\n");
		return 0;
	} else if (!display->dyn_bit_clk_pending) {
		DSI_DEBUG("dynamic bit clock rate not updated\n");
		return 0;
	} else if (!display->dyn_bit_clk) {
		DSI_DEBUG("dynamic bit clock rate cleared\n");
		return 0;
	} else if (display->dyn_bit_clk < mode->priv_info->min_dsi_clk_hz) {
		DSI_ERR("dynamic bit clock rate %llu smaller than minimum value:%llu\n",
				display->dyn_bit_clk, mode->priv_info->min_dsi_clk_hz);
		return -EINVAL;
	}

	/* update mode clk rate with user value */
	mode->timing.clk_rate_hz = display->dyn_bit_clk;
	mode->priv_info->clk_rate_hz = display->dyn_bit_clk;

	host_cfg = &(display->panel->host_config);
	bpp = dsi_pixel_format_to_bpp(host_cfg->dst_format);

	if (host_cfg->data_lanes & DSI_DATA_LANE_0)
		lanes++;
	if (host_cfg->data_lanes & DSI_DATA_LANE_1)
		lanes++;
	if (host_cfg->data_lanes & DSI_DATA_LANE_2)
		lanes++;
	if (host_cfg->data_lanes & DSI_DATA_LANE_3)
		lanes++;

	dsi_display_adjust_mode_timing(display, mode, lanes, bpp);

	SDE_EVT32(display->dyn_bit_clk, mode->priv_info->min_dsi_clk_hz, mode->pixel_clk_khz);
	DSI_DEBUG("dynamic bit clk:%u, min dsi clk:%llu, lanes:%d, bpp:%d, pck:%d Khz\n",
			display->dyn_bit_clk, mode->priv_info->min_dsi_clk_hz, lanes, bpp,
			mode->pixel_clk_khz);

	return 0;
}

int dsi_display_dump_clks_state(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		DSI_ERR("invalid display argument\n");
		return -EINVAL;
	}

	if (!display->clk_mngr) {
		DSI_ERR("invalid clk manager\n");
		return -EINVAL;
	}

	if (!display->dsi_clk_handle || !display->mdp_clk_handle) {
		DSI_ERR("invalid clk handles\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	rc = dsi_display_dump_clk_handle_state(display->dsi_clk_handle);
	if (rc) {
		DSI_ERR("failed to dump dsi clock state\n");
		goto end;
	}

	rc = dsi_display_dump_clk_handle_state(display->mdp_clk_handle);
	if (rc) {
		DSI_ERR("failed to dump mdp clock state\n");
		goto end;
	}

end:
	mutex_unlock(&display->display_lock);

	return rc;
}

int dsi_display_unprepare(struct dsi_display *display)
{
	int rc = 0;

	if (!display) {
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY);
	mutex_lock(&display->display_lock);

	rc = dsi_display_wake_up(display);
	if (rc)
		DSI_ERR("[%s] display wake up failed, rc=%d\n",
		       display->name, rc);
	if (!display->poms_pending && !is_skip_op_required(display)) {
		rc = dsi_panel_unprepare(display->panel);
		if (rc)
			DSI_ERR("[%s] panel unprepare failed, rc=%d\n",
			       display->name, rc);
	}

	rc = dsi_display_ctrl_host_disable(display);
	if (rc)
		DSI_ERR("[%s] failed to disable DSI host, rc=%d\n",
		       display->name, rc);

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_LINK_CLK, DSI_CLK_OFF);
	if (rc)
		DSI_ERR("[%s] failed to disable Link clocks, rc=%d\n",
		       display->name, rc);

	rc = dsi_display_ctrl_deinit(display);
	if (rc)
		DSI_ERR("[%s] failed to deinit controller, rc=%d\n",
		       display->name, rc);

	if (!display->panel->ulps_suspend_enabled) {
		rc = dsi_display_phy_disable(display);
		if (rc)
			DSI_ERR("[%s] failed to disable DSI PHY, rc=%d\n",
			       display->name, rc);
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);
	if (rc)
		DSI_ERR("[%s] failed to disable DSI clocks, rc=%d\n",
		       display->name, rc);

	/* destrory dsi isr set up */
	dsi_display_ctrl_isr_configure(display, false);

	if (!display->poms_pending && !is_skip_op_required(display)) {
		rc = dsi_panel_post_unprepare(display->panel);
		if (rc)
			DSI_ERR("[%s] panel post-unprepare failed, rc=%d\n",
			       display->name, rc);
	}
	display->hw_ownership = false;

	mutex_unlock(&display->display_lock);

	/* Free up DSI ERROR event callback */
	dsi_display_unregister_error_handler(display);

	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT);
	return rc;
}

void __init dsi_display_register(void)
{
	mi_disp_feature_init();
	dsi_phy_drv_register();
	dsi_ctrl_drv_register();

	dsi_display_parse_boot_display_selection();

	platform_driver_register(&dsi_display_driver);
}

void __exit dsi_display_unregister(void)
{
	platform_driver_unregister(&dsi_display_driver);
	dsi_ctrl_drv_unregister();
	dsi_phy_drv_unregister();
	mi_disp_feature_deinit();
}
module_param_string(dsi_display0, dsi_display_primary, MAX_CMDLINE_PARAM_LEN,
								0600);
MODULE_PARM_DESC(dsi_display0,
	"msm_drm.dsi_display0=<display node>:<configX> where <display node> is 'primary dsi display node name' and <configX> where x represents index in the topology list");
module_param_string(dsi_display1, dsi_display_secondary, MAX_CMDLINE_PARAM_LEN,
								0600);
MODULE_PARM_DESC(dsi_display1,
	"msm_drm.dsi_display1=<display node>:<configX> where <display node> is 'secondary dsi display node name' and <configX> where x represents index in the topology list");

/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#define pr_fmt(fmt)	"mi-dsi-display:[%s] " fmt, __func__

#include <linux/wait.h>
#include <linux/backlight.h>

#include <drm/drm_device.h>

#include "msm_kms.h"
#include "sde_connector.h"
#include "dsi_display.h"
#include "dsi_panel.h"
#include "mi_sde_encoder.h"
#include "mi_disp_print.h"
#include "mi_dsi_display.h"
#include "mi_dsi_panel.h"
#include "mi_disp_feature.h"

static char oled_wp_info_str[32] = {0};
static char sec_oled_wp_info_str[32] = {0};
static char cell_id_info_str[32] = {0};
static bool wp_info_cmdline_flag = 1;

char *get_display_power_mode_name(int power_mode)
{
	switch (power_mode) {
	case SDE_MODE_DPMS_ON:
		return "On";
	case SDE_MODE_DPMS_LP1:
		return "Doze";
	case SDE_MODE_DPMS_LP2:
		return "Doze_Suspend";
	case SDE_MODE_DPMS_STANDBY:
		return "Standby";
	case SDE_MODE_DPMS_SUSPEND:
		return "Suspend";
	case SDE_MODE_DPMS_OFF:
		return "Off";
	default:
		return "Unknown";
	}
}

int mi_get_disp_id(struct dsi_display *display)
{
	if (!strncmp(display->display_type, "primary", 7))
		return MI_DISP_PRIMARY;
	else
		return MI_DISP_SECONDARY;
}

struct dsi_display * mi_get_primary_dsi_display(void)
{
	struct disp_feature *df = mi_get_disp_feature();
	struct disp_display *dd_ptr = NULL;
	struct dsi_display *dsi_display = NULL;

	if (df) {
		dd_ptr = &df->d_display[MI_DISP_PRIMARY];
		if (dd_ptr->display && dd_ptr->intf_type == MI_INTF_DSI) {
			dsi_display = (struct dsi_display *)dd_ptr->display;
			return dsi_display;
		} else {
			return NULL;
		}
	} else {
		return NULL;
	}
}

struct dsi_display * mi_get_secondary_dsi_display(void)
{
	struct disp_feature *df = mi_get_disp_feature();
	struct disp_display *dd_ptr = NULL;
	struct dsi_display *dsi_display = NULL;

	if (df) {
		dd_ptr = &df->d_display[MI_DISP_SECONDARY];
		if (dd_ptr->display && dd_ptr->intf_type == MI_INTF_DSI) {
			dsi_display = (struct dsi_display *)dd_ptr->display;
			return dsi_display;
		} else {
			return NULL;
		}
	} else {
		return NULL;
	}
}

int mi_dsi_display_set_disp_param(void *display,
			struct disp_feature_ctl *ctl)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;

	if (!dsi_display || !ctl) {
		DISP_ERROR("Invalid display or ctl ptr\n");
		return -EINVAL;
	}

	if (sde_kms_is_suspend_blocked(dsi_display->drm_dev) &&
		mi_dsi_panel_is_need_tx_cmd(ctl->feature_id)) {
		DISP_ERROR("sde_kms is suspended, skip to set_disp_param\n");
		return -EBUSY;
	}

	mi_dsi_acquire_wakelock(dsi_display->panel);
	ret = mi_dsi_panel_set_disp_param(dsi_display->panel, ctl);
	mi_dsi_release_wakelock(dsi_display->panel);

	return ret;
}

ssize_t mi_dsi_display_get_disp_param(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}
	return mi_dsi_panel_get_disp_param(dsi_display->panel, buf, size);
}

int mi_dsi_display_write_mipi_reg(void *display,
			char *buf)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (sde_kms_is_suspend_blocked(dsi_display->drm_dev)) {
		DISP_ERROR("sde_kms is suspended, skip to write mipi register\n");
		return -EBUSY;
	}

	mi_dsi_acquire_wakelock(dsi_display->panel);
	ret = mi_dsi_panel_write_mipi_reg(dsi_display->panel, buf);
	mi_dsi_release_wakelock(dsi_display->panel);

	return ret;
}

ssize_t mi_dsi_display_read_mipi_reg(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (sde_kms_is_suspend_blocked(dsi_display->drm_dev)) {
		DISP_ERROR("sde_kms is suspended, skip to read mipi register\n");
		return -EBUSY;
	}

	mi_dsi_acquire_wakelock(dsi_display->panel);
	ret = mi_dsi_panel_read_mipi_reg(dsi_display->panel, buf, size);
	mi_dsi_release_wakelock(dsi_display->panel);

	return ret;
}

ssize_t mi_dsi_display_read_panel_info(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	char *pname = NULL;
	ssize_t ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	pname = mi_dsi_display_get_cmdline_panel_info(dsi_display);
	if (pname) {
		ret = snprintf(buf, size, "panel_name=%s\n", pname);
		kfree(pname);
	} else {
		if (dsi_display->name) {
			/* find the last occurrence of a character in a string */
			pname = strrchr(dsi_display->name, ',');
			if (pname && *pname)
				ret = snprintf(buf, size, "panel_name=%s\n", ++pname);
			else
				ret = snprintf(buf, size, "panel_name=%s\n", dsi_display->name);
		} else {
			ret = snprintf(buf, size, "panel_name=%s\n", "null");
		}
	}

	return ret;
}

ssize_t mi_dsi_display_read_wp_info(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int display_id = 0;
	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (wp_info_cmdline_flag) {
		display_id = mi_get_disp_id(dsi_display);
		if (display_id == MI_DISP_PRIMARY)
			return snprintf(buf, size, "%s\n", oled_wp_info_str);
		else
			return snprintf(buf, size, "%s\n", sec_oled_wp_info_str);
	} else {
		return snprintf(buf, size, "%s\n", "null");
	}
}

int mi_dsi_display_get_fps(void *display, u32 *fps)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	struct dsi_display_mode *cur_mode = NULL;
	int ret = 0;

	if (!dsi_display || !dsi_display->panel) {
		DISP_ERROR("Invalid display/panel ptr\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_display->display_lock);
	cur_mode = dsi_display->panel->cur_mode;
	if (cur_mode) {
		*fps =  cur_mode->timing.refresh_rate;
	} else {
		ret = -EINVAL;
	}
	mutex_unlock(&dsi_display->display_lock);

	return ret;
}

int mi_dsi_display_set_doze_brightness(void *display,
			u32 doze_brightness)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	struct disp_event event;
	struct sde_connector *c_conn = NULL;
	int ret = 0;

	if (!dsi_display || !dsi_display->drm_conn) {
		DISP_ERROR("Invalid display ptr or Invalid drm_conn ptr\n");
		return -EINVAL;
	}

	if (sde_kms_is_suspend_blocked(dsi_display->drm_dev)) {
		DISP_ERROR("sde_kms is suspended, skip to set doze brightness\n");
		return -EBUSY;
	}

	c_conn = to_sde_connector(dsi_display->drm_conn);

	mutex_lock(&dsi_display->display_lock);
	mi_dsi_acquire_wakelock(dsi_display->panel);
	ret = mi_dsi_panel_set_doze_brightness(dsi_display->panel,
				doze_brightness);
	mi_dsi_release_wakelock(dsi_display->panel);
	mutex_unlock(&dsi_display->display_lock);

	event.disp_id = mi_get_disp_id(dsi_display);
	event.type = MI_DISP_EVENT_DOZE;
	event.length = sizeof(doze_brightness);
	mi_disp_feature_event_notify(&event, (u8 *)&doze_brightness);

	return ret;
}

int mi_dsi_display_get_doze_brightness(void *display,
			u32 *doze_brightness)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_doze_brightness(dsi_display->panel,
				doze_brightness);
}

int mi_dsi_display_get_brightness(void *display,
			u32 *brightness)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_brightness(dsi_display->panel,
				brightness);
}

int mi_dsi_display_write_dsi_cmd(void *display,
			struct dsi_cmd_rw_ctl *ctl)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (sde_kms_is_suspend_blocked(dsi_display->drm_dev)) {
		DISP_ERROR("sde_kms is suspended, skip to write dsi cmd\n");
		return -EBUSY;
	}

	mi_dsi_acquire_wakelock(dsi_display->panel);
	ret = mi_dsi_panel_write_dsi_cmd(dsi_display->panel, ctl);
	mi_dsi_release_wakelock(dsi_display->panel);

	return ret;
}

int mi_dsi_display_read_dsi_cmd(void *display,
			struct dsi_cmd_rw_ctl *ctl)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (sde_kms_is_suspend_blocked(dsi_display->drm_dev)) {
		DISP_ERROR("sde_kms is suspended, skip to read dsi cmd\n");
		return -EBUSY;
	}

	mi_dsi_acquire_wakelock(dsi_display->panel);
	ret = dsi_display_cmd_receive(dsi_display,
			ctl->tx_ptr, ctl->tx_len, ctl->rx_ptr, ctl->rx_len);
	mi_dsi_release_wakelock(dsi_display->panel);

	return ret;
}

int mi_dsi_display_write_dsi_cmd_set(void *display,
			int type)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (sde_kms_is_suspend_blocked(dsi_display->drm_dev)) {
		DISP_ERROR("sde_kms is suspended, skip to write dsi cmd_set\n");
		return -EBUSY;
	}

	mi_dsi_acquire_wakelock(dsi_display->panel);
	ret = mi_dsi_panel_write_dsi_cmd_set(dsi_display->panel, type);
	mi_dsi_release_wakelock(dsi_display->panel);

	return ret;
}

ssize_t mi_dsi_display_show_dsi_cmd_set_type(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_show_dsi_cmd_set_type(dsi_display->panel, buf, size);
}

int mi_dsi_display_set_brightness_clone(void *display,
			u32 brightness_clone)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;
	struct disp_event event;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	dsi_display->panel->mi_cfg.real_brightness_clone = brightness_clone;
	if (!dsi_display->panel->mi_cfg.thermal_dimming)
		if (brightness_clone > dsi_display->panel->mi_cfg.thermal_max_brightness_clone)
			brightness_clone = dsi_display->panel->mi_cfg.thermal_max_brightness_clone;

	ret = mi_dsi_panel_set_brightness_clone(dsi_display->panel,
				brightness_clone);

	event.disp_id = mi_get_disp_id(dsi_display);

	event.type = MI_DISP_EVENT_BRIGHTNESS_CLONE;
	event.length = sizeof(brightness_clone);
	mi_disp_feature_event_notify(&event, (u8 *)&brightness_clone);
	return ret;
}

int mi_dsi_display_get_brightness_clone(void *display,
			u32 *brightness_clone)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_brightness_clone(dsi_display->panel,
				brightness_clone);
}

ssize_t mi_dsi_display_get_hw_vsync_info(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	return mi_sde_encoder_calc_hw_vsync_info(dsi_display, buf, size);
}

int mi_dsi_display_esd_irq_ctrl(struct dsi_display *display,
			bool enable)
{
	int ret = 0;

	if (!display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	ret = mi_dsi_panel_esd_irq_ctrl(display->panel, enable);
	if (ret)
		DISP_ERROR("[%s] failed to set esd irq, rc=%d\n",
				display->name, ret);

	mutex_unlock(&display->display_lock);

	return ret;
}

void mi_dsi_display_wakeup_pending_doze_work(struct dsi_display *display)
{
	int disp_id = 0;
	struct disp_display *dd_ptr;
	struct disp_feature *df = mi_get_disp_feature();

	if (!display) {
		DISP_ERROR("Invalid display ptr\n");
		return;
	}

	disp_id = mi_get_disp_id(display);

	dd_ptr = &df->d_display[disp_id];
	DISP_INFO("%s pending_doze_cnt = %d\n",
			display->display_type, atomic_read(&dd_ptr->pending_doze_cnt));
	if (atomic_read(&dd_ptr->pending_doze_cnt)) {
		DISP_INFO("%s display wake up pending doze brightness work\n",
			display->display_type);
		wake_up_interruptible_all(&dd_ptr->pending_wq);
	}
}

bool mi_is_doze_full_brightness_supported(void *display)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display || !dsi_display->panel) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return dsi_display->panel->mi_cfg.aod_bl_51ctl;
}

void mi_dsi_display_update_backlight(struct dsi_display *display)
{
	struct drm_connector *connector = NULL;
	struct sde_connector *c_conn = NULL;

	if (!display) {
		DISP_INFO("invalid dsi_display ptr\n");
		return;
	}

	connector = display->drm_conn;
	c_conn = to_sde_connector(connector);
	if (!c_conn) {
		DISP_INFO("invalid sde_connector ptr\n");
		return;
	}

	backlight_update_status(c_conn->bl_device);
}

ssize_t mi_dsi_display_cell_id_read(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	DISP_INFO("read cell_id: %s \n",cell_id_info_str);
	return snprintf(buf, size, "%s\n", cell_id_info_str);
}


int mi_display_pm_suspend(void)
{
	struct dsi_display *dsi_display;
	struct drm_device *ddev;
	struct msm_drm_private *priv;
	struct msm_kms *kms;

	dsi_display = mi_get_primary_dsi_display();
	ddev = dsi_display->drm_dev;

	if (!ddev || !ddev->dev_private)
		return -EINVAL;
	priv = ddev->dev_private;
	kms = priv->kms;
	if (kms && kms->funcs && kms->funcs->pm_suspend)
		return kms->funcs->pm_suspend(ddev->dev);
	return 0;
}
EXPORT_SYMBOL(mi_display_pm_suspend);

module_param_string(oled_wp, oled_wp_info_str, MAX_CMDLINE_PARAM_LEN,
								0600);
MODULE_PARM_DESC(oled_wp, "msm_drm.oled_wp=<wp info> while <wp info> is 'white point info' ");

module_param_string(sec_oled_wp, sec_oled_wp_info_str, MAX_CMDLINE_PARAM_LEN,
								0600);
MODULE_PARM_DESC(sec_oled_wp, "msm_drm.sec_oled_wp=<wp info> while <wp info> is 'white point info' ");

module_param_string(cell_id, cell_id_info_str, MAX_CMDLINE_PARAM_LEN,
								0600);
MODULE_PARM_DESC(cell_id, "msm_drm.cell_id=<cell id> while <cell id> is 'white cell id' ");
